"""
API de haut niveau : poutre continue → analyse → résultats.

    >>> import Tsaraloha.LIPoutreContinue as tsa
    >>> beam = tsa.Beam()
    >>> beam.add_span(length=10.0, E=210e9, I=8e-4)
    >>> beam.add_span(length=12.0, E=210e9, I=1.2e-3)
    >>> beam.add_support(0.0, "pin")
    >>> beam.add_support(10.0, "roller")
    >>> beam.add_support(22.0, "roller")
    >>> truck = tsa.PointLoadTrain(axles=[60, 120, 120], spacings=[1.5, 4.5], name="Camion")
    >>> lane = tsa.DistributedLoad(q=9.0, length=10.0, name="Voie")
    >>> res = beam.analyze(step=0.25, loads=[truck, lane])
    >>> res.maximum_bending_moment           # Extremum(value=…, x=…, …) en kN·m
    >>> env = res.envelope("M")              # courbes max(x) / min(x)
    >>> res.plot("M"); res.report("note.html")

Ce module ne fait AUCUN calcul numérique : il valide les entrées, traduit le
modèle vers le moteur C++ (`Output`) et présente ses résultats (numpy).

Unités : E en Pa, I en m⁴, longueurs en m, charges en kN et kN/m.
Résultats : M en kN·m, V et R en kN, w en m, θ en rad.
Conventions : M > 0 = fibre inférieure tendue ; w > 0 vers le haut (une
charge descendante donne w < 0) ; R > 0 vers le haut ; charges vers le bas.
"""
from __future__ import annotations

import json
import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Optional, Sequence, Union

import numpy as np

__all__ = ["Beam", "PointLoadTrain", "DistributedLoad", "Results", "Envelope", "Extremum"]

_SUPPORT_TYPES = ("pin", "roller", "fixed")
_TOL = 1e-9

# Grandeur → (attribut tenseur Output, clé des dicts d'enveloppe, unité, libellé)
_QUANTITIES = {
    "M": ("BM", "bending_moment", "kN·m", "Moment fléchissant"),
    "V": ("SF", "shear_force", "kN", "Effort tranchant"),
    "w": ("Def", "deflection", "m", "Flèche"),
    "theta": ("Rot", "rotation", "rad", "Rotation"),
}
_ALIASES = {"m": "M", "bm": "M", "moment": "M", "bending_moment": "M",
            "v": "V", "sf": "V", "shear": "V", "shear_force": "V",
            "w": "w", "def": "w", "deflection": "w",
            "theta": "theta", "rot": "theta", "rotation": "theta", "θ": "theta"}
_KINDS = {"point": "point_load", "distributed": "rectangular_load", "combined": "combined_load"}


def _quantity(q: str) -> str:
    key = _ALIASES.get(str(q).lower(), _ALIASES.get(str(q)))
    if key is None:
        raise ValueError(f"Grandeur inconnue {q!r}. Valeurs possibles : 'M', 'V', 'w', 'theta'.")
    return key


def _positive(name: str, value) -> float:
    try:
        v = float(value)
    except (TypeError, ValueError):
        raise TypeError(f"'{name}' doit être un nombre, reçu {value!r}.") from None
    if not (math.isfinite(v) and v > 0):
        raise ValueError(f"'{name}' doit être un réel strictement positif, reçu {value!r}.")
    return v


# =============================================================================
#  Charges
# =============================================================================
@dataclass(frozen=True)
class PointLoadTrain:
    """Convoi de charges ponctuelles se déplaçant sur toute la poutre.

    Parameters
    ----------
    axles : forces des essieux [kN], dans l'ordre (≥ 1 essieu).
    spacings : distances entre essieux consécutifs [m], ``len(axles) - 1`` valeurs.
    name : étiquette reprise dans les résultats.

    Toutes les positions sont explorées, y compris l'entrée et la sortie
    partielles du convoi. Le sens de circulation est celui des essieux donnés
    (ajouter le convoi retourné comme seconde charge si les deux sens sont
    possibles).
    """
    axles: Sequence[float]
    spacings: Sequence[float] = ()
    name: str = "Convoi"

    def __post_init__(self):
        axles = tuple(float(a) for a in self.axles)
        spacings = tuple(float(s) for s in self.spacings)
        if not axles:
            raise ValueError("PointLoadTrain : au moins un essieu est requis.")
        if len(spacings) != len(axles) - 1:
            raise ValueError(f"PointLoadTrain : {len(axles)} essieu(x) → {len(axles) - 1} entraxe(s) attendu(s), "
                             f"reçu {len(spacings)}. Exemple : axles=[60, 120], spacings=[1.5].")
        if any(s < 0 or not math.isfinite(s) for s in spacings):
            raise ValueError("PointLoadTrain : les entraxes doivent être positifs ou nuls.")
        object.__setattr__(self, "axles", axles)
        object.__setattr__(self, "spacings", spacings)

    def reversed(self) -> "PointLoadTrain":
        """Le même convoi circulant dans l'autre sens."""
        return PointLoadTrain(self.axles[::-1], self.spacings[::-1], name=f"{self.name} (retourné)")

    def _engine(self, core):
        return core.Load(intensity=list(self.axles), length=[0.0, *self.spacings], name=self.name)


