"""
Tsaraloha.LIPoutreContinue.plot
================================
Module de visualisation des lignes d'influence et des charges pour
poutres continues (isostatiques et hyperstatiques).

Fonctions disponibles
---------------------
plot_isostatique_influence_lines(poutre, x, *, title, figsize)
    Trace les 4 L.I. (M, V, w, θ) d'une poutre isostatique.

plot_hyperstatique_influence_lines(hyper, BM, X, *, span, section, title, figsize)
    Trace les L.I. Moment de chaque section d'une travée donnée.

plot_support_moments(hyper, X, *, title, figsize)
    Trace les L.I. des moments hyperstatiques sur chaque appui.

plot_bm_envelopes(X, BM, *, title, figsize)
    Enveloppe du moment fléchissant (mi-travée de chaque travée).

plot_load_on_influence_line(X, li_curve, loads, alpha_opt, *, span_lengths,
                            title, figsize)
    Trace une L.I. et superpose la/les charges à leur position optimale.

plot_load_summary(X, BM, loading, span, section, *, title, figsize)
    Résumé complet : L.I. + charges ponctuelles + réparties + combinée
    pour une section donnée.

plot_output_full(out, *, figsize)
    Vue synthétique complète à partir d'un objet Output (après compute).

Usage rapide
------------
    >>> import Tsaraloha.LIPoutreContinue as lipc
    >>> from Tsaraloha.LIPoutreContinue.plot import plot_output_full
    >>> out = lipc.Output(E=[30e9]*3, I=[I_val]*3, L=[10,14,10], steps=0.5)
    >>> out.compute()
    >>> plot_output_full(out)
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Dict, List, Optional, Sequence, Tuple

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.gridspec as gridspec
from matplotlib.figure import Figure
from matplotlib.axes import Axes
import numpy as np

if TYPE_CHECKING:
    from Tsaraloha.LIPoutreContinue import (
        Isostatique,
        Hyperstatique,
        Loading,
        Load,
        Output,
    )

# ---------------------------------------------------------------------------
# Palette / thème interne
# ---------------------------------------------------------------------------
_PALETTE = [
    "#4C9BE8",  # bleu ciel
    "#E84C4C",  # rouge corail
    "#4CE8A0",  # vert menthe
    "#E8A44C",  # orange doré
    "#A44CE8",  # violet
    "#4CE8E8",  # cyan
    "#E8E84C",  # jaune
]

_ZERO_STYLE   = dict(color="#888888", linewidth=0.9, linestyle="--")
_GRID_STYLE   = dict(alpha=0.25, linestyle="--", linewidth=0.6)
_FILL_ALPHA   = 0.18
_LINE_WIDTH   = 2.2
_FONT_TITLE   = 13
_FONT_SUBTITLE = 10

# ---------------------------------------------------------------------------
# Helpers privés
# ---------------------------------------------------------------------------

def _pick(colors: Optional[List[str]], idx: int) -> str:
    if colors:
        return colors[idx % len(colors)]
    return _PALETTE[idx % len(_PALETTE)]


def _style_axes(ax: Axes, xlabel: str = "Position charge α [m]",
                ylabel: str = "", title: str = "") -> None:
    """Applique le style standard à un axe."""
    ax.set_facecolor("#0D1117")
    ax.tick_params(colors="#CCCCCC", labelsize=8)
    ax.spines[:].set_color("#333344")
    ax.set_xlabel(xlabel, color="#AAAAAA", fontsize=8)
    if ylabel:
        ax.set_ylabel(ylabel, color="#AAAAAA", fontsize=8)
    if title:
        ax.set_title(title, fontsize=_FONT_SUBTITLE, color="#DDDDEE", pad=6)
    ax.grid(**_GRID_STYLE, color="#334")
    ax.axhline(0, **_ZERO_STYLE)


def _style_fig(fig: Figure, title: str = "") -> None:
    """Fond sombre + titre global."""
    fig.patch.set_facecolor("#070B12")
    if title:
        fig.suptitle(title, fontsize=_FONT_TITLE, color="#E8EEFF",
                     fontweight="bold", y=0.98)


def _add_span_boundaries(ax: Axes, span_lengths: Sequence[float],
                          ymin: float, ymax: float) -> None:
    """
    Trace les appuis avec :
    - une ligne verticale jaune pointillée
    - un symbole triangle ▼ ancré sur l'axe des x
    - une étiquette «Ai — X.XX m» sous le triangle
    """
    cumul  = 0.0
    n_spans = len(span_lengths)

    # Collecte de tous les appuis (début + intermédiaires + fin)
    supports = []
    for i in range(n_spans + 1):
        supports.append(cumul)
        if i < n_spans:
            cumul += span_lengths[i]

    for k, x_sup in enumerate(supports):
        # Ligne verticale
        ax.axvline(x_sup, color="#FFCC44", linewidth=0.9,
                   linestyle=":", alpha=0.7, zorder=2)
        # Triangle — positionné sur l'axe zéro (transform=ax.transData)
        y_tri = ax.get_ylim()[0] + (ax.get_ylim()[1] - ax.get_ylim()[0]) * 0.01
        ax.plot(x_sup, y_tri, marker="v", color="#FFCC44",
                markersize=7, zorder=5, clip_on=False)
        # Étiquette position
        ax.text(
            x_sup, y_tri,
            f"  A{k}\n  {x_sup:.2f} m",
            color="#FFCC44", fontsize=6.5, va="top", ha="left",
            rotation=0, zorder=6,
        )


def _fill_curve(ax: Axes, x, y, color: str) -> None:
    """Remplissage positif/négatif en deux tons."""
    y_arr = np.asarray(y, dtype=float)
    ax.fill_between(x, y_arr, 0, where=y_arr >= 0, alpha=_FILL_ALPHA,
                    color=color, interpolate=True)
    ax.fill_between(x, y_arr, 0, where=y_arr < 0, alpha=_FILL_ALPHA * 1.6,
                    color=color, interpolate=True, hatch="///", linewidth=0)


def _ld_intensity(load) -> list:
    """Retourne intensity d'un Load (objet ou dict)."""
    return load["intensity"] if isinstance(load, dict) else load.intensity


