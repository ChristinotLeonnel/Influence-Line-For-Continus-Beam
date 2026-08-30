"""
Tsaraloha.LIPoutreContinue — sous-librairie de calcul de poutres continues
(isostatiques et hyperstatiques) : lignes d'influence, enveloppes de
charge, export JSON.

Fait partie de la librairie « Tsaraloha », qui a vocation à regrouper
plusieurs sous-librairies. Chaque sous-librairie vit dans son propre
sous-paquet (ex. `Tsaraloha.LIPoutreContinue`), sur le modèle de
`Tsaraloha/__init__.py` (racine, léger) + sous-paquets spécifiques.

Structure du paquet (calquée sur numpy) :

    Tsaraloha/
        __init__.py                      <- racine du namespace Tsaraloha
        LIPoutreContinue/
            __init__.py                  <- vous êtes ici : API publique, pur Python
            __init__.pyi                 <- stubs de types pour l'autocomplétion (IDE)
            _validation.py               <- vérifications d'entrée + messages d'erreur
                                             clairs (voir docstring du module),
                                             sur le modèle des messages numpy
            _LIPoutreContinue*.so         <- extension compilée (pybind11), détail privé
                                             d'implémentation — ne jamais l'importer
                                             directement, toujours passer par
                                             `Tsaraloha.LIPoutreContinue.*`

Comme pour numpy (`numpy.ndarray` réexporté depuis
`numpy._core.multiarray`), les classes publiques ci-dessous enveloppent
légèrement les types de l'extension compilée : elles valident leurs
arguments *avant* de traverser la frontière C++ et transforment les
erreurs génériques (`ValueError`/`RuntimeError`) en messages qui disent
précisément quoi corriger, avec un exemple minimal.

Usage rapide
------------

    >>> import Tsaraloha.LIPoutreContinue as lipc
    >>> out = lipc.Output(E=[210e9, 210e9], I=[8e-4, 8e-4], L=[10, 10], steps=1.0)
    >>> out.compute()                    # calcul en mémoire uniquement
    >>> out.BM, out.SF, out.Def, out.Rot  # résultats exploitables directement
    >>> out.set_loads(
    ...     point_loads=[lipc.Load(intensity=[50.0], length=[0.0], name="Camion")],
    ...     distrib_loads=[],
    ... )
    >>> out.export_all()                 # optionnel : écrit le JSON dans `root`

Aucun calcul ni écriture disque n'est automatique : chaque étape
(`compute()`, `export_*()`) est déclenchée explicitement par l'appelant.

Davantage d'exemples
---------------------

1) Poutre à 3 travées, charge ponctuelle uniquement ::

    >>> import Tsaraloha.LIPoutreContinue as lipc
    >>> out = lipc.Output(E=[210e9] * 3, I=[6e-5] * 3, L=[6, 8, 6], steps=0.5)
    >>> out.compute()
    >>> out.set_loads(
    ...     point_loads=[lipc.Load(intensity=[80.0], length=[0.0], name="Essieu")],
    ...     distrib_loads=[],
    ... )
    >>> out.export_load_envelopes()      # écrit 04_Load_Envelopes/

2) Charge répartie uniquement (ex. poids propre) ::

    >>> udl = lipc.Load(intensity=[12.0], length=[0.0, 4.0], name="Poids propre")
    >>> out.set_loads(point_loads=[], distrib_loads=[udl])
    >>> out.export_load_envelopes()

3) Combinaison ponctuelle + répartie (enveloppe combinée) ::

    >>> out.set_loads(
    ...     point_loads=[lipc.Load(intensity=[50.0], length=[0.0], name="Camion")],
    ...     distrib_loads=[lipc.Load(intensity=[12.0], length=[0.0, 4.0], name="UDL")],
    ... )
    >>> out.export_all()

4) `Isostatique` seul — bas niveau, sans passer par `Output` ::

    >>> from Tsaraloha.LIPoutreContinue import Isostatique
    >>> travee = Isostatique(E=210e9, I=8e-4, L=10.0, steps=1.0)
    >>> travee.bending_moment()

5) Recharger des positions de charge déjà exportées ::

    >>> up = lipc.UpdatePositions(root="/chemin/de/sortie", input_lines=["ligne 1"])
    >>> up.run()                          # compute() + write_all()

6) Gestion des erreurs — les messages disent quoi corriger (cf. section
   « Messages d'erreur » ci-dessous) ::

    >>> lipc.Output(E=[210e9, 210e9], I=[8e-4], L=[10, 10], steps=1.0)
    Traceback (most recent call last):
        ...
    ValueError: E, I, L doivent décrire le même nombre de travées (même longueur) ; reçu len(E)=2, len(I)=1, len(L)=2.

Messages d'erreur
-----------------

Comme numpy (ex. ``ValueError: operands could not be broadcast together
with shapes (3,) (4,)``), chaque erreur d'entrée indique : (1) ce qui ne
va pas, (2) les valeurs concrètes fournies, (3) un exemple d'appel
correct. Voir `Tsaraloha.LIPoutreContinue._validation` pour le détail des
vérifications effectuées avant d'appeler l'extension compilée.
"""

