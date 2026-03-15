"""
plot_results.py — Tracé des résultats d'analyse structurelle.

BUG CORRIGÉ : ce module n'appelait jamais load_plot_config() et ignorait
totalement plot_config.json (grid, legend, style, font...).
Toutes les options du fichier de configuration sont maintenant respectées.
"""

from __future__ import annotations

import matplotlib.pyplot as plt
from matplotlib.font_manager import FontProperties
from pathlib import Path
from typing import Optional

from utils.plot_utils import get_context, load_plot_config, open_json

SAVE_DIR_PLOT = Path.home() / "Documents/Matrix One/Influence Line/Plots"
SAVE_DIR_PLOT.mkdir(parents=True, exist_ok=True)


# =============================================================================
#  Helpers
# =============================================================================

def _get_text_format(config: dict) -> dict:
    """Construit le dictionnaire de formatage texte depuis la config."""
    return {
        "family": config["style"]["font_family"],
        "size":   config["style"]["axis_fontsize"],
    }


def _apply_legend(ax, config: dict) -> None:
    """Ajoute la légende selon la config (position + taille de police)."""
    if config.get("legend", True):
        ax.legend(
            loc=config["style"]["legend_position"],
            prop=FontProperties(
                family=config["style"]["font_family"],
                size=config["style"]["legend_fontsize"],
            ),
        )


def _apply_style(fig, ax, config: dict) -> None:
    """Applique les couleurs de fond, grille et style général depuis la config."""
    style = config["style"]

    if not config["default_matplotlib_style"]:
        fig.patch.set_facecolor(style["background_color"])
        ax.set_facecolor(style["background_color"])
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)

    if config.get("grid", True):
        if config["default_matplotlib_style"]:
            ax.grid(True)
        else:
            ax.minorticks_on()
            ax.grid(which="major", color=style["grid_color"],       linewidth=1.0, alpha=0.8)
            ax.grid(which="minor", color=style["minor_grid_color"], linewidth=0.8, alpha=0.6)

    if config.get("axe_y_inverser", False):
        ax.invert_yaxis()


def _plot_line(ax, x, y, label: str, config: dict) -> None:
    """Trace une ligne en respectant le style de la config."""
    style = config["style"]
    if config["default_matplotlib_style"]:
        ax.plot(x, y, label=label)
    else:
        ax.plot(x, y,
                color=style["line_color"],
                linewidth=style["line_width"],
                linestyle=style["line_style"],
                label=label)


def _plot_nodes(ax, nodes, config: dict) -> None:
    """Trace les nœuds d'appui selon la config."""
    if not config.get("noeud", True):
        return
    style = config["style"]
    if config["default_matplotlib_style"]:
        ax.scatter(nodes, [0] * len(nodes))
    else:
        ax.scatter(nodes, [0] * len(nodes),
                   color=style["noeud_color"],
                   s=style["noeud_size"],
                   zorder=5,
                   marker=style["marker_style"])


# =============================================================================
#  Configuration des axes
# =============================================================================

def setup_plot_axes(ax, fig, nodes, distances, x_normal, config: dict) -> None:
    """Configure les axes, la grille, les nœuds et la ligne de référence."""
    _apply_style(fig, ax, config)

    ax.set_xticks(nodes)
    ax.set_xticklabels(distances,
                       fontdict={"family": config["style"]["font_family"],
                                 "size":   config["style"]["axis_fontsize"]})
    plt.setp(ax.get_xticklabels(), rotation=45, ha="right")

    if config.get("travee", True):
        if config["default_matplotlib_style"]:
            ax.plot(x_normal, [0] * len(x_normal), "k--", alpha=0.5)
        else:
            ax.plot(x_normal, [0] * len(x_normal),
                    color=config["style"]["edge_color"],
                    linestyle="--",
                    linewidth=config["style"]["line_width"] / 2,
                    alpha=0.5)

    _plot_nodes(ax, nodes, config)


# =============================================================================
#  Fonctions de tracé
# =============================================================================