def _ld_length(load) -> list:
    """Retourne length d'un Load (objet ou dict)."""
    return load["length"] if isinstance(load, dict) else load.length


def _ld_name(load) -> str:
    """Retourne name d'un Load (objet ou dict)."""
    return load["name"] if isinstance(load, dict) else load.name


def _load_label(load) -> str:
    intensity = _ld_intensity(load)
    length    = _ld_length(load)
    name      = _ld_name(load)
    if len(intensity) == 1:
        return f"{name}  [{intensity[0]:.0f} kN/m · {length[-1]:.1f} m]"
    parts = " | ".join(f"{p:.0f} kN" for p in intensity)
    return f"{name}  [{parts}]"


# ===========================================================================
#  1. Isostatique — 4 panneaux (M, V, w, θ)
# ===========================================================================

def plot_isostatique_influence_lines(
    poutre,
    x: float,
    *,
    title: str = "",
    figsize: Tuple[float, float] = (13, 7),
) -> Figure:
    """
    Trace les 4 lignes d'influence (M, V, w, θ) d'une poutre **isostatique**
    pour la section d'abscisse *x*.

    Parameters
    ----------
    poutre : Isostatique
        Objet poutre isostatique (déjà instancié).
    x : float
        Position de la section étudiée [m].
    title : str, optional
        Titre global de la figure.
    figsize : tuple, optional
        Taille de la figure (largeur, hauteur) en pouces.

    Returns
    -------
    Figure
        La figure matplotlib.

    Example
    -------
    >>> import Tsaraloha.LIPoutreContinue as lipc
    >>> from Tsaraloha.LIPoutreContinue.plot import plot_isostatique_influence_lines
    >>> p = lipc.Isostatique(E=30e9, I=5e-4, L=10.0, steps=0.25)
    >>> fig = plot_isostatique_influence_lines(p, x=5.0)
    >>> fig.show()
    """
    alpha   = poutre.node_positions
    LI_M    = poutre.eq_bending_moment(x)
    LI_V    = poutre.eq_shear_force(x, False)
    alpha_V = poutre.eq_shear_force(x, True)
    LI_w    = poutre.eq_deflection(x)
    LI_th   = poutre.eq_rotation(x)

    datasets = [
        (alpha,   LI_M,  "Moment fléchissant  M [kN·m/kN]", _PALETTE[0]),
        (alpha_V, LI_V,  "Effort tranchant  V [kN/kN]",      _PALETTE[1]),
        (alpha,   LI_w,  "Flèche  w [m/kN]",                 _PALETTE[2]),
        (alpha,   LI_th, "Rotation  θ [rad/kN]",             _PALETTE[3]),
    ]

    default_title = (
        title or
        f"Lignes d'influence — Poutre isostatique  L = {poutre.L} m  "
        f"(section x = {x} m)"
    )

    fig, axes = plt.subplots(2, 2, figsize=figsize)
    _style_fig(fig, default_title)

    for (absc, li, label, color), ax in zip(datasets, axes.flat):
        ax.plot(absc, li, color=color, linewidth=_LINE_WIDTH)
        _fill_curve(ax, absc, li, color)
        ax.axvline(x, color="#FFDD77", linewidth=1.0, linestyle=":",
                   alpha=0.8, label=f"x = {x} m")
        _style_axes(ax, ylabel=label, title=label)
        ax.legend(fontsize=7, facecolor="#111118", labelcolor="#CCCCCC",
                  framealpha=0.6)

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    return fig


# ===========================================================================
#  2. Hyperstatique — L.I. Moment d'une travée
# ===========================================================================