@dataclass(frozen=True)
class DistributedLoad:
    """Charge répartie mobile, uniforme (``q`` scalaire) ou par tronçons.

    Parameters
    ----------
    q : intensité [kN/m] — un nombre, ou une liste (une valeur par tronçon).
    length : longueur [m] — un nombre, ou une liste de même taille que ``q``.
        ``None`` (uniforme seulement) : longueur = longueur totale de la poutre.
    name : étiquette reprise dans les résultats.

    La position la plus défavorable du bloc rigide est recherchée exactement
    (y compris partiellement hors poutre). Le chargement « zones défavorables
    seulement » (longueur variable) n'est pas encore proposé.
    """
    q: Union[float, Sequence[float]]
    length: Union[None, float, Sequence[float]] = None
    name: str = "Charge répartie"

    def _segments(self, total_length: float):
        qs = [float(self.q)] if np.isscalar(self.q) else [float(v) for v in self.q]
        if self.length is None:
            if len(qs) != 1:
                raise ValueError("DistributedLoad : 'length' est obligatoire pour une charge par tronçons.")
            ls = [total_length]
        else:
            ls = [float(self.length)] if np.isscalar(self.length) else [float(v) for v in self.length]
        if len(ls) != len(qs):
            raise ValueError(f"DistributedLoad : {len(qs)} intensité(s) mais {len(ls)} longueur(s).")
        if any(not (math.isfinite(v) and v > 0) for v in ls):
            raise ValueError("DistributedLoad : les longueurs doivent être strictement positives.")
        return qs, ls

    def _engine(self, core, total_length: float):
        qs, ls = self._segments(total_length)
        return core.Load(intensity=qs, length=[0.0, *ls], name=self.name)


# =============================================================================
#  Résultats
# =============================================================================
@dataclass(frozen=True)
class Extremum:
    """Valeur extrême et sa localisation physique."""
    value: float
    x: float                 # abscisse globale de la section [m]
    span: int                # indice de travée (0 = première travée réelle)
    unit: str
    loads: dict = field(default_factory=dict)  # {nom: {"value", "position"}} — position = origine de la charge [m]

    def __float__(self) -> float:
        return float(self.value)


@dataclass(frozen=True)
class Envelope:
    """Courbes enveloppes d'une grandeur, alignées sur ``x``."""
    quantity: str
    kind: str
    x: np.ndarray
    max: np.ndarray
    min: np.ndarray
    unit: str
    max_extremum: Extremum
    min_extremum: Extremum

    @property
    def governing(self) -> Extremum:
        """Extrême de plus grand module."""
        return self.max_extremum if abs(self.max_extremum.value) >= abs(self.min_extremum.value) else self.min_extremum


def _extremum(block: dict, unit: str) -> Extremum:
    loads = {n: {"value": d.get("value"), "position": d.get("Position")} for n, d in block.get("load", {}).items()}
    return Extremum(value=float(block["value"]), x=float(block["section_x"]), span=int(block["span"]),
                    unit=unit, loads=loads)


