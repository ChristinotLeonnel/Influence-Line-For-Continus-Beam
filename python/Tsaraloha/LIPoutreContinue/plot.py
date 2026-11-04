"""
Tsaraloha.LIPoutreContinue.plot — graphiques des résultats d'analyse.

Toutes les fonctions prennent un objet ``Results`` (``beam.analyze(...)``)
et renvoient l'``Axes`` ou la ``Figure`` matplotlib créée, pour pouvoir la
personnaliser ou l'enregistrer (``fig.savefig("m.png", dpi=200)``).

    >>> import Tsaraloha.LIPoutreContinue as tsa
    >>> from Tsaraloha.LIPoutreContinue import plot
    >>> res = beam.analyze(loads=[tsa.PointLoadTrain([60, 120], [1.5])])
    >>> plot.dashboard(res)                       # vue d'ensemble
    >>> plot.envelope(res, "M")                   # enveloppe annotée
    >>> plot.critical_configuration(res, "M")     # charges à la position critique
    >>> plot.influence_line(res, "V", x=10.0)     # ligne d'influence
    >>> plot.reactions(res)

Fonctions :
    beam(res)                         schéma coté de la poutre (et des charges)
    envelope(res, q, kind)            enveloppes max(x) / min(x) annotées
    influence_line(res, q, x)         ligne d'influence en une section
    critical_configuration(res, q)    position critique des charges + ligne d'influence
    reactions(res)                    réactions extrêmes par appui
    dashboard(res)                    figure de synthèse (tout ce qui précède)

Conventions : le moment fléchissant est tracé côté fibre tendue (axe M
orienté vers le bas, M > 0 sous la ligne de référence), comme en
résistance des matériaux. Les fonctions 1.x (``plot_output_full``, …)
restent disponibles, mais sont dépréciées.
"""
from __future__ import annotations

import warnings
from typing import Optional

import numpy as np

try:
    import matplotlib.pyplot as plt
    from matplotlib import patches as mpatches
    _HAS_MATPLOTLIB = True
except ImportError:  # pragma: no cover - matplotlib est une dépendance requise
    plt = None
    _HAS_MATPLOTLIB = False

__all__ = ["beam", "envelope", "influence_line", "critical_configuration", "reactions", "dashboard",
           "COLORS"]

# Palette (lisible en couleur et en niveaux de gris)
COLORS = {
    "max": "#c0392b",       # rouge : extrême positif
    "min": "#1f5fa8",       # bleu : extrême négatif
    "fill": "#8fb3d9",
    "beam": "#1b1b1b",
    "support": "#4a4a4a",
    "axle": "#d35400",
    "udl": "#16a085",
    "grid": "#d9d9d9",
    "il": "#2c3e50",
}
_Q = {
    "M": ("Moment fléchissant", "kN·m"), "V": ("Effort tranchant", "kN"),
    "w": ("Flèche", "m"), "theta": ("Rotation", "rad"),
}
_W_SCALE = {"w": (1e3, "mm")}   # affichage : flèche en mm


def _require_matplotlib() -> None:
    if not _HAS_MATPLOTLIB:
        raise ImportError("matplotlib est requis : pip install --force-reinstall Tsaraloha")


def _q(quantity: str) -> str:
    from .api import _quantity
    return _quantity(quantity)


def _ax(ax, figsize=(11, 4.2)):
    _require_matplotlib()
    if ax is None:
        _, ax = plt.subplots(figsize=figsize, constrained_layout=True)
    return ax


def _style(ax, xlabel="x [m]", ylabel=""):
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.grid(True, color=COLORS["grid"], linewidth=0.6)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)


def _supports(ax, res):
    for xs in res.support_positions:
        ax.axvline(xs, color=COLORS["support"], linewidth=0.8, linestyle=(0, (4, 3)), alpha=0.6, zorder=1)


def _display(q, values):
    """Conversion d'affichage (flèche en mm)."""
    f, unit = _W_SCALE.get(q, (1.0, _Q[q][1]))
    return np.asarray(values) * f, unit


def _fmt(v: float) -> str:
    a = abs(v)
    if a == 0:
        return "0"
    if a >= 1000 or a < 0.01:
        return f"{v:.3g}"
    return f"{v:.2f}" if a < 100 else f"{v:.1f}"


def _annotate(ax, x, y, text, color, right):
    """Étiquette posée à côté du point (pas au-dessus) : elle reste dans le
    cadre des données et ne chevauche jamais le titre."""
    ax.plot([x], [y], marker="o", color=color, markersize=6, zorder=5)
    ax.annotate(text, (x, y), xytext=(10 if right else -10, 0), textcoords="offset points",
                ha="left" if right else "right", va="center", fontsize=9, color=color, fontweight="bold",
                bbox=dict(boxstyle="round,pad=0.25", fc="white", ec=color, lw=0.8, alpha=0.9), zorder=6)