def plot_hyperstatique_influence_lines(
    hyper,
    BM: List[List[List[float]]],
    X: List[float],
    *,
    span: int = 0,
    section: Optional[int] = None,
    title: str = "",
    figsize: Tuple[float, float] = (13, 5),
) -> Figure:
    """
    Trace les L.I. de moment fléchissant d'une travée donnée, pour
    plusieurs sections.

    Parameters
    ----------
    hyper : Hyperstatique
        Objet poutre hyperstatique.
    BM : list
        Résultat de ``hyper.bending_moments()`` — shape [travée][section][alpha].
    X : list
        Abscisses globales (depuis ``hyper.points_x_coordinates(...)``).
    span : int
        Indice de la travée à afficher (0-indexé, défaut 0).
    section : int, optional
        Indice unique de section à tracer. Si None, trace 1/4, 1/2 et 3/4.
    title : str, optional
        Titre de la figure.
    figsize : tuple, optional
        Taille de la figure.

    Returns
    -------
    Figure

    Example
    -------
    >>> hyper = lipc.Hyperstatique(E=[30e9]*3, I=[I]*3, L=[10,14,10], steps=0.5)
    >>> BM = hyper.bending_moments()
    >>> X  = hyper.points_x_coordinates(hyper.span_node_positions)
    >>> from Tsaraloha.LIPoutreContinue.plot import plot_hyperstatique_influence_lines
    >>> fig = plot_hyperstatique_influence_lines(hyper, BM, X, span=1)
    """
    bm_span = BM[span]
    n_sec   = len(bm_span)
    mid     = n_sec // 2

    if section is not None:
        sections_to_plot = [section]
    else:
        q1  = n_sec // 4
        q3  = 3 * n_sec // 4
        sections_to_plot = sorted(set([q1, mid, q3]))

    default_title = (
        title or
        f"L.I. Moment fléchissant — Travée {span}  "
        f"(L = {hyper.L_spans[span]} m)"
    )

    fig, ax = plt.subplots(figsize=figsize)
    _style_fig(fig, default_title)

    for idx, sec in enumerate(sections_to_plot):
        color = _PALETTE[idx % len(_PALETTE)]
        frac  = sec / max(n_sec - 1, 1)
        label = f"Section {sec}  (x = {frac * hyper.L_spans[span]:.1f} m)"
        ax.plot(X, bm_span[sec], color=color, linewidth=_LINE_WIDTH, label=label)
        _fill_curve(ax, X, bm_span[sec], color)

    _add_span_boundaries(ax, hyper.L_spans, 0, 0)
    _style_axes(ax, ylabel="Moment fléchissant [kN·m]",
                title=f"Travée {span} — {len(sections_to_plot)} sections")
    ax.legend(fontsize=8, facecolor="#111118", labelcolor="#CCCCCC",
              framealpha=0.7)
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    return fig


# ===========================================================================
#  3. Moments sur appuis
# ===========================================================================

def plot_support_moments(
    hyper,
    X: List[float],
    *,
    title: str = "",
    figsize: Tuple[float, float] = (13, 5),
) -> Figure:
    """
    Trace les lignes d'influence des **moments hyperstatiques sur appuis**
    pour une poutre continue.

    Parameters
    ----------
    hyper : Hyperstatique
        Objet poutre hyperstatique.
    X : list
        Abscisses globales.
    title : str, optional
    figsize : tuple, optional

    Returns
    -------
    Figure

    Example
    -------
    >>> from Tsaraloha.LIPoutreContinue.plot import plot_support_moments
    >>> fig = plot_support_moments(hyper, X)
    """
    SM = hyper.support_moment
    default_title = (
        title or
        f"L.I. Moments hyperstatiques sur appuis — "
        f"{hyper.number_of_spans} travées"
    )

    fig, ax = plt.subplots(figsize=figsize)
    _style_fig(fig, default_title)

    for k, sm in enumerate(SM):
        color = _PALETTE[k % len(_PALETTE)]
        label = f"Appui {k}"
        ax.plot(X, sm, color=color, linewidth=_LINE_WIDTH, label=label)
        _fill_curve(ax, X, sm, color)

    _add_span_boundaries(ax, hyper.L_spans, 0, 0)
    _style_axes(ax, ylabel="Moment hyperstatique [kN·m]")
    ax.legend(fontsize=8, facecolor="#111118", labelcolor="#CCCCCC",
              framealpha=0.7)
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    return fig


# ===========================================================================
#  4. Enveloppe du moment — mi-travée de chaque travée
# ===========================================================================

def plot_bm_envelopes(
    X: List[float],
    BM: List[List[List[float]]],
    *,
    span_lengths: Optional[List[float]] = None,
    title: str = "",
    figsize: Tuple[float, float] = (13, 5),
) -> Figure:
    """
    Trace les L.I. de moment fléchissant à la **mi-travée** de chaque travée.

    Parameters
    ----------
    X : list
        Abscisses globales.
    BM : list
        Lignes d'influence du moment — shape [travée][section][alpha].
    span_lengths : list, optional
        Longueurs de chaque travée pour tracer les appuis.
    title : str, optional
    figsize : tuple, optional

    Returns
    -------
    Figure

    Example
    -------
    >>> from Tsaraloha.LIPoutreContinue.plot import plot_bm_envelopes
    >>> fig = plot_bm_envelopes(X, BM, span_lengths=[10, 14, 10])
    """
    default_title = title or "Enveloppe L.I. Moment fléchissant — mi-travée"
    fig, ax = plt.subplots(figsize=figsize)
    _style_fig(fig, default_title)

    for i, bm_span in enumerate(BM):
        mid   = len(bm_span) // 2
        color = _PALETTE[i % len(_PALETTE)]
        ax.plot(X, bm_span[mid], color=color, linewidth=_LINE_WIDTH,
                label=f"Travée {i}  (sec. {mid})")
        _fill_curve(ax, X, bm_span[mid], color)

    if span_lengths:
        _add_span_boundaries(ax, span_lengths, 0, 0)
    _style_axes(ax, ylabel="Moment fléchissant [kN·m]")
    ax.legend(fontsize=8, facecolor="#111118", labelcolor="#CCCCCC",
              framealpha=0.7)
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    return fig


# ===========================================================================
#  5. Charge(s) sur une L.I. — position optimale ou personnalisée
# ===========================================================================