from __future__ import annotations

import difflib

# =============================================================================
#  Import de l'extension compilée — avec message d'erreur explicite si le
#  module n'a pas été construit (cas fréquent : on a lancé `python` depuis
#  les sources sans avoir fait `pip install .` ou `cmake --build`).
# =============================================================================
try:
    from . import _LIPoutreContinue as _core
except ImportError as _exc:  # pragma: no cover
    raise ImportError(
        "Impossible d'importer l'extension compilée "
        "'Tsaraloha.LIPoutreContinue._LIPoutreContinue'.\n"
        "\n"
        "Causes les plus fréquentes, dans l'ordre à vérifier :\n"
        "  1. La librairie n'a pas été compilée du tout.\n"
        "     -> Depuis la racine du projet (là où se trouve pyproject.toml) :\n"
        "            pip install .\n"
        "        ou, pour une compilation manuelle :\n"
        "            cmake -B build && cmake --build build --config Release -j\n"
        "            export PYTHONPATH=build/python   # (Linux/macOS)\n"
        "  2. Vous exécutez un interpréteur Python différent de celui utilisé\n"
        "     pour la compilation (versions majeures/mineures incompatibles\n"
        "     entre l'extension .so/.pyd et l'interpréteur courant).\n"
        f"     -> Interpréteur courant : {__import__('sys').executable}\n"
        "  3. Le répertoire courant masque le paquet installé (un dossier\n"
        "     local `Tsaraloha/` sans l'extension compilée passe avant le\n"
        "     paquet installé dans site-packages).\n"
        "\n"
        f"Erreur d'origine ({type(_exc).__name__}) : {_exc}"
    ) from _exc

from . import _validation as _v

__version__ = "1.0.0"

# =============================================================================
#  API publique.
#
#  Comme numpy expose `numpy.ndarray` alors que le type vit dans
#  `numpy._core.multiarray`, les classes ci-dessous vivent dans
#  `Tsaraloha.LIPoutreContinue._LIPoutreContinue` et sont ré-exportées ici.
#
#  Les points d'entrée les plus utilisés par l'appelant Python
#  (`Output`, `Hyperstatique`, `Isostatique`, `Load`) sont enveloppés dans
#  une fine sous-classe Python : elle valide les arguments *avant*
#  d'appeler le constructeur C++, et enrichit les erreurs qui peuvent en
#  revenir (voir `_validation.enrich`). Le comportement calculatoire est
#  strictement identique à la classe C++ d'origine — seuls les messages
#  d'erreur changent.
# =============================================================================

_RawLoad = _core.Load
_RawIsostatique = _core.Isostatique
_RawHyperstatique = _core.Hyperstatique
_RawOutput = _core.Output