def plot_single_result(result_type: str = "Bending Moment.json",
                       span: int = 0,
                       section: int = 0,
                       show_legend: bool = True,
                       config: Optional[dict] = None,
                       ctx=None) -> None:
    """Trace un seul résultat pour une travée et section données."""
    if config is None:
        config = load_plot_config()
    if ctx is None:
        ctx = get_context()

    text_fmt = _get_text_format(config)
    fig  = plt.gcf()
    ax   = plt.gca()
    setup_plot_axes(ax, fig, ctx.nodes, ctx.distances, ctx.x_normal, config)

    name = result_type.split(".")[0]

    if result_type in ["Deflection.json", "Bending Moment.json", "Rotation.json"]:
        _plot_line(ax, ctx.x_normal, open_json(result_type)[span][section],
                   label=f"Travée : {span+1}\nSection : {section}", config=config)

    elif result_type == "Shear Force.json":
        _plot_line(ax, ctx.x_forces[span][section], open_json(result_type)[span][section],
                   label=f"Travée : {span+1}\nSection : {section}", config=config)

    elif result_type == "Support Moment.json":
        _plot_line(ax, ctx.x_normal, open_json(result_type)[span],
                   label=f"M_{span}", config=config)

    elif result_type == "support_reactions":
        _plot_line(ax, ctx.x_forces[span][0], open_json(result_type)[span],
                   label=f"R_{span}", config=config)

    if show_legend:
        title = config["style"].get("title") or name.upper()
        ax.set_title(title, fontdict=text_fmt)
        ax.set_xlabel("Distance entre les appuis".upper(), fontdict=text_fmt)
        ax.set_ylabel(f"Values of {name}".upper(), fontdict=text_fmt)
        _apply_legend(ax, config)


def plot_shear_forces(span=None, section=None, show_legend: bool = True,
                      config: Optional[dict] = None, ctx=None) -> None:
    """Trace les efforts tranchants selon les paramètres."""
    if config is None:
        config = load_plot_config()
    if ctx is None:
        ctx = get_context()

    text_fmt  = _get_text_format(config)
    fig = plt.gcf()
    ax  = plt.gca()
    setup_plot_axes(ax, fig, ctx.nodes, ctx.distances, ctx.x_normal, config)

    shear_data = open_json("Shear Force.json")
    name = "Shear Force"

    if span is None and section is None:
        for i, (x_coords, forces) in enumerate(zip(ctx.x_forces, shear_data)):
            for j, (x, force) in enumerate(zip(x_coords, forces)):
                lbl = f"T{i+1} · S{j}" if j == 0 else "_nolegend_"
                _plot_line(ax, x, force, label=lbl, config=config)
        ax.set_title("All Spans - All Sections", fontdict=text_fmt)

    elif span is not None and section is None:
        for j, (x, force) in enumerate(zip(ctx.x_forces[span], shear_data[span])):
            _plot_line(ax, x, force, label=f"Section {j}", config=config)
        ax.set_title(f"All Sections for Span {span+1}", fontdict=text_fmt)

    elif span is None and section is not None:
        for i in range(len(ctx.nodes) - 1):
            _plot_line(ax, ctx.x_forces[i][section], shear_data[i][section],
                       label=f"Travée {i+1} :: Section {section}", config=config)
        ax.set_title(f"All Spans for Section {section}", fontdict=text_fmt)

    else:
        plot_single_result("Shear Force.json", span, section, show_legend, config, ctx)
        return

    if show_legend:
        ax.set_xlabel("Distance entre les appuis".upper(), fontdict=text_fmt)
        ax.set_ylabel(f"Values of {name}".upper(), fontdict=text_fmt)
        _apply_legend(ax, config)


def plot_distributed_results(result_type: str = "Bending Moment.json",
                              span=None, section=None,
                              show_legend: bool = True,
                              config: Optional[dict] = None,
                              ctx=None) -> None:
    """Trace les résultats pour moment, déflexion ou rotation."""
    if config is None:
        config = load_plot_config()
    if ctx is None:
        ctx = get_context()

    text_fmt = _get_text_format(config)
    fig = plt.gcf()
    ax  = plt.gca()
    setup_plot_axes(ax, fig, ctx.nodes, ctx.distances, ctx.x_normal, config)

    data = open_json(result_type)
    name = result_type.split(".")[0]

    if span is None and section is None:
        for i, span_data in enumerate(data):
            for j, section_data in enumerate(span_data):
                lbl = f"Travée {i+1} · Section {j}" if j == 0 else "_nolegend_"
                _plot_line(ax, ctx.x_normal, section_data, label=lbl, config=config)
        ax.set_title("All Spans - All Sections", fontdict=text_fmt)

    elif span is not None and section is None:
        for j, section_data in enumerate(data[span]):
            _plot_line(ax, ctx.x_normal, section_data,
                       label=f"Section {j}", config=config)
        ax.set_title(f"All Sections for Span {span+1}", fontdict=text_fmt)

    elif span is None and section is not None:
        for i in range(len(ctx.nodes) - 1):
            _plot_line(ax, ctx.x_normal, data[i][section],
                       label=f"Travée {i+1} :: Section {section}", config=config)
        ax.set_title(f"All Spans for Section {section}", fontdict=text_fmt)

    else:
        plot_single_result(result_type, span, section, show_legend, config, ctx)
        return

    if show_legend:
        ax.set_xlabel("Distance entre les appuis".upper(), fontdict=text_fmt)
        ax.set_ylabel(f"Values of {name}".upper(), fontdict=text_fmt)
        _apply_legend(ax, config)