def plot_load_on_influence_line(
    X: List[float],
    li_curve: List[float],
    loads: Sequence,
    alpha_opt: int,
    *,
    span_lengths: Optional[List[float]] = None,
    ylabel: str = "Moment fléchissant [kN·m]",
    title: str = "Charge sur ligne d'influence",
    figsize: Tuple[float, float] = (13, 5),
    load_type: str = "point",
) -> Figure:
    """
    Trace une **ligne d'influence** et superpose la/les charges à la
    position *alpha_opt* (optimale ou personnalisée).

    Parameters
    ----------
    X : list
        Abscisses globales [m].
    li_curve : list
        Valeurs de la L.I. pour chaque position alpha.
    loads : sequence of Load
        Charges à afficher.
    alpha_opt : int
        Indice de la position optimale dans *X*.
    span_lengths : list, optional
        Pour tracer les appuis.
    ylabel : str, optional
        Étiquette de l'axe Y.
    title : str, optional
        Titre de la figure.
    figsize : tuple, optional
    load_type : {'point', 'rect'}
        Type de charge : ``'point'`` pour des flèches, ``'rect'``
        pour un rectangle hachuré.

    Returns
    -------
    Figure

    Example
    -------
    >>> from Tsaraloha.LIPoutreContinue.plot import plot_load_on_influence_line
    >>> fig = plot_load_on_influence_line(
    ...     X, li_curve=BM[1][mid_sec], loads=[convoi_1],
    ...     alpha_opt=pos_opt["max_position"],
    ...     span_lengths=[10, 14, 10],
    ...     title="Convoi lourd — position optimale",
    ... )
    """
    X_arr  = np.asarray(X, dtype=float)
    li_arr = np.asarray(li_curve, dtype=float)

    fig, ax = plt.subplots(figsize=figsize)
    _style_fig(fig, title)

    # ── L.I. de fond ──────────────────────────────────────────────────────
    ax.plot(X_arr, li_arr, color=_PALETTE[0], linewidth=_LINE_WIDTH,
            label="L.I.")
    _fill_curve(ax, X_arr, li_arr, _PALETTE[0])

    # ── Marqueur de position optimale ─────────────────────────────────────
    x_opt = X_arr[alpha_opt] if alpha_opt < len(X_arr) else X_arr[-1]
    y_opt = li_arr[alpha_opt] if alpha_opt < len(li_arr) else li_arr[-1]
    ax.axvline(x_opt, color="#FFDD77", linewidth=1.2, linestyle="--",
               alpha=0.85, label=f"α opt = {x_opt:.2f} m")
    ax.scatter([x_opt], [y_opt], color="#FFDD77", s=60, zorder=5)

    # ── Représentation des charges ────────────────────────────────────────
    y_min, y_max = li_arr.min(), li_arr.max()
    span_y       = max(abs(y_max - y_min), 1e-6)
    arrow_base   = y_max + 0.08 * span_y
    arrow_h      = 0.12 * span_y

    for load_idx, load in enumerate(loads):
        color_load = _PALETTE[(load_idx + 2) % len(_PALETTE)]

        if load_type == "rect":
            ld_len   = _ld_length(load)   # ex. [0.0, 6.0, 2.0]
            ld_int   = _ld_intensity(load) # ex. [12.0, 20.0]
            ld_name  = _ld_name(load)
            n_seg    = len(ld_int)         # nombre de segments

            # Palette de dégradés verts pour chaque segment
            seg_colors = ["#4CE8A0", "#1DC46E", "#0E7A43",
                          "#7AFFC4", "#2AFFA0"]

            # Hauteur du bloc pour le segment d'intensité maximale
            rect_h_ref = arrow_h * 0.9
            int_max    = max(ld_int) if ld_int else 1.0

            # ── Premier marqueur : début global de la charge ──────────────
            x_global_start = x_opt
            ax.axvline(x_global_start, color=seg_colors[0],
                       linewidth=1.1, linestyle="--", alpha=0.85)
            ax.text(x_global_start, y_max + 0.02 * span_y,
                    f"  {x_global_start:.2f} m",
                    color=seg_colors[0], fontsize=7,
                    va="bottom", ha="left", rotation=90)

            legend_added = False
            cumul = 0.0
            for seg_idx in range(n_seg):
                seg_start = x_opt + cumul
                seg_len   = ld_len[seg_idx + 1]  # length[1], length[2], ...
                seg_end   = min(seg_start + seg_len, X_arr[-1])
                seg_int   = ld_int[seg_idx]
                seg_color = seg_colors[seg_idx % len(seg_colors)]
                cumul    += seg_len

                # ── Zone LI : sous-tableau précis sans débordure ─────────
                inner = (X_arr > seg_start) & (X_arr < seg_end)
                X_seg = np.concatenate([[seg_start], X_arr[inner], [seg_end]])
                y_s   = float(np.interp(seg_start, X_arr, li_arr))
                y_e   = float(np.interp(seg_end,   X_arr, li_arr))
                Y_seg = np.concatenate([[y_s], li_arr[inner], [y_e]])

                fill_label = (f"Zone UDL — {seg_int:.0f} kN/m "
                              f"({seg_start:.2f}–{seg_end:.2f} m)")
                ax.fill_between(
                    X_seg, Y_seg, 0,
                    color=seg_color, alpha=0.50,
                    label=fill_label if not legend_added else "_nolegend_",
                    zorder=3,
                )
                legend_added = True

                # Courbe vive sur le segment
                ax.plot(X_seg, Y_seg, color=seg_color,
                        linewidth=_LINE_WIDTH + 0.6, zorder=4)
                # Bords verticaux nets
                ax.plot([seg_start, seg_start], [0, y_s],
                        color=seg_color, linewidth=1.8, zorder=4)
                ax.plot([seg_end,   seg_end],   [0, y_e],
                        color=seg_color, linewidth=1.8, zorder=4)

                # Marqueur de fin de segment
                ax.axvline(seg_end, color=seg_color,
                           linewidth=1.0, linestyle="--", alpha=0.80)
                ax.text(seg_end, y_max + 0.02 * span_y,
                        f"  {seg_end:.2f} m",
                        color=seg_color, fontsize=7,
                        va="bottom", ha="left", rotation=90)

                # ── Bloc hachuré au-dessus : hauteur ∝ intensité ─────────
                rect_h = rect_h_ref * (seg_int / int_max)
                ax.barh(
                    arrow_base + rect_h / 2,
                    seg_end - seg_start,
                    height=rect_h,
                    left=seg_start,
                    color=seg_color,
                    alpha=0.55,
                    hatch="///",
                    edgecolor=seg_color,
                )
                ax.annotate(
                    f"{seg_int:.0f} kN/m",
                    xy=((seg_start + seg_end) / 2, arrow_base + rect_h),
                    fontsize=7, color=seg_color,
                    ha="center", va="bottom",
                )

            # Nom de la charge centré sur l'ensemble
            x_total_end = min(x_opt + sum(ld_len[1:]), X_arr[-1])
            ax.text(
                (x_opt + x_total_end) / 2,
                arrow_base + rect_h_ref * 1.25,
                ld_name,
                color="#FFFFFF", fontsize=8, ha="center", va="bottom",
                fontweight="bold",
            )
        else:
            # Charges ponctuelles — une flèche par essieu
            cumul_offset = 0.0
            for ess_idx, (P, gap) in enumerate(
                zip(load.intensity, load.length)
            ):
                if ess_idx > 0:
                    cumul_offset += gap
                x_ess = x_opt + cumul_offset
                if x_ess > X_arr[-1]:
                    break
                ax.annotate(
                    "",
                    xy=(x_ess, max(y_max * 0.02, 0)),
                    xytext=(x_ess, arrow_base + arrow_h * (load_idx * 0.35)),
                    arrowprops=dict(
                        arrowstyle="-|>",
                        color=color_load,
                        lw=1.6,
                        mutation_scale=10,
                    ),
                )
                ax.text(
                    x_ess,
                    arrow_base + arrow_h * (load_idx * 0.35 + 0.05),
                    f"{P:.0f} kN",
                    color=color_load,
                    fontsize=7,
                    ha="center",
                    va="bottom",
                )
            # Entrée légende via ligne fictive (mpatches.Patch est abstrait)
            ax.plot([], [], color=color_load, linewidth=3,
                    label=_load_label(load), alpha=0.9)

    if span_lengths:
        _add_span_boundaries(ax, span_lengths, 0, 0)

    _style_axes(ax, ylabel=ylabel)
    ax.legend(fontsize=7, facecolor="#111118", labelcolor="#CCCCCC",
              framealpha=0.7)
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    return fig