class Load(_RawLoad):
    """
    Décrit une charge mobile — ponctuelle (convoi d'essieux) ou répartie
    (uniforme / plurielle) — à passer à `Output.set_loads()`.

    Parameters
    ----------
    intensity : list[float]
        • Charge ponctuelle : force de chaque essieu [kN].
          `len(intensity)` = nombre d'essieux du convoi.
        • Charge répartie : intensité de chaque tronçon [kN/m].
          `len(intensity)` = nombre de tronçons.
    length : list[float]
        • Charge ponctuelle : distances entre essieux consécutifs [m].
          `len(length) == len(intensity)` (la dernière valeur n'est pas utilisée).
        • Charge répartie : `[PositionDepart, L_q1, L_q2, ..., L_qn]` [m].
          `len(length) == len(intensity) + 1`.
          `PositionDepart = 0` si la charge commence au début de la travée.
    name : str, optional
        Étiquette libre, utilisée dans les exports JSON (ex. "Essieu", "UDL").

    Raises
    ------
    TypeError
        Si `intensity`/`length` ne sont pas des séquences de nombres, ou
        si `name` n'est pas une chaîne.
    ValueError
        Si `intensity` est vide, ou si `length` n'a ni la longueur
        `len(intensity)` (ponctuelle) ni `len(intensity) + 1` (répartie).

    Examples
    --------
    Essieu unique 50 kN ::

        >>> Load(intensity=[50.0], length=[0.0], name="Essieu")
        <Load name=Essieu n_points=1>

    Convoi 6 essieux (tandem + tandem) ::

        >>> Load(
        ...     intensity=[6.0, 12.0, 12.0, 6.0, 12.0, 12.0],
        ...     length=[2.25, 4.5, 1.5, 5.0, 4.5, 1.5],
        ...     name="Convoi BC",
        ... )
        <Load name=Convoi BC n_points=6>

    Charge uniforme 12 kN/m sur 4 m, depuis le début ::

        >>> Load(intensity=[12.0], length=[0.0, 4.0], name="UDL")
        <Load name=UDL n_points=1>

    Charge plurielle : 45 kN/m sur 3 m, puis 10 sur 5 m, puis 25 sur 2 m ::

        >>> Load(intensity=[45.0, 10.0, 25.0], length=[0.0, 3.0, 5.0, 2.0], name="UDL2")
        <Load name=UDL2 n_points=3>
    """

    def __init__(self, intensity, length, name: str = ""):
        intensity = _v.as_float_sequence("intensity", intensity, example="[50.0]")
        length = _v.as_float_sequence("length", length, example="[0.0]")
        if len(length) not in (len(intensity), len(intensity) + 1):
            raise ValueError(
                "Incohérence de dimensions pour Load : "
                "pour une charge ponctuelle, len(length) doit être égal à len(intensity) ; "
                "pour une charge répartie, len(length) doit être égal à len(intensity) + 1 "
                f"(reçu len(intensity)={len(intensity)}, len(length)={len(length)}).\n"
                "Exemples :\n"
                '  • Ponctuelle : Load(intensity=[50.0], length=[0.0], name="Essieu")\n'
                '  • Répartie   : Load(intensity=[12.0], length=[0.0, 4.0], name="UDL")'
            )
        _v.check_type("name", name, str, example='"Camion"')
        super().__init__(intensity, length, name)


class Isostatique(_RawIsostatique):
    """
    Poutre isostatique simple (une seule travée).

    Contrairement à `Hyperstatique`/`Output`, E/I/L/steps sont copiés par
    valeur : aucun piège de durée de vie ici.

    Parameters
    ----------
    E : float
        Module d'élasticité (Pa), strictement positif.
    I : float
        Moment d'inertie (m^4), strictement positif.
    L : float
        Longueur de la travée (m), strictement positive.
    steps : float
        Pas de discrétisation (m), strictement positif.

    Raises
    ------
    TypeError
        Si l'un des arguments n'est pas un nombre.
    ValueError
        Si l'un des arguments n'est pas strictement positif.

    Examples
    --------
        >>> travee = Isostatique(E=210e9, I=8e-4, L=10.0, steps=1.0)
        >>> travee.bending_moment()

        >>> Isostatique(E=210e9, I=8e-4, L=-10.0, steps=1.0)
        Traceback (most recent call last):
            ...
        ValueError: 'L' doit être strictement positif, reçu -10.0.
    """

    def __init__(self, E, I, L, steps):
        E = _v.check_positive_number("E", E)
        I = _v.check_positive_number("I", I)
        L = _v.check_positive_number("L", L)
        steps = _v.check_positive_number("steps", steps)
        super().__init__(E, I, L, steps)