def _engine_names(res):
    """Associe les noms uniques du moteur ('Camion', 'Camion #2', …) aux charges."""
    from .api import PointLoadTrain
    loads = list(getattr(res, "loads", ()))
    ordered = [l for l in loads if isinstance(l, PointLoadTrain)] + [l for l in loads if not isinstance(l, PointLoadTrain)]
    taken, out = [], {}
    for l in ordered:
        base = l.name or "charge"
        c, k = base, 2
        while c in taken:
            c, k = f"{base} #{k}", k + 1
        taken.append(c)
        out[c] = l
    return out


# =============================================================================
#  Schéma de la poutre
# =============================================================================
def beam(res, ax=None, *, placed_loads: Optional[dict] = None, title: str = "Poutre"):
    """Schéma coté : travées, appuis et, si ``placed_loads`` ({nom: origine}),
    les charges dessinées à leur position."""
    ax = _ax(ax, figsize=(11, 2.6))
    bd = res.beam.to_dict()
    L = bd["total_length"]
    ax.plot([0, L], [0, 0], color=COLORS["beam"], linewidth=4, solid_capstyle="butt", zorder=3)
    h = 0.06 * L
    for s in bd["supports"]:
        x = s["x"]
        if s["type"] == "fixed":
            side = -1 if x <= 1e-9 else 1
            ax.plot([x, x], [-h, h], color=COLORS["beam"], linewidth=3, zorder=3)
            for k in np.linspace(-h, h, 6):
                ax.plot([x, x + side * 0.25 * h], [k, k - 0.25 * h], color=COLORS["support"], linewidth=1)
        else:
            ax.add_patch(mpatches.Polygon([[x, 0], [x - 0.35 * h, -0.7 * h], [x + 0.35 * h, -0.7 * h]],
                                          closed=True, fc="white", ec=COLORS["beam"], lw=1.4, zorder=4))
            if s["type"] == "roller":
                ax.plot([x - 0.45 * h, x + 0.45 * h], [-0.85 * h, -0.85 * h], color=COLORS["beam"], lw=1.2)
    c = 0.0
    for i, sp in enumerate(bd["spans"]):
        ax.annotate("", (c, -1.6 * h), (c + sp["length"], -1.6 * h),
                    arrowprops=dict(arrowstyle="<->", color=COLORS["support"], lw=0.8))
        ax.text(c + sp["length"] / 2, -1.5 * h, f"L{i + 1} = {sp['length']:g} m", ha="center", va="bottom", fontsize=9)
        c += sp["length"]

    if placed_loads:
        names = _engine_names(res)
        for name, origin in placed_loads.items():
            ld = names.get(name)
            if ld is None or origin is None or not np.isfinite(origin):
                continue
            if hasattr(ld, "axles"):
                offs = np.concatenate([[0.0], np.cumsum(ld.spacings)])
                pmax = max(ld.axles)
                for p, d in zip(ld.axles, offs):
                    xa = origin + d
                    if -1e-9 <= xa <= L + 1e-9:
                        top = 0.4 * h + 1.6 * h * p / pmax
                        ax.annotate("", (xa, 0.12 * h), (xa, top), arrowprops=dict(arrowstyle="-|>", color=COLORS["axle"], lw=1.6))
                        ax.text(xa, top, f"{p:g}", ha="center", va="bottom", fontsize=8, color=COLORS["axle"])
            else:
                qs, ls = ld._segments(L)
                x0, qmax = origin, max(qs)
                for q, l in zip(qs, ls):
                    a, b = max(x0, 0.0), min(x0 + l, L)
                    if b > a:
                        hh = 0.3 * h + 0.9 * h * q / qmax
                        ax.add_patch(mpatches.Rectangle((a, 0.12 * h), b - a, hh, fc=COLORS["udl"], alpha=0.25, ec=COLORS["udl"]))
                        ax.text(a, 0.12 * h + hh, f" {q:g} kN/m", ha="left", va="bottom", fontsize=8,
                                color=COLORS["udl"])
                    x0 += l
    ax.set_xlim(-0.04 * L, 1.04 * L)
    ax.set_ylim(-2.3 * h, 2.6 * h)
    ax.set_aspect("auto")
    ax.axis("off")
    ax.set_title(title, loc="left", fontsize=11, fontweight="bold")
    return ax