# ===========================================================================
#  6. Résumé de chargement sur une section (ponctuel + réparti + combiné)
# ===========================================================================

def plot_load_summary(
    X: List[float],
    BM: List[List[List[float]]],
    loading,
    span: int,
    section: int,
    *,
    title: str = "",
    figsize: Tuple[float, float] = (14, 12),
) -> Figure:
    """
    Résumé complet pour une section donnée :
    - L.I. de moment + charge ponctuelle optimale
    - L.I. de moment + charge répartie optimale
    - L.I. de moment + charge combinée

    Parameters
    ----------
    X : list
        Abscisses globales.
    BM : list
        Lignes d'influence du moment — shape [travée][section][alpha].
    loading : Loading
        Objet Loading déjà instancié.
    span : int
        Indice de la travée (0-indexé).
    section : int
        Indice de la section dans la travée.
    title : str, optional
    figsize : tuple, optional

    Returns
    -------
    Figure

    Example
    -------
    >>> from Tsaraloha.LIPoutreContinue.plot import plot_load_summary
    >>> fig = plot_load_summary(X, BM, ch, span=1, section=14)
    """
    li_curve = BM[span][section]
    li_arr   = np.asarray(li_curve, dtype=float)
    X_arr    = np.asarray(X, dtype=float)

    pt_loads   = loading.point_load_inputs
    dist_loads = loading.distrib_load_inputs

    # Calcul des positions optimales (point_load_inputs retourne des dicts)
    pt_results = []
    for ld in pt_loads:
        res = loading.plural_point_load(
            intensity=_ld_intensity(ld), length=_ld_length(ld),
            span=span, section=section,
        )
        pt_results.append(res)

    dist_results = []
    for ld in dist_loads:
        res = loading.plural_rectangular_load(
            intensity=_ld_intensity(ld), length=_ld_length(ld),
            span=span, section=section,
        )
        dist_results.append(res)

    combined = loading.combined_load_at(span=span, section=section)

    default_title = (
        title or
        f"Résumé de chargement — Travée {span}, Section {section}"
    )

    fig = plt.figure(figsize=figsize)
    _style_fig(fig, default_title)
    gs  = gridspec.GridSpec(3, 1, hspace=0.45, figure=fig)

    y_min, y_max = li_arr.min(), li_arr.max()
    span_y       = max(abs(y_max - y_min), 1e-6)
    arrow_base   = y_max + 0.08 * span_y
    arrow_h      = 0.10 * span_y

    # ── Panneau 1 : charges ponctuelles ───────────────────────────────────
    ax1 = fig.add_subplot(gs[0])
    _style_axes(ax1, ylabel="Moment [kN·m]",
                title="Charges ponctuelles — positions optimales")
    ax1.plot(X_arr, li_arr, color=_PALETTE[0], linewidth=_LINE_WIDTH,
             label="L.I. Moment")
    _fill_curve(ax1, X_arr, li_arr, _PALETTE[0])

    for idx, (ld, res) in enumerate(zip(pt_loads, pt_results)):
        alpha_opt = int(res["max_position"])
        x_opt     = X_arr[alpha_opt] if alpha_opt < len(X_arr) else X_arr[-1]
        color_ld  = _PALETTE[(idx + 2) % len(_PALETTE)]
        ax1.axvline(x_opt, color=color_ld, linewidth=1.0, linestyle="--",
                    alpha=0.7)
        cumul_offset = 0.0
        for ess_idx, (P, gap) in enumerate(zip(_ld_intensity(ld), _ld_length(ld))):
            if ess_idx > 0:
                cumul_offset += gap
            x_ess = x_opt + cumul_offset
            if x_ess > X_arr[-1]:
                break
            ax1.annotate(
                "",
                xy=(x_ess, max(y_max * 0.02, 0)),
                xytext=(x_ess, arrow_base + arrow_h * idx * 0.4),
                arrowprops=dict(arrowstyle="-|>", color=color_ld, lw=1.5,
                                mutation_scale=9),
            )
            ax1.text(
                x_ess,
                arrow_base + arrow_h * (idx * 0.4 + 0.05),
                f"{P:.0f} kN",
                color=color_ld, fontsize=7, ha="center", va="bottom",
            )
        # Entrée légende via ligne invisible
        ax1.plot([], [], color=color_ld, linewidth=2,
                 label=f"{_ld_name(ld)}  M={float(res['value']):.1f} kN·m")

    ax1.legend(fontsize=7, facecolor="#111118", labelcolor="#CCCCCC",
               framealpha=0.7)
    ax1.axhline(0, **_ZERO_STYLE)

    # ── Panneau 2 : charges réparties ─────────────────────────────────────
    ax2 = fig.add_subplot(gs[1])
    _style_axes(ax2, ylabel="Moment [kN·m]",
                title="Charges réparties — positions optimales")
    ax2.plot(X_arr, li_arr, color=_PALETTE[0], linewidth=_LINE_WIDTH,
             label="L.I. Moment")
    _fill_curve(ax2, X_arr, li_arr, _PALETTE[0])

    for idx, (ld, res) in enumerate(zip(dist_loads, dist_results)):
        alpha_opt  = int(res["max_position"])
        x_opt      = X_arr[alpha_opt] if alpha_opt < len(X_arr) else X_arr[-1]
        ld_length  = _ld_length(ld)
        x_end      = x_opt + (ld_length[-1] if len(ld_length) > 1 else 0)
        x_end      = min(x_end, X_arr[-1])
        color_ld   = _PALETTE[(idx + 2) % len(_PALETTE)]
        rect_h     = arrow_h * 0.85
        ax2.barh(
            arrow_base + rect_h / 2 + idx * rect_h * 1.5,
            x_end - x_opt,
            height=rect_h,
            left=x_opt,
            color=color_ld,
            alpha=0.55,
            hatch="///",
            edgecolor=color_ld,
        )
        ax2.text(
            (x_opt + x_end) / 2,
            arrow_base + rect_h * 1.1 + idx * rect_h * 1.5,
            f"{_ld_name(ld)}  {_ld_intensity(ld)[0]:.0f} kN/m  "
            f"M={float(res['value']):.1f} kN·m",
            color=color_ld, fontsize=7, ha="center", va="bottom",
        )

    ax2.plot([], [], color=_PALETTE[0], linewidth=0,
             label="Répartie (rectangle hachuré)")
    ax2.legend(fontsize=7, facecolor="#111118", labelcolor="#CCCCCC",
               framealpha=0.7)
    ax2.axhline(0, **_ZERO_STYLE)

    # ── Panneau 3 : combinée — toutes les forces affichées ────────────────
    comb_val = float(combined["value"])
    x_comb   = float(combined["position"])
    addition = combined.get("addition", {})

    ax3 = fig.add_subplot(gs[2])
    _style_axes(ax3, ylabel="Moment [kN·m]",
                title=f"Charge combinée — M total = {comb_val:.2f} kN·m "
                      f"@ x = {x_comb:.2f} m")
    ax3.plot(X_arr, li_arr, color=_PALETTE[0], linewidth=_LINE_WIDTH,
             label="L.I. Moment")
    _fill_curve(ax3, X_arr, li_arr, _PALETTE[0])

    # Ligne + point position combinée
    idx_comb = int(np.argmin(np.abs(X_arr - x_comb)))
    y_comb   = float(li_arr[idx_comb])
    ax3.axvline(x_comb, color="#FFDD77", linewidth=1.2, linestyle="--",
                alpha=0.85)
    ax3.scatter([x_comb], [y_comb], color="#FFDD77", s=70, zorder=6)

    # Calcul des dimensions des flèches
    y_min3, y_max3 = li_arr.min(), li_arr.max()
    span_y3  = max(abs(y_max3 - y_min3), 1e-6)
    ab3      = y_max3 + 0.08 * span_y3   # arrow_base
    ah3      = 0.11 * span_y3             # arrow_height step
    seg_col  = ["#4CE8A0", "#1DC46E", "#0E7A43", "#7AFFC4"]

    load_colors = {}   # nom -> couleur pour la légende
    row = 0            # rang vertical pour empiler les flèches

    for load_name, contrib in addition.items():
        # Déterminer si c'est une charge ponctuelle ou répartie
        # par comparaison avec les listes d'entrée
        is_rect = any(
            _ld_name(ld) == load_name for ld in dist_loads
        )
        color_ld = _PALETTE[(list(addition.keys()).index(load_name) + 2)
                             % len(_PALETTE)]
        load_colors[load_name] = color_ld

        comb_total = sum(
            v for k, v in contrib.items() if k != "Position"
        )

        if is_rect:
            # ── Charge répartie : récupérer la définition ──────────────
            ld_def = next(
                (ld for ld in dist_loads if _ld_name(ld) == load_name),
                None,
            )
            if ld_def is None:
                continue
            ld_len_def = _ld_length(ld_def)
            ld_int_def = _ld_intensity(ld_def)
            int_max_c  = max(ld_int_def) if ld_int_def else 1.0
            rect_h_ref = ah3 * 0.8

            cumul_seg = 0.0
            for si, (seg_q, seg_l) in enumerate(
                zip(ld_int_def, ld_len_def[1:])
            ):
                x_s = x_comb + cumul_seg
                x_e = min(x_s + seg_l, X_arr[-1])
                sc  = seg_col[si % len(seg_col)]
                cumul_seg += seg_l

                # Zone LI colorée
                inner_c = (X_arr > x_s) & (X_arr < x_e)
                X_sc = np.concatenate([[x_s], X_arr[inner_c], [x_e]])
                y_sc_s = float(np.interp(x_s, X_arr, li_arr))
                y_sc_e = float(np.interp(x_e, X_arr, li_arr))
                Y_sc = np.concatenate([[y_sc_s], li_arr[inner_c], [y_sc_e]])
                ax3.fill_between(X_sc, Y_sc, 0,
                                 color=sc, alpha=0.45, zorder=3)
                ax3.plot(X_sc, Y_sc, color=sc,
                         linewidth=_LINE_WIDTH + 0.4, zorder=4)
                ax3.plot([x_s, x_s], [0, y_sc_s],
                         color=sc, linewidth=1.6, zorder=4)
                ax3.plot([x_e, x_e], [0, y_sc_e],
                         color=sc, linewidth=1.6, zorder=4)

                # Bloc hachuré proportionnel
                rh = rect_h_ref * (seg_q / int_max_c)
                ax3.barh(ab3 + rh / 2 + row * ah3 * 0.5,
                         x_e - x_s, height=rh, left=x_s,
                         color=sc, alpha=0.55, hatch="///",
                         edgecolor=sc)
                ax3.text((x_s + x_e) / 2,
                         ab3 + rh + row * ah3 * 0.5,
                         f"{seg_q:.0f} kN/m",
                         color=sc, fontsize=6.5,
                         ha="center", va="bottom")
            row += 1
            ax3.plot([], [], color=color_ld, linewidth=3,
                     label=f"{load_name}  M = {comb_total:+.1f} kN·m")

        else:
            # ── Charge ponctuelle : flèche par essieu ──────────────────
            ld_def = next(
                (ld for ld in pt_loads if _ld_name(ld) == load_name),
                None,
            )
            if ld_def is None:
                continue
            ld_int_def = _ld_intensity(ld_def)
            ld_len_def = _ld_length(ld_def)

            cumul_pt = 0.0
            for ei, (P, gap) in enumerate(
                zip(ld_int_def, ld_len_def)
            ):
                if ei > 0:
                    cumul_pt += gap
                x_ess = x_comb + cumul_pt
                if x_ess > X_arr[-1]:
                    break
                y_arrow_top = ab3 + ah3 * (row * 0.55)
                ax3.annotate(
                    "",
                    xy=(x_ess, max(y_comb * 0.05, 0)),
                    xytext=(x_ess, y_arrow_top),
                    arrowprops=dict(
                        arrowstyle="-|>", color=color_ld,
                        lw=1.6, mutation_scale=10,
                    ),
                )
                ax3.text(
                    x_ess, y_arrow_top + ah3 * 0.04,
                    f"{P:.0f} kN",
                    color=color_ld, fontsize=7,
                    ha="center", va="bottom",
                )
            row += 1
            ax3.plot([], [], color=color_ld, linewidth=2,
                     label=f"{load_name}  M = {comb_total:+.1f} kN·m")

    # Annotation valeur totale
    offset_txt = (X_arr[-1] - X_arr[0]) * 0.04
    ax3.annotate(
        f"ΣM = {comb_val:.2f} kN·m",
        xy=(x_comb, y_comb),
        xytext=(x_comb + offset_txt, y_comb + span_y3 * 0.1),
        color="#FFDD77", fontsize=8, fontweight="bold",
        arrowprops=dict(arrowstyle="->", color="#FFDD77", lw=1.2),
    )

    ax3.legend(fontsize=7, facecolor="#111118", labelcolor="#CCCCCC",
               framealpha=0.7)
    ax3.axhline(0, **_ZERO_STYLE)

    fig.align_ylabels([ax1, ax2, ax3])
    plt.tight_layout(rect=[0, 0, 1, 0.97])
    return fig