class Hyperstatique(_RawHyperstatique):
    """
    Poutre continue hyperstatique (plusieurs travées) — calcul bas niveau,
    utilisable indépendamment de `Output` quand seuls les résultats en
    mémoire (sans export JSON) sont nécessaires.

    Parameters
    ----------
    E, I, L : list[float]
        Module d'élasticité, moment d'inertie et longueur de chaque
        travée. Les trois listes doivent avoir la même longueur (une
        valeur par travée).
    steps : float
        Pas de discrétisation (m), strictement positif, commun à toutes
        les travées.

    Raises
    ------
    TypeError
        Si E/I/L ne sont pas des séquences de nombres, ou si `steps`
        n'est pas un nombre.
    ValueError
        Si E, I et L n'ont pas la même longueur, ou si `steps` n'est pas
        strictement positif.

    Examples
    --------
        >>> poutre = Hyperstatique(E=[210e9] * 2, I=[8e-4] * 2, L=[10, 10], steps=1.0)
        >>> poutre.bending_moments()

        >>> Hyperstatique(E=[210e9, 210e9], I=[8e-4], L=[10, 10], steps=1.0)
        Traceback (most recent call last):
            ...
        ValueError: E, I, L doivent décrire le même nombre de travées (même longueur) ; reçu len(E)=2, len(I)=1, len(L)=2.
    """

    def __init__(self, E, I, L, steps):
        E = _v.as_float_sequence("E", E, example="[210e9, 210e9]")
        I = _v.as_float_sequence("I", I, example="[8e-4, 8e-4]")
        L = _v.as_float_sequence("L", L, example="[10, 10]")
        _v.check_matching_lengths(E=E, I=I, L=L)
        steps = _v.check_positive_number("steps", steps)
        super().__init__(E, I, L, steps)

    @property
    def structural_model(self) -> dict:
        """
        Retourne la description complète du modèle de structure sous forme de dictionnaire Python
        (spans, young_modulus, inertia, step, node_lengths, n_spans, n_total_nodes, nodes).
        """
        node_lengths = []
        cumul = 0.0
        for span in self.L_spans:
            node_lengths.append(cumul)
            cumul += span
        node_lengths.append(cumul)

        nodes = self.points_x_coordinates(self.span_node_positions)

        return {
            "spans": list(self.L_spans),
            "young_modulus": list(self.E_spans),
            "inertia": list(self.I_spans),
            "step": float(self.steps),
            "node_lengths": node_lengths,
            "n_spans": int(self.number_of_spans),
            "n_total_nodes": len(nodes),
            "nodes": list(nodes),
        }

    def to_dict(self) -> dict:
        """Alias pour structural_model."""
        return self.structural_model


