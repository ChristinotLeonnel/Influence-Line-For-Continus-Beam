"""
animate_single_curve.py — Animation frame-par-frame d'une courbe structurelle.

AMÉLIORATIONS :
  - config["style"]["legend_fontsize"] ignoré  [→ ax.legend(prop=FontProperties)]
  - config["style"]["legend_position"] ignoré  [→ ax.legend(loc=...)]
  - config["style"]["marker_style"] ignoré     [→ scatter / plot marker]
  - config["style"]["line_style"] ignoré       [→ ax.plot(linestyle=...)]
  - config["style"]["shadow_*"] ignorés        [→ effet ombre sur la ligne]
  - config["style"]["title"] ignoré            [→ ax.set_title]
  - config["vitesse"] ignoré                   [→ interval = 1000/vitesse]
  - repeat=True → GIF tronqué                  [→ False]
"""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.font_manager import FontProperties

from utils.plot_utils import get_context, load_plot_config, open_json

SAVE_DIR = Path.home() / "Documents/Matrix One/Influence Line/Animation"
SAVE_DIR.mkdir(parents=True, exist_ok=True)

_DEFAULT_FPS  = 30
ANIMATION_FPS = _DEFAULT_FPS   # mis à jour dynamiquement, importable depuis main.py


def animate_curvature(Curve: str, span: int = 0, section: int = 0) -> FuncAnimation:
    """Crée une animation de la courbe d'influence pour une travée/section donnée.

    Respecte intégralement plot_config.json :
      vitesse, grid, noeud, travee, axe_y_inverser, default_matplotlib_style,
      default_matplotlib_color, style.* (toutes les clés).

    Args:
        Curve:   Nom du fichier JSON (ex: "Bending Moment.json").
        span:    Indice de la travée.
        section: Indice de la section.

    Returns:
        FuncAnimation prête à être sauvegardée :
            anim.save(path, writer='pillow', fps=ANIMATION_FPS)
    """
    global ANIMATION_FPS

    ctx    = get_context()
    config = load_plot_config()
    style  = config["style"]

    # ── FPS depuis config["vitesse"] ─────────────────────────────────────────
    v   = config.get("vitesse", _DEFAULT_FPS)
    fps = int(v) if isinstance(v, (int, float)) and v >= 1 else _DEFAULT_FPS
    ANIMATION_FPS = fps
    interval_ms   = int(1000 / fps)

    use_default       = config.get("default_matplotlib_style", True)
    use_default_color = config.get("default_matplotlib_color", True)

    # ── Données X/Y ───────────────────────────────────────────────────────────
    if Curve == "Shear Force.json":
        x_vals = np.asarray(ctx.x_forces[span][section], dtype=float)
    else:
        x_vals = np.asarray(ctx.x_normal, dtype=float)

    y_vals = np.asarray(open_json(Curve)[span][section], dtype=float)

    x0     = float(np.min(x_vals)) if len(x_vals) else 0.0
    x_vals = x_vals - x0
    nodes  = np.asarray(ctx.nodes, dtype=float) - x0
    distances = [f"{round(float(n), 5)}" for n in nodes]

    # ── Font properties ───────────────────────────────────────────────────────
    text_fmt    = {"family": style["font_family"], "size": style["axis_fontsize"]}
    legend_prop = FontProperties(family=style["font_family"],
                                 size=style["legend_fontsize"])

    # ── Figure & axes ─────────────────────────────────────────────────────────
    fig, ax = plt.subplots(figsize=(10, 6))
    if not use_default:
        fig.patch.set_facecolor(style["background_color"])
        ax.set_facecolor(style["background_color"])
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)

    # ── Grille ────────────────────────────────────────────────────────────────
    if config.get("grid", True):
        if use_default:
            ax.grid(True)
        else:
            ax.minorticks_on()
            ax.grid(which="major", color=style["grid_color"],       linewidth=1.0, alpha=0.8)
            ax.grid(which="minor", color=style["minor_grid_color"], linewidth=0.8, alpha=0.6)

    # ── Axes X ────────────────────────────────────────────────────────────────
    ax.set_xticks(nodes)
    ax.set_xticklabels(distances, fontdict=text_fmt)
    plt.setp(ax.get_xticklabels(), rotation=45, ha="right")

    # ── Titre et labels ───────────────────────────────────────────────────────
    curve_name = Curve.replace(".json", "")
    title = (style.get("title") or
             f"{curve_name.upper()} — Travée {span+1}, Section {section}")
    ax.set_title(title, fontdict=text_fmt)
    ax.set_xlabel("Distance entre les appuis".upper(), fontdict=text_fmt)
    ax.set_ylabel(curve_name.upper(), fontdict=text_fmt)

    # ── Ligne de référence (structure axis) ───────────────────────────────────
    if config.get("travee", True):
        ref_kw = dict(linestyle="--", linewidth=1.2, alpha=0.6)
        if not use_default:
            ref_kw["color"] = style["edge_color"]
        ax.axhline(0.0, **ref_kw)

    # ── Nœuds ─────────────────────────────────────────────────────────────────
    if config.get("noeud", True):
        scatter_kw = dict(zorder=5)
        if not use_default:
            scatter_kw |= dict(color=style["noeud_color"],
                               s=style["noeud_size"] * 0.35,
                               marker=style["marker_style"])
        ax.scatter(nodes, np.zeros_like(nodes), **scatter_kw)

    # ── Limites des axes ──────────────────────────────────────────────────────
    x_span = float(np.max(x_vals) - np.min(x_vals)) if len(x_vals) else 1.0
    y_min  = float(np.min(y_vals)) if len(y_vals) else -1.0
    y_max  = float(np.max(y_vals)) if len(y_vals) else  1.0
    y_span = y_max - y_min
    x_margin = x_span * 0.06 if x_span > 0 else 1.0
    y_margin = y_span * 0.10 if y_span > 0 else max(1.0, abs(y_max) * 0.1)

    ax.set_xlim(float(np.min(x_vals)) - x_margin, float(np.max(x_vals)) + x_margin)
    ax.set_ylim(y_min - y_margin, y_max + y_margin)

    if config.get("axe_y_inverser", False):
        ax.invert_yaxis()

    # ── Artistes animés ───────────────────────────────────────────────────────
    line_kw = dict(label=curve_name)
    if not use_default_color:
        line_kw |= dict(color=style["line_color"],
                        linewidth=style["line_width"],
                        linestyle=style["line_style"])
    (line,) = ax.plot([], [], **line_kw)

    point_kw = dict(zorder=6)
    point_kw["marker"] = style["marker_style"] if not use_default else "o"
    if not use_default_color:
        point_kw["color"]    = style["line_color"]
        point_kw["markersize"] = 6
    (point,) = ax.plot([], [], **point_kw)

    vline_kw = dict(linewidth=1.0, alpha=0.35)
    if not use_default:
        vline_kw["color"] = style["edge_color"]
    vline = ax.axvline(0.0, **vline_kw)

    # Ombre (shadow) sous la ligne animée si shadow_alpha > 0
    shadow_alpha = style.get("shadow_alpha", 0.0)
    shadow_line  = None
    if not use_default and shadow_alpha > 0:
        (shadow_line,) = ax.plot([], [],
                                 color=style.get("shadow_color", "gray"),
                                 linewidth=style["line_width"] * 2,
                                 alpha=shadow_alpha,
                                 zorder=0)

    # Légende avec position et taille de police depuis la config
    legend_loc = style["legend_position"] if not use_default else "best"
    ax.legend(loc=legend_loc, prop=legend_prop)

    # ── Fonctions d'animation ─────────────────────────────────────────────────
    def init():
        line.set_data([], [])
        point.set_data([], [])
        vline.set_xdata([0.0, 0.0])
        artists = [line, point, vline]
        if shadow_line is not None:
            shadow_line.set_data([], [])
            artists.append(shadow_line)
        return tuple(artists)

    def animate(frame_idx: int):
        x = x_vals[:frame_idx]
        y = y_vals[:frame_idx]
        line.set_data(x, y)
        if frame_idx > 0:
            point.set_data([x[-1]], [y[-1]])
            vline.set_xdata([x[-1], x[-1]])
        artists = [line, point, vline]
        if shadow_line is not None:
            shadow_line.set_data(x, y - y_margin * 0.05)
            artists.append(shadow_line)
        return tuple(artists)

    # BUG CORRIGÉ : repeat=False obligatoire pour durée GIF/MP4 correcte
    anim = FuncAnimation(
        fig,
        animate,
        init_func=init,
        frames=range(1, len(x_vals) + 1),
        interval=interval_ms,
        blit=True,
        repeat=False,
    )

    fig.tight_layout()
    return anim


if __name__ == "__main__":
    animation = animate_curvature("Bending Moment.json", span=2, section=16)
    out = SAVE_DIR / "Bending Moment Travée 3 Section 17.gif"
    animation.save(str(out), writer="pillow", fps=ANIMATION_FPS)
    plt.close()
    print(f"Animation sauvegardée : {out}")