# ===========================================================================
#  7. Vue synthétique complète depuis un objet Output
# ===========================================================================

def plot_output_full(
    out,
    *,
    figsize: Tuple[float, float] = (14, 10),
    title: str = "",
) -> Figure:
    """
    Vue synthétique complète depuis un objet **Output** déjà calculé
    (après ``out.compute()``).

    Panneaux :
    - Ligne 1 : moments sur appuis (L.I. hyperstatique)
    - Ligne 2 : L.I. Moment mi-travée de chaque travée
    - Ligne 3 : L.I. Effort tranchant mi-travée de chaque travée
    - Ligne 4 : L.I. Flèche mi-travée de chaque travée

    Parameters
    ----------
    out : Output
        Objet Output après ``compute()``.
    figsize : tuple, optional
    title : str, optional

    Returns
    -------
    Figure

    Example
    -------
    >>> from Tsaraloha.LIPoutreContinue.plot import plot_output_full
    >>> out = lipc.Output(E=[30e9]*3, I=[I]*3, L=[10,14,10], steps=0.5)
    >>> out.compute()
    >>> fig = plot_output_full(out)
    >>> plt.show()
    """
    X      = out.X
    BM     = out.BM
    SF     = out.SF
    Def    = out.Def
    SM     = out.support_moment
    spans  = out.L_spans
    n      = out.number_of_spans
    X_arr  = np.asarray(X, dtype=float)

    default_title = (
        title or
        f"Vue synthétique — Poutre continue {n} travées "
        f"({' + '.join(f'{l:.0f} m' for l in spans)})"
    )

    fig = plt.figure(figsize=figsize)
    _style_fig(fig, default_title)
    gs  = gridspec.GridSpec(4, 1, hspace=0.50, figure=fig)

    # ── Panneau 1 : moments sur appuis ────────────────────────────────────
    ax1 = fig.add_subplot(gs[0])
    _style_axes(ax1, ylabel="M appui [kN·m]",
                title="L.I. Moments sur appuis")
    for k, sm in enumerate(SM):
        c = _PALETTE[k % len(_PALETTE)]
        ax1.plot(X_arr, sm, color=c, linewidth=_LINE_WIDTH,
                 label=f"Appui {k}")
        _fill_curve(ax1, X_arr, sm, c)
    _add_span_boundaries(ax1, spans, 0, 0)
    ax1.legend(fontsize=7, facecolor="#111118", labelcolor="#CCCCCC",
               framealpha=0.7)

    # ── Panneau 2 : moment fléchissant mi-travée ──────────────────────────
    ax2 = fig.add_subplot(gs[1], sharex=ax1)
    _style_axes(ax2, ylabel="M [kN·m]",
                title="L.I. Moment fléchissant — mi-travée")
    for i, bm_span in enumerate(BM):
        mid = len(bm_span) // 2
        c   = _PALETTE[i % len(_PALETTE)]
        ax2.plot(X_arr, bm_span[mid], color=c, linewidth=_LINE_WIDTH,
                 label=f"Travée {i}")
        _fill_curve(ax2, X_arr, bm_span[mid], c)
    _add_span_boundaries(ax2, spans, 0, 0)
    ax2.legend(fontsize=7, facecolor="#111118", labelcolor="#CCCCCC",
               framealpha=0.7)

    # ── Panneau 3 : effort tranchant mi-travée ────────────────────────────
    ax3 = fig.add_subplot(gs[2], sharex=ax1)
    _style_axes(ax3, ylabel="V [kN]",
                title="L.I. Effort tranchant — mi-travée")
    SF_absc = getattr(out, "shear_force_all_abscissa", None)
    for i, sf_span in enumerate(SF):
        mid = len(sf_span) // 2
        c   = _PALETTE[i % len(_PALETTE)]
        if SF_absc and i < len(SF_absc) and mid < len(SF_absc[i]):
            x_sf = np.asarray(SF_absc[i][mid], dtype=float)
        elif len(sf_span[mid]) == len(X_arr):
            x_sf = X_arr
        else:
            x_sf = np.linspace(X_arr[0], X_arr[-1], len(sf_span[mid]))
        ax3.plot(x_sf, sf_span[mid], color=c, linewidth=_LINE_WIDTH,
                 label=f"Travée {i}")
        _fill_curve(ax3, x_sf, sf_span[mid], c)
    _add_span_boundaries(ax3, spans, 0, 0)
    ax3.legend(fontsize=7, facecolor="#111118", labelcolor="#CCCCCC",
               framealpha=0.7)

    # ── Panneau 4 : flèche mi-travée ──────────────────────────────────────
    ax4 = fig.add_subplot(gs[3], sharex=ax1)
    _style_axes(ax4, ylabel="w [m/kN]",
                title="L.I. Flèche — mi-travée")
    for i, def_span in enumerate(Def):
        mid = len(def_span) // 2
        c   = _PALETTE[i % len(_PALETTE)]
        ax4.plot(X_arr, def_span[mid], color=c, linewidth=_LINE_WIDTH,
                 label=f"Travée {i}")
        _fill_curve(ax4, X_arr, def_span[mid], c)
    _add_span_boundaries(ax4, spans, 0, 0)
    ax4.set_xlabel("Abscisse globale x [m]", color="#AAAAAA", fontsize=8)
    ax4.legend(fontsize=7, facecolor="#111118", labelcolor="#CCCCCC",
               framealpha=0.7)

    fig.align_ylabels([ax1, ax2, ax3, ax4])
    plt.tight_layout(rect=[0, 0, 1, 0.97])
    return fig


# ===========================================================================
#  API publique
# ===========================================================================

__all__ = [
    "plot_isostatique_influence_lines",
    "plot_hyperstatique_influence_lines",
    "plot_support_moments",
    "plot_bm_envelopes",
    "plot_load_on_influence_line",
    "plot_load_summary",
    "plot_output_full",
]