class Output(_RawOutput):
    """
    Point d'entrée principal de la librairie : calcule (en mémoire) et
    exporte (sur demande explicite) les résultats d'une poutre continue.

    Aucun calcul ni écriture disque n'est déclenché par le constructeur :
    appelez explicitement `compute()`, puis, si besoin, `set_loads()` et
    l'un des `export_*()`.

    Parameters
    ----------
    E, I, L : list[float]
        Module d'élasticité (Pa), moment d'inertie (m^4) et longueur (m)
        de chaque travée. Même longueur pour les trois (une valeur par
        travée).
    steps : float
        Pas de discrétisation (m), strictement positif.
    root : str or path-like, optional
        Dossier racine d'export JSON (utilisé par `export_*()`). Peut
        être omis si aucun export n'est prévu.

    Raises
    ------
    TypeError
        Si E/I/L ne sont pas des séquences de nombres, ou si `steps`
        n'est pas un nombre.
    ValueError
        Si E, I et L n'ont pas la même longueur, ou si `steps` n'est pas
        strictement positif.

    Examples
    --------
    Calcul en mémoire uniquement, sans charge ni export ::

        >>> out = Output(E=[210e9, 210e9], I=[8e-4, 8e-4], L=[10, 10], steps=1.0)
        >>> out.compute()
        >>> out.BM, out.SF, out.Def, out.Rot
        >>> out.bending_moment_max_positions

    Avec charges et export ::

        >>> out.set_loads(
        ...     point_loads=[Load(intensity=[50.0], length=[0.0], name="Camion")],
        ...     distrib_loads=[Load(intensity=[12.0], length=[0.0, 4.0], name="UDL")],
        ... )
        >>> out.export_all()

    Erreur typique — tailles incohérentes (E/I/L doivent correspondre) ::

        >>> Output(E=[210e9, 210e9], I=[8e-4], L=[10, 10], steps=1.0)
        Traceback (most recent call last):
            ...
        ValueError: E, I, L doivent décrire le même nombre de travées (même longueur) ; reçu len(E)=2, len(I)=1, len(L)=2.

    Erreur typique — export de charges sans `set_loads()` préalable ::

        >>> out.export_load_envelopes()
        Traceback (most recent call last):
            ...
        RuntimeError: Output::exportLoadEnvelopes: aucune charge fournie — appelez setLoads(point_loads, distrib_loads) avant (...)
        <BLANKLINE>
          Levée par : Output.export_load_envelopes()
          Exemple minimal :
              out.set_loads(
                  point_loads=[Load(intensity=[50.0], length=[0.0], name="Camion")],
                  distrib_loads=[],
              )
              out.export_load_envelopes()
    """

    def __init__(self, E, I, L, steps, root=""):
        E = _v.as_float_sequence("E", E, example="[210e9, 210e9]")
        I = _v.as_float_sequence("I", I, example="[8e-4, 8e-4]")
        L = _v.as_float_sequence("L", L, example="[10, 10]")
        _v.check_matching_lengths(E=E, I=I, L=L)
        steps = _v.check_positive_number("steps", steps)
        super().__init__(E, I, L, steps, root)

    def set_loads(self, point_loads=(), distrib_loads=()):
        """
        Fournit les charges directement (mode « sans fichier »),
        obligatoire avant `export_load_envelopes()`/`export_all()`.

        Parameters
        ----------
        point_loads : list[Load]
            Charges ponctuelles. Liste vide si aucune.
        distrib_loads : list[Load]
            Charges réparties. Liste vide si aucune.

        Raises
        ------
        TypeError
            Si un élément de `point_loads`/`distrib_loads` n'est pas un
            `Load`.
        """
        point_loads = list(point_loads)
        distrib_loads = list(distrib_loads)
        for i, load_obj in enumerate(point_loads):
            if not isinstance(load_obj, _RawLoad):
                raise TypeError(
                    f"'point_loads[{i}]' doit être un "
                    "Tsaraloha.LIPoutreContinue.Load, reçu "
                    f"{type(load_obj).__name__!r} : {load_obj!r}.\n"
                    'Exemple attendu : Load(intensity=[50.0], length=[0.0], name="Camion")'
                )
        for i, load_obj in enumerate(distrib_loads):
            if not isinstance(load_obj, _RawLoad):
                raise TypeError(
                    f"'distrib_loads[{i}]' doit être un "
                    "Tsaraloha.LIPoutreContinue.Load, reçu "
                    f"{type(load_obj).__name__!r} : {load_obj!r}.\n"
                    "Exemple attendu : Load(intensity=[12.0], "
                    'length=[0.0, 4.0], name="UDL")'
                )
        self._user_point_loads = point_loads
        self._user_distrib_loads = distrib_loads
        return super().set_loads(point_loads, distrib_loads)

    def export_load_envelopes(self):
        """Écrit 04_Load_Envelopes/ (nécessite `set_loads()` au préalable)."""
        try:
            return super().export_load_envelopes()
        except RuntimeError as exc:
            raise _v.enrich(
                exc,
                where="Output.export_load_envelopes()",
                example=(
                    "out.set_loads(\n"
                    "    point_loads=[Load(intensity=[50.0], length=[0.0], "
                    'name="Camion")],\n'
                    "    distrib_loads=[],\n"
                    ")\n"
                    "out.export_load_envelopes()"
                ),
            ) from exc

    def export_all(self):
        """Écrit tout (équivalent de export_influence_lines() + \
        export_load_envelopes() + export_critical_values()). Nécessite \
        `set_loads()` au préalable, comme `export_load_envelopes()`."""
        try:
            return super().export_all()
        except RuntimeError as exc:
            raise _v.enrich(
                exc,
                where="Output.export_all()",
                example=(
                    "out.set_loads(\n"
                    "    point_loads=[Load(intensity=[50.0], length=[0.0], "
                    'name="Camion")],\n'
                    "    distrib_loads=[],\n"
                    ")\n"
                    "out.export_all()"
                ),
            ) from exc

    @property
    def structural_model(self) -> dict:
        """
        Retourne la description complète du modèle de structure et de ses charges
        sous forme de dictionnaire Python.
        """
        node_lengths = []
        cumul = 0.0
        for span in self.L_spans:
            node_lengths.append(cumul)
            cumul += span
        node_lengths.append(cumul)

        model = {
            "spans": list(self.L_spans),
            "young_modulus": list(self.E_spans),
            "inertia": list(self.I_spans),
            "step": float(self.steps),
            "node_lengths": list(self.node_lengths) if len(self.node_lengths) > 0 else node_lengths,
            "n_spans": int(self.number_of_spans),
        }
        if len(self.X) > 0:
            model["n_total_nodes"] = len(self.X)
            model["nodes"] = list(self.X)

        if hasattr(self, "_user_point_loads") and self._user_point_loads:
            model["point_loads"] = [
                l.to_dict() if hasattr(l, "to_dict") else {
                    "intensity": list(l.intensity),
                    "length": list(l.length),
                    "name": l.name,
                }
                for l in self._user_point_loads
            ]
        if hasattr(self, "_user_distrib_loads") and self._user_distrib_loads:
            model["distrib_loads"] = [
                l.to_dict() if hasattr(l, "to_dict") else {
                    "intensity": list(l.intensity),
                    "length": list(l.length),
                    "name": l.name,
                }
                for l in self._user_distrib_loads
            ]

        return model

    def to_dict(self) -> dict:
        """Alias pour structural_model."""
        return self.structural_model