# =============================================================================
#  Enveloppes
# =============================================================================
def envelope(res, quantity: str = "M", kind: str = "combined", ax=None, *, annotate: bool = True,
             title: Optional[str] = None):
    """Enveloppes max(x) et min(x) d'une grandeur, extrêmes annotés."""
    q = _q(quantity)
    e = res.envelope(q, kind)
    ymax, unit = _display(q, e.max)
    ymin, _ = _display(q, e.min)
    ax = _ax(ax)
    ax.fill_between(e.x, ymin, ymax, color=COLORS["fill"], alpha=0.25, linewidth=0, zorder=2)
    ax.plot(e.x, ymax, color=COLORS["max"], lw=1.8, label="enveloppe max", zorder=3)
    ax.plot(e.x, ymin, color=COLORS["min"], lw=1.8, label="enveloppe min", zorder=3)
    ax.axhline(0, color=COLORS["beam"], lw=1.0, zorder=2)
    _supports(ax, res)
    invert = q == "M"
    if annotate:
        mid = 0.5 * (e.x[0] + e.x[-1])
        for ext, color in ((e.max_extremum, COLORS["max"]), (e.min_extremum, COLORS["min"])):
            if ext.value == 0:
                continue
            v, _ = _display(q, [ext.value])
            _annotate(ax, ext.x, v[0], f"{_fmt(v[0])} {unit}\nx = {ext.x:.2f} m", color, right=ext.x < mid)
    if invert:
        ax.invert_yaxis()
    _style(ax, ylabel=f"{_Q[q][0]} [{unit}]")
    kinds = {"combined": "toutes charges", "point": "convois", "distributed": "charges réparties"}
    ax.set_title(title or f"{_Q[q][0]} — enveloppe ({kinds[kind]})", loc="left", fontsize=11, fontweight="bold")
    ax.legend(loc="upper right", fontsize=8, frameon=False)
    ax.margins(x=0.01, y=0.18)
    return ax


# =============================================================================
#  Lignes d'influence
# =============================================================================
def influence_line(res, quantity: str = "M", x: float = 0.0, ax=None, *, title: Optional[str] = None):
    """Ligne d'influence de ``quantity`` à la section la plus proche de ``x``
    (effet par kN d'une charge unité)."""
    q = _q(quantity)
    lx, v, xs = res.influence_line(q, x)
    f, unit = _W_SCALE.get(q, (1.0, _Q[q][1]))
    v = v * f
    ax = _ax(ax)
    ax.fill_between(lx, 0, v, where=v >= 0, color=COLORS["max"], alpha=0.18, lw=0, interpolate=True)
    ax.fill_between(lx, 0, v, where=v <= 0, color=COLORS["min"], alpha=0.18, lw=0, interpolate=True)
    ax.plot(lx, v, color=COLORS["il"], lw=1.6)
    ax.axhline(0, color=COLORS["beam"], lw=1.0)
    ax.axvline(xs, color=COLORS["axle"], lw=1.2, linestyle="--")
    ax.text(xs, ax.get_ylim()[1], f" section x = {xs:.2f} m", color=COLORS["axle"], fontsize=8, va="top")
    _supports(ax, res)
    _style(ax, xlabel="position de la charge unité [m]", ylabel=f"{_Q[q][0]} [{unit}/kN]")
    ax.set_title(title or f"Ligne d'influence — {_Q[q][0]} en x = {xs:.2f} m", loc="left", fontsize=11,
                 fontweight="bold")
    return ax


def critical_configuration(res, quantity: str = "M", which: str = "governing", fig=None):
    """Position la plus défavorable des charges : poutre chargée (haut) et
    ligne d'influence de la section critique (bas).

    ``which`` : 'max', 'min' ou 'governing' (plus grand module)."""
    _require_matplotlib()
    q = _q(quantity)
    e = res.envelope(q)
    ext = {"max": e.max_extremum, "min": e.min_extremum}.get(which, e.governing)
    if fig is None:
        fig = plt.figure(figsize=(11, 5.8), constrained_layout=True)
    gs = fig.add_gridspec(2, 1, height_ratios=[1, 2])
    a0, a1 = fig.add_subplot(gs[0]), fig.add_subplot(gs[1])
    origins = {n: d["position"] for n, d in ext.loads.items()}
    v, unit = _display(q, [ext.value])
    beam(res, a0, placed_loads=origins,
         title=f"Position critique — {_Q[q][0]} = {_fmt(v[0])} {unit} en x = {ext.x:.2f} m")
    influence_line(res, q, ext.x, a1)
    for name, d in ext.loads.items():
        if d["position"] is not None and np.isfinite(d["position"]):
            a1.plot([], [], " ", label=f"{name} : {_fmt(_display(q, [d['value']])[0][0])} {unit} (origine {d['position']:.2f} m)")
    a1.legend(loc="best", fontsize=8, frameon=False)
    a1.set_xlim(a0.get_xlim())
    return fig


