"""
animate_results.py — Animations de toutes les courbes structurelles.

BUGS CORRIGÉS / AMÉLIORATIONS :
  - interval=0.005 ms → comportement imprévisible          [→ vitesse depuis config]
  - anim.save() sans fps/writer → Pillow 10 fps par défaut [→ fps depuis config]
  - DEFAULT_CONFIG.copy() superficiel → mutations          [→ load_plot_config()]
  - x_normal recalculé deux fois                           [supprimé]
  - config["vitesse"] ignoré                               [→ interval=1000/vitesse]
  - config["style"]["line_style"] ignoré                   [→ ax.plot(linestyle=...)]
  - config["style"]["grid_color"] ignoré                   [→ ax.grid avec couleur]
  - config["style"]["font_family"] ignoré                  [→ ax labels]
  - config["style"]["axis_fontsize"] ignoré                [→ ax labels]
  - config["style"]["legend_position"] ignoré              [→ ax.legend(loc=...)]
  - config["style"]["legend_fontsize"] ignoré              [→ ax.legend(prop=...)]
  - config["style"]["title"] ignoré                        [→ ax.set_title]
  - config["default_matplotlib_color"] ignoré              [→ color override logic]
"""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.font_manager import FontProperties


from utils.plot_utils import open_json, load_plot_config

# ── Courbes disponibles ───────────────────────────────────────────────────────
ALL_CURVES = [
    "Shear Force.json",
    "Bending Moment.json",
    "Deflection.json",
    "Rotation.json",
]

# ── Répertoires de sortie ─────────────────────────────────────────────────────
SAVE_DIR_GIF = Path.home() / "Documents/Matrix One/Influence Line/Animation/Results/GIF"
SAVE_DIR_MP4 = Path.home() / "Documents/Matrix One/Influence Line/Animation/Results/MP4"
SAVE_DIR_GIF.mkdir(parents=True, exist_ok=True)
SAVE_DIR_MP4.mkdir(parents=True, exist_ok=True)

_DEFAULT_FPS = 20


def _fps_from_config(config: dict) -> int:
    """Dérive le FPS depuis config["vitesse"]."""
    v = config.get("vitesse", _DEFAULT_FPS)
    return int(v) if isinstance(v, (int, float)) and v >= 1 else _DEFAULT_FPS


def _interval_from_config(config: dict) -> int:
    """Dérive l'interval matplotlib (ms) depuis config["vitesse"]."""
    return int(1000 / _fps_from_config(config))


# Exposé pour import externe
ANIMATION_FPS = _DEFAULT_FPS


# =============================================================================
def load_curve_data(Curve: str):
    """Charge et normalise les données X/Y pour une courbe donnée."""
    if Curve == "Shear Force.json":
        shear_abscissas = open_json("sheard abscisse.json")
        curvature = open_json(Curve)
        frames, x_coords = [], []
        for span_idx, span in enumerate(curvature):
            for section_idx, section in enumerate(span):
                frames.append(section)
                x_coords.append(shear_abscissas[span_idx][section_idx])
        x_normal = [np.array(x) - min(x) for x in x_coords]
    else:
        raw_x    = np.array(open_json())
        x_normal = raw_x - raw_x.min()
        curvature = open_json(Curve)
        frames = [section for span in curvature for section in span]

    return frames, x_normal