def plot_support_moments(span=None, show_legend: bool = True,
                         exclude_boundaries: bool = True,
                         config: Optional[dict] = None,
                         ctx=None) -> None:
    """Trace les moments aux appuis."""
    if config is None:
        config = load_plot_config()
    if ctx is None:
        ctx = get_context()

    text_fmt = _get_text_format(config)
    fig = plt.gcf()
    ax  = plt.gca()
    setup_plot_axes(ax, fig, ctx.nodes, ctx.distances, ctx.x_normal, config)

    data = open_json("Support Moment.json")

    if span is None:
        indices = range(1, len(ctx.nodes) - 1) if exclude_boundaries else range(len(data))
        for i in indices:
            _plot_line(ax, ctx.x_normal, data[i], label=f"M_{i}", config=config)
    else:
        plot_single_result("Support Moment.json", span, show_legend=show_legend, config=config, ctx=ctx)
        return

    if show_legend:
        _apply_legend(ax, config)


def plot_maximum_values(result_type: str = "Bending Moment.json",
                        config: Optional[dict] = None,
                        ctx=None) -> None:
    """Trace la courbe correspondant à la position du maximum absolu."""
    if config is None:
        config = load_plot_config()
    if ctx is None:
        ctx = get_context()

    max_positions = open_json(f"{result_type.split('.')[0]} Max Positions.json", "Analysis")

    if result_type == "Support Moment.json":
        plot_single_result(result_type, span=max_positions["span"],
                           config=config, ctx=ctx)
    else:
        plot_single_result(result_type,
                           span=max_positions["span"],
                           section=max_positions["section"],
                           config=config, ctx=ctx)


# =============================================================================
#  Point d'entrée principal
# =============================================================================

def plot_analysis_results(
    result_type: str = "Bending Moment.json",
    span=None,
    section=None,
    show_legend: bool = True,
    show_maximum: bool = False,
    exclude_boundaries: bool = True,
    is_show: bool = False,
    is_save: bool = False,
    save_dir: Path = SAVE_DIR_PLOT,
) -> None:
    """Fonction principale : trace tous types de résultats d'analyse structurelle.

    Charge la configuration depuis plot_config.json à chaque appel pour
    garantir que les modifications du fichier sont immédiatement reflétées.

    Args:
        result_type:        Fichier JSON à tracer.
        span:               Indice de travée (None = toutes).
        section:            Indice de section (None = toutes).
        show_legend:        Affiche la légende.
        show_maximum:       Trace uniquement la courbe du maximum absolu.
        exclude_boundaries: Exclut les appuis extérieurs pour Support Moment.
        is_show:            Affiche la fenêtre matplotlib.
        is_save:            Sauvegarde le plot en PNG.
        save_dir:           Répertoire de sauvegarde.
    """
    # Chargement unique config + contexte pour tout l'appel
    config = load_plot_config()
    ctx    = get_context()

    plt.clf()
    fig, ax = plt.subplots(figsize=(10, 6))
    plt.sca(ax)

    if show_maximum:
        plot_maximum_values(result_type, config, ctx)
    elif result_type == "Shear Force.json":
        plot_shear_forces(span, section, show_legend, config, ctx)
    elif result_type == "Support Moment.json":
        plot_support_moments(span, show_legend, exclude_boundaries, config, ctx)
    elif result_type in ["Deflection.json", "Bending Moment.json", "Rotation.json"]:
        plot_distributed_results(result_type, span, section, show_legend, config, ctx)
    elif result_type == "support_reactions":
        plot_single_result(result_type, span=span or 0,
                           show_legend=show_legend, config=config, ctx=ctx)

    fig.tight_layout()

    if is_save:
        Path(save_dir).mkdir(parents=True, exist_ok=True)
        name = result_type.split(".")[0]
        fig.savefig(str(Path(save_dir) / f"{name}.png"), dpi=300, bbox_inches="tight")

    if is_show:
        plt.show()

    plt.close(fig)


if __name__ == "__main__":
    plot_analysis_results("Bending Moment.json",
                          show_maximum=True,
                          is_show=True,
                          is_save=False)