# ── Autres structures — ré-exportées telles quelles (API déjà simple,
#    peu de manières incorrectes de les utiliser) ─────────────────────────
Configuration = _core.Configuration
Loading = _core.Loading

# ── Chemins d'export et repositionnement des charges ────────────────────────
ProjectPaths = _core.ProjectPaths
UpdatePositions = _core.UpdatePositions

# ── Structures de résultats ──────────────────────────────────────────────────
Position1D = _core.Position1D
Position2D = _core.Position2D
Position3D = _core.Position3D
CombineLoadPosition = _core.CombineLoadPosition
LoadDelivery = _core.LoadDelivery
CriticalSectionResult = _core.CriticalSectionResult

__all__ = [
    "__version__",
    "Output",
    "Isostatique",
    "Hyperstatique",
    "Configuration",
    "Loading",
    "Load",
    "ProjectPaths",
    "UpdatePositions",
    "Position1D",
    "Position2D",
    "Position3D",
    "CombineLoadPosition",
    "LoadDelivery",
    "CriticalSectionResult",
]


def __getattr__(name: str):
    """
    Filet de sécurité (PEP 562) : si un type existe dans l'extension
    compilée mais n'a pas encore été ré-exporté explicitement ci-dessus,
    on le retrouve quand même plutôt que de lever un AttributeError sec.

    Comme numpy (depuis la 1.25) sur ses erreurs d'attribut de module, on
    suggère les noms les plus proches quand on en trouve, pour couvrir le
    cas fréquent d'une simple faute de frappe ::

        >>> import Tsaraloha.LIPoutreContinue as lipc
        >>> lipc.Outut
        Traceback (most recent call last):
            ...
        AttributeError: module 'Tsaraloha.LIPoutreContinue' has no attribute 'Outut'. Did you mean: 'Output'?
    """
    if hasattr(_core, name):
        return getattr(_core, name)

    candidates = difflib.get_close_matches(
        name, list(dict.fromkeys(list(__all__) + dir(_core))), n=3
    )
    message = f"module 'Tsaraloha.LIPoutreContinue' has no attribute {name!r}."
    if candidates:
        suggestions = ", ".join(repr(c) for c in candidates)
        message += f" Did you mean: {suggestions}?"
    raise AttributeError(message)