class Results:
    """Résultats d'une analyse (lecture seule). Créés par :meth:`Beam.analyze`."""

    def __init__(self, beam: "Beam", output, step: float, has_loads: bool):
        self.beam = beam
        self.output = output          # objet moteur (accès avancé, API 1.x)
        self.step = step
        self._has_loads = has_loads
        self._x = np.asarray(output.X, dtype=float)

    # -- géométrie -----------------------------------------------------------
    @property
    def x(self) -> np.ndarray:
        """Abscisses globales des sections de calcul [m] (un appui intérieur
        apparaît deux fois : fin de la travée i et début de la travée i+1)."""
        return self._x.copy()

    @property
    def support_positions(self) -> np.ndarray:
        return np.asarray(self.output.node_lengths, dtype=float)

    # -- lignes d'influence -------------------------------------------------
    def influence_line(self, quantity: str, x: float):
        """Ligne d'influence de ``quantity`` à la section la plus proche de ``x``.

        Retourne ``(load_x, values, section_x)`` : effet par kN de charge
        (kN·m/kN, kN/kN, m/kN, rad/kN) pour une charge unité en ``load_x``.
        """
        q = _quantity(quantity)
        tensor = getattr(self.output, _QUANTITIES[q][0])
        offsets = np.cumsum([0] + [len(s) for s in self.output.span_node_positions])
        k = int(np.argmin(np.abs(self._x - float(x))))
        span = int(np.searchsorted(offsets, k, side="right") - 1)
        sec = k - offsets[span]
        values = np.asarray(tensor[span][sec], dtype=float)
        load_x = np.asarray(self.output.shear_force_all_abscissa[span][sec]) if q == "V" else self._x
        if q in ("w", "theta"):
            values = values * 1e3       # moteur : par N → par kN
        return load_x.copy(), values, float(self._x[k])

    def reaction_influence_line(self, support: int):
        """Ligne d'influence de la réaction de l'appui ``support`` (kN/kN)."""
        return self._x.copy(), np.asarray(self.output.support_reaction[support], dtype=float)

    # -- enveloppes ----------------------------------------------------------
    def _require_loads(self):
        if not self._has_loads:
            raise RuntimeError("Aucune charge mobile : passez loads=[...] à Beam.analyze() "
                               "pour obtenir des enveloppes.")

    def envelope(self, quantity: str, kind: str = "combined") -> Envelope:
        """Enveloppes max(x) / min(x). ``kind`` : 'combined' (défaut), 'point', 'distributed'."""
        self._require_loads()
        q = _quantity(quantity)
        if kind not in _KINDS:
            raise ValueError(f"kind doit être l'un de {list(_KINDS)}, reçu {kind!r}.")
        key, unit = _QUANTITIES[q][1], _QUANTITIES[q][2]
        curves = self.output.section_envelopes[key]
        general = getattr(self.output, f"{key}_general_envelope")[_KINDS[kind]]
        return Envelope(quantity=q, kind=kind, x=np.asarray(curves["x"], dtype=float),
                        max=np.asarray(curves[_KINDS[kind]]["max"], dtype=float),
                        min=np.asarray(curves[_KINDS[kind]]["min"], dtype=float),
                        unit=unit, max_extremum=_extremum(general["max"], unit),
                        min_extremum=_extremum(general["min"], unit))

    @property
    def maximum_bending_moment(self) -> Extremum:
        """Moment positif maximal (travée) sous charges mobiles combinées [kN·m]."""
        return self.envelope("M").max_extremum

    @property
    def minimum_bending_moment(self) -> Extremum:
        """Moment négatif minimal (appuis) sous charges mobiles combinées [kN·m]."""
        return self.envelope("M").min_extremum

    @property
    def maximum_shear_force(self) -> Extremum:
        return self.envelope("V").governing

    @property
    def maximum_deflection(self) -> Extremum:
        """Flèche de plus grand module [m] (négative = vers le bas)."""
        return self.envelope("w").governing

    def reactions(self, kind: str = "combined") -> list:
        """Réactions extrêmes par appui : liste de dicts {support, x, max, min} [kN]."""
        self._require_loads()
        out = []
        for env in self.output.support_reaction_envelopes:
            block = env[_KINDS[kind]]
            out.append({"support": env["support"], "x": env["x"],
                        "max": block["max"]["value"], "min": block["min"]["value"]})
        return out

    # -- exports -------------------------------------------------------------
    def to_dict(self) -> dict:
        """Résumé sérialisable : modèle, extrema, réactions (sans les tenseurs N²)."""
        d = {"model": self.beam.to_dict(), "step": self.step,
             "units": {"M": "kN·m", "V": "kN", "R": "kN", "w": "m", "theta": "rad"},
             "influence_line_extrema": self.output.influence_line_extrema}
        if self._has_loads:
            d["envelopes"] = {}
            for q in _QUANTITIES:
                e = self.envelope(q)
                d["envelopes"][q] = {"max": e.max_extremum.__dict__, "min": e.min_extremum.__dict__}
            d["reactions"] = self.reactions()
        return d

    def to_json(self, path: Union[str, Path]) -> Path:
        path = Path(path)
        def _clean(o):
            if isinstance(o, float) and not math.isfinite(o):
                return None
            if isinstance(o, dict):
                return {k: _clean(v) for k, v in o.items()}
            if isinstance(o, (list, tuple)):
                return [_clean(v) for v in o]
            return o
        path.write_text(json.dumps(_clean(self.to_dict()), indent=2, ensure_ascii=False), encoding="utf-8")
        return path

    # -- graphiques (voir le module plot) ------------------------------------
    def plot(self, quantity: str = "M", kind: str = "combined", ax=None):
        """Enveloppes annotées de ``quantity`` ('M', 'V', 'w', 'theta')."""
        from . import plot
        return plot.envelope(self, quantity, kind, ax)

    def plot_influence_line(self, quantity: str = "M", x: float = 0.0, ax=None):
        """Ligne d'influence de ``quantity`` à la section la plus proche de ``x``."""
        from . import plot
        return plot.influence_line(self, quantity, x, ax)

    def plot_critical(self, quantity: str = "M", which: str = "governing"):
        """Charges dessinées à leur position la plus défavorable + ligne d'influence."""
        from . import plot
        return plot.critical_configuration(self, quantity, which)

    def dashboard(self):
        """Figure de synthèse : poutre, enveloppes M/V/w, réactions, extrêmes."""
        from . import plot
        return plot.dashboard(self)

    def show(self):
        """Affiche le tableau de bord (fenêtre matplotlib)."""
        import matplotlib.pyplot as plt
        self.dashboard()
        plt.show()

    def report(self, path: Union[str, Path] = "note_de_calcul.html", **kwargs) -> Path:
        """Note de calcul HTML (ou PDF si ``path`` se termine par .pdf)."""
        from .report import generate_report
        return generate_report(self, path, **kwargs)

    def __repr__(self) -> str:
        return f"<Results spans={len(self.beam.spans)} step={self.step} m nodes={len(self._x)} loads={self._has_loads}>"