# =============================================================================
#  Réactions
# =============================================================================
def reactions(res, ax=None, kind: str = "combined"):
    """Réactions extrêmes (max ≥ 0, min ≤ 0) à chaque appui [kN]."""
    ax = _ax(ax, figsize=(8, 3.8))
    rs = res.reactions(kind)
    idx = np.arange(len(rs))
    rmax = np.array([r["max"] for r in rs])
    rmin = np.array([r["min"] for r in rs])
    ax.bar(idx, rmax, color=COLORS["max"], alpha=0.85, label="R max")
    ax.bar(idx, rmin, color=COLORS["min"], alpha=0.85, label="R min")
    for i, (a, b) in enumerate(zip(rmax, rmin)):
        ax.text(i, a, f"{_fmt(a)}", ha="center", va="bottom", fontsize=8)
        if b < 0:
            ax.text(i, b, f"{_fmt(b)}", ha="center", va="top", fontsize=8)
    ax.axhline(0, color=COLORS["beam"], lw=1)
    ax.set_xticks(idx, [f"Appui {r['support']}\nx = {r['x']:g} m" for r in rs], fontsize=8)
    _style(ax, xlabel="", ylabel="Réaction [kN]")
    ax.set_title("Réactions d'appui extrêmes", loc="left", fontsize=11, fontweight="bold")
    ax.legend(fontsize=8, frameon=False)
    ax.margins(y=0.2)
    return ax


# =============================================================================
#  Synthèse
# =============================================================================
def dashboard(res, fig=None):
    """Figure de synthèse : poutre, enveloppes M/V/w, réactions, extrêmes."""
    _require_matplotlib()
    if fig is None:
        fig = plt.figure(figsize=(13, 11), constrained_layout=True)
    if not res._has_loads:
        gs = fig.add_gridspec(3, 1, height_ratios=[0.7, 1.5, 1.5])
        beam(res, fig.add_subplot(gs[0]))
        x0 = res.output.influence_line_extrema["bending_moment"]["min"]["section_x"]
        influence_line(res, "M", x0, fig.add_subplot(gs[1]))
        influence_line(res, "V", res.support_positions[0], fig.add_subplot(gs[2]))
        fig.suptitle("Lignes d'influence (aucune charge mobile)", fontsize=13, fontweight="bold")
        return fig
    gs = fig.add_gridspec(4, 2, height_ratios=[0.75, 1.3, 1.3, 1.3], width_ratios=[2.2, 1])
    e = res.envelope("M")
    beam(res, fig.add_subplot(gs[0, :]), placed_loads={n: d["position"] for n, d in e.governing.loads.items()},
         title="Poutre et charges à la position du moment gouvernant")
    envelope(res, "M", ax=fig.add_subplot(gs[1, 0]))
    envelope(res, "V", ax=fig.add_subplot(gs[2, 0]))
    envelope(res, "w", ax=fig.add_subplot(gs[3, 0]))
    reactions(res, fig.add_subplot(gs[1:3, 1]))
    at = fig.add_subplot(gs[3, 1])
    at.axis("off")
    rows = []
    for q in ("M", "V", "w"):
        ee = res.envelope(q)
        f, u = _W_SCALE.get(q, (1.0, _Q[q][1]))
        rows.append([_Q[q][0], f"{_fmt(ee.max_extremum.value * f)}", f"{_fmt(ee.min_extremum.value * f)}", u])
    t = at.table(cellText=rows, colLabels=["Grandeur", "max", "min", "unité"], loc="center", cellLoc="center",
                 colWidths=[0.46, 0.2, 0.2, 0.14])
    t.auto_set_font_size(False)
    t.set_fontsize(9)
    t.scale(1, 1.5)
    at.set_title("Valeurs extrêmes", loc="left", fontsize=11, fontweight="bold")
    fig.suptitle(f"Synthèse — {len(res.beam.spans)} travée(s), pas {res.step:g} m", fontsize=13, fontweight="bold")
    return fig


# =============================================================================
#  Compatibilité 1.x (dépréciée)
# =============================================================================
def _legacy(name):
    def wrapper(*args, **kwargs):
        warnings.warn(f"plot.{name}() est déprécié (API 1.x) : utilisez plot.dashboard(), plot.envelope(), "
                      "plot.influence_line() ou plot.critical_configuration() avec un objet Results.",
                      DeprecationWarning, stacklevel=2)
        from . import _plot_legacy
        return getattr(_plot_legacy, name)(*args, **kwargs)
    wrapper.__name__ = name
    return wrapper


for _n in ("plot_isostatique_influence_lines", "plot_hyperstatique_influence_lines", "plot_support_moments",
           "plot_bm_envelopes", "plot_load_on_influence_line", "plot_load_summary", "plot_output_full"):
    globals()[_n] = _legacy(_n)
del _n