# =============================================================================
def build_and_save_animation(Curve: str,
                              is_save: bool = True,
                              is_show: bool = False) -> None:
    """Génère, affiche et/ou sauvegarde l'animation d'une courbe structurelle."""
    global ANIMATION_FPS
    print(f"\n>>> Traitement : {Curve}")

    frames, x_normal = load_curve_data(Curve)
    max_moment_data  = open_json(f"{Curve.split('.')[0]} Max Positions.json", "Analysis")

    # Chargement config depuis fichier
    config   = load_plot_config()
    style    = config["style"]
    neouds   = np.array(open_json("noeud lengths.json"))
    distances = [f"{round(n, 5)}" for n in neouds]

    absolute_max      = max_moment_data["value"]
    y_max             = absolute_max * 1.1
    y_min             = -absolute_max * 1.1
    is_shear          = Curve == "Shear Force.json"
    use_default       = config.get("default_matplotlib_style", True)
    use_default_color = config.get("default_matplotlib_color", True)

    animation_fps      = _fps_from_config(config)
    animation_interval = _interval_from_config(config)
    ANIMATION_FPS      = animation_fps

    text_fmt    = {"family": style["font_family"], "size": style["axis_fontsize"]}
    legend_prop = FontProperties(family=style["font_family"],
                                 size=style["legend_fontsize"])
    curve_name  = Curve.split(".")[0]

    # ── Helpers ───────────────────────────────────────────────────────────────
    def _set_xlabel(ax):
        ax.set_xticks(neouds)
        ax.set_xticklabels(distances, fontdict=text_fmt)
        plt.setp(ax.get_xticklabels(), rotation=45, ha="right")

    def _apply_grid(ax):
        if config.get("grid", True):
            if use_default:
                ax.grid(True)
            else:
                ax.minorticks_on()
                ax.grid(which="major", color=style["grid_color"],       linewidth=1.0, alpha=0.8)
                ax.grid(which="minor", color=style["minor_grid_color"], linewidth=0.8, alpha=0.6)

    def _xlim(ax):
        if is_shear:
            all_x = np.concatenate(x_normal)
            ax.set_xlim(float(all_x.min()), float(all_x.max()))
        else:
            ax.set_xlim(0, float(np.max(x_normal)))

    def _draw_structure(ax, frame_index):
        if config.get("travee", True):
            if is_shear:
                visible_x = np.concatenate(x_normal[:frame_index]) if frame_index > 0 else np.array([])
            else:
                visible_x = x_normal[:frame_index]
            if len(visible_x):
                kw = dict(linestyle="--", alpha=0.5, label="Travées")
                if not use_default:
                    kw |= dict(color=style["edge_color"],
                               linewidth=style["line_width"] / 2)
                ax.plot(visible_x, np.zeros(len(visible_x)), **kw)

        if config.get("noeud", True):
            if is_shear:
                ref_x = float(x_normal[frame_index - 1][-1]) if frame_index > 0 else 0.0
            else:
                ref_x = float(x_normal[frame_index - 1]) if frame_index > 0 else 0.0
            visible_nodes = neouds[neouds <= ref_x]
            if len(visible_nodes):
                kw = dict(label="Noeuds")
                if not use_default:
                    kw |= dict(color=style["noeud_color"],
                               s=style["noeud_size"],
                               marker=style["marker_style"],
                               zorder=5)
                ax.scatter(visible_nodes, [0] * len(visible_nodes), **kw)

    def _set_labels(ax):
        title  = style.get("title") or curve_name.upper()
        xlabel = "Longueur des travées".upper() if use_default else style["xlabel"]
        ylabel = f"Values of {curve_name}".upper() if use_default else style["ylabel"]
        ax.set_title(title,  fontdict=text_fmt)
        ax.set_xlabel(xlabel, fontdict=text_fmt)
        ax.set_ylabel(ylabel, fontdict=text_fmt)

    def _set_legend(ax):
        if config.get("legend", True):
            loc  = "center left" if use_default else style["legend_position"]
            bbox = (1, 0.5) if "left" in loc else None
            kw   = dict(loc=loc, prop=legend_prop)
            if bbox:
                kw["bbox_to_anchor"] = bbox
            ax.legend(**kw)
            plt.subplots_adjust(right=0.85)

    # ── Fonctions d'animation ─────────────────────────────────────────────────
    def init():
        ax.clear()
        _apply_grid(ax)
        _set_xlabel(ax)
        _xlim(ax)
        ax.set_ylim(y_min, y_max)
        if config.get("axe_y_inverser", False):
            ax.invert_yaxis()
        return (ax.plot([], [])[0],)

    def animate(frame_index: int):
        ax.clear()
        _apply_grid(ax)
        _set_xlabel(ax)
        _draw_structure(ax, frame_index)

        ax.axhline(y= absolute_max, color="r", linestyle="--", alpha=0.5,
                   label=f"Max: {absolute_max:.2f}")
        ax.axhline(y=-absolute_max, color="r", linestyle="--", alpha=0.5)

        if frame_index < len(frames):
            current_x = x_normal[frame_index] if is_shear else x_normal
            current_y = frames[frame_index]
            kw = dict(label=curve_name)
            if not use_default_color:
                kw |= dict(color=style["line_color"],
                           linewidth=style["line_width"],
                           linestyle=style["line_style"])
            ax.plot(current_x, current_y, **kw)

        _set_labels(ax)
        _xlim(ax)
        ax.set_ylim(y_min, y_max)
        if config.get("axe_y_inverser", False):
            ax.invert_yaxis()
        _set_legend(ax)
        return (ax.plot([], [])[0],)

    # ── Création figure ───────────────────────────────────────────────────────
    plt.close("all")
    fig, ax = plt.subplots(figsize=(10, 6))
    if not use_default:
        fig.patch.set_facecolor(style["background_color"])
        ax.set_facecolor(style["background_color"])

    anim = FuncAnimation(
        fig, animate,
        frames=len(frames),
        init_func=init,
        interval=animation_interval,
        blit=False,
        repeat=False,
    )

    if is_save:
        gif_path = SAVE_DIR_GIF / f"{curve_name}.gif"
        print(f"    Sauvegarde GIF ({animation_fps} fps) : {gif_path}")
        SAVE_DIR_GIF.mkdir(parents=True, exist_ok=True)
        anim.save(str(gif_path), writer="pillow", fps=animation_fps)

    if is_show:
        plt.show()

    plt.close(fig)
    print("    OK ✓")


# =============================================================================
def convert_gif_to_mp4(input_path: str, output_path: str) -> None:
    """Convertit un GIF en MP4 H.264 haute qualité via ffmpeg (subprocess).

    Remplace moviepy — ffmpeg doit être installé sur le système.
    """
    import subprocess
    cmd = [
        "ffmpeg", "-y",
        "-i", str(input_path),
        "-vcodec", "libx264",
        "-crf", "18",
        "-preset", "slow",
        "-pix_fmt", "yuv420p",
        "-an",
        str(output_path),
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"ffmpeg a échoué (code {result.returncode}):\n{result.stderr}"
        )


if __name__ == "__main__":
    build_and_save_animation("Bending Moment.json", is_save=False, is_show=True)