# =============================================================================
#  Poutre
# =============================================================================
@dataclass(frozen=True)
class _Span:
    length: float
    E: float
    I: float


class Beam:
    """Poutre continue : travées (de gauche à droite) et appuis.

    Les appuis doivent se trouver aux extrémités des travées. Appuis
    supportés : 'pin' / 'roller' (appui simple vertical), 'fixed'
    (encastrement, aux extrémités uniquement). Les porte-à-faux et les
    rotules ne sont pas modélisés par le moteur : ils lèvent une erreur
    explicite plutôt que de produire un résultat faux.
    """

    def __init__(self):
        self.spans: list[_Span] = []
        self.supports: dict[float, str] = {}

    @classmethod
    def continuous(cls, lengths: Sequence[float], E: Union[float, Sequence[float]],
                   I: Union[float, Sequence[float]], *, left: str = "pin", right: str = "pin") -> "Beam":
        """Poutre continue en une ligne : appuis simples à chaque extrémité de
        travée ; ``left`` / ``right`` = 'pin' ou 'fixed' pour les extrémités.

            >>> beam = Beam.continuous([10, 12, 10], E=210e9, I=8e-4)
            >>> beam = Beam.continuous([8, 8], E=30e9, I=[0.02, 0.03], left="fixed")
        """
        lengths = list(lengths)
        n = len(lengths)
        Es = [E] * n if np.isscalar(E) else list(E)
        Is = [I] * n if np.isscalar(I) else list(I)
        if len(Es) != n or len(Is) != n:
            raise ValueError(f"{n} travée(s) : E et I doivent être un nombre ou une liste de {n} valeurs.")
        b = cls()
        for L, e, i in zip(lengths, Es, Is):
            b.add_span(L, e, i)
        bounds = b.boundaries
        for x in bounds[1:-1]:
            b.add_support(x, "roller")
        b.add_support(bounds[0], left)
        b.add_support(bounds[-1], right if right != "pin" else "roller")
        return b

    # -- construction ----------------------------------------------------------
    def add_span(self, length: float, E: float, I: float) -> "Beam":
        self.spans.append(_Span(_positive("length", length), _positive("E", E), _positive("I", I)))
        return self

    def add_support(self, position: float, type: str = "pin") -> "Beam":
        t = str(type).lower()
        if t not in _SUPPORT_TYPES:
            raise ValueError(f"Type d'appui inconnu {type!r}. Valeurs possibles : {', '.join(_SUPPORT_TYPES)}.")
        self.supports[float(position)] = t
        return self

    @property
    def total_length(self) -> float:
        return float(sum(s.length for s in self.spans))

    @property
    def boundaries(self) -> list:
        xs, c = [0.0], 0.0
        for s in self.spans:
            c += s.length
            xs.append(c)
        return xs

    # -- traduction vers le moteur ------------------------------------------
    def _engine_model(self):
        """Listes (E, I, L) du moteur, encastrements = travées de longueur nulle."""
        if not self.spans:
            raise ValueError("La poutre n'a aucune travée : appelez add_span(length=..., E=..., I=...).")
        bounds = self.boundaries
        supports = dict(self.supports) if self.supports else {x: "pin" for x in bounds}

        def at(x):
            for p, t in supports.items():
                if abs(p - x) <= _TOL * max(1.0, self.total_length):
                    return t
            return None

        for p in supports:
            if all(abs(p - b) > _TOL * max(1.0, self.total_length) for b in bounds):
                raise ValueError(f"Appui à x = {p} m : les appuis doivent être aux extrémités des travées "
                                 f"{[round(b, 6) for b in bounds]}. Ajoutez une travée pour placer un appui "
                                 "intermédiaire.")
        missing = [b for b in bounds if at(b) is None]
        if missing:
            raise NotImplementedError(
                f"Pas d'appui en x = {missing} m : les porte-à-faux / rotules ne sont pas encore modélisés. "
                "Chaque extrémité de travée doit porter un appui ('pin', 'roller' ou 'fixed').")
        for b in bounds[1:-1]:
            if at(b) == "fixed":
                raise NotImplementedError(f"Encastrement intérieur en x = {b} m non supporté (extrémités uniquement).")

        E, I, L = [], [], []
        first = self.spans[0]
        if at(bounds[0]) == "fixed":
            E.append(first.E); I.append(first.I); L.append(0.0)
        for s in self.spans:
            E.append(s.E); I.append(s.I); L.append(s.length)
        last = self.spans[-1]
        if at(bounds[-1]) == "fixed":
            E.append(last.E); I.append(last.I); L.append(0.0)
        return E, I, L, {b: at(b) for b in bounds}

    def default_step(self) -> float:
        """Pas par défaut : plus petite travée / 40 (≥ 40 éléments par travée)."""
        return min(s.length for s in self.spans) / 40.0

    def analyze(self, step: Optional[float] = None,
                loads: Iterable[Union[PointLoadTrain, DistributedLoad]] = (),
                *, influence_lines: bool = True, moving_loads: bool = True) -> Results:
        """Calcule les lignes d'influence et, si ``loads`` est fourni, les enveloppes.

        ``step`` : pas de discrétisation [m] (défaut : :meth:`default_step`).
        L'erreur de discrétisation des enveloppes est O(step²) pour les
        grandeurs hyperstatiques (voir docs/audit/03_numerique.md, N6b).
        """
        from . import _core, Output

        E, I, L, _ = self._engine_model()
        h = self.default_step() if step is None else _positive("step", step)
        out = Output(E=E, I=I, L=L, steps=h)
        out.compute()
        loads = list(loads)
        has_loads = bool(loads) and moving_loads
        if has_loads:
            point, dist = [], []
            for ld in loads:
                if isinstance(ld, PointLoadTrain):
                    point.append(ld._engine(_core))
                elif isinstance(ld, DistributedLoad):
                    dist.append(ld._engine(_core, self.total_length))
                else:
                    raise TypeError(f"Charge non reconnue {ld!r} : utilisez PointLoadTrain ou DistributedLoad.")
            out.set_loads(point_loads=point, distrib_loads=dist)
            out.compute_load_envelopes()
        res = Results(self, out, h, has_loads)
        res.loads = tuple(loads)
        return res

    def to_dict(self) -> dict:
        _, _, _, supports = self._engine_model()
        return {"spans": [{"length": s.length, "E": s.E, "I": s.I} for s in self.spans],
                "supports": [{"x": x, "type": t} for x, t in supports.items()],
                "total_length": self.total_length}

    def __repr__(self) -> str:
        return f"<Beam spans={[s.length for s in self.spans]} supports={self.supports or 'auto'}>"
