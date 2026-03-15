from __future__ import annotations

"""
Common helpers used by plotting scripts.

This module intentionally avoids import-time I/O. Call `get_context()` to load
the geometry/abscissas when needed.
"""

import json
from copy import deepcopy
from pathlib import Path
from typing import Optional

try:
    from plots.plot_context import PlotContext, load_plot_context
    from json_io.json_loader import open_json
except ImportError:  # pragma: no cover
    from plots.plot_context import PlotContext, load_plot_context  # type: ignore
    from json_io.json_loader import open_json  # type: ignore

try:
    from json_io.data_paths import influence_line_dir
except ImportError:  # pragma: no cover
    from json_io.data_paths import influence_line_dir  # type: ignore


DEFAULT_CONFIG = {
    "grid": True,
    "travee": True,
    "noeud": True,
    "vitesse": 10,
    "vitesse_bridge": 0.005,
    "legend": True,
    "axe_y_inverser": False,
    "default_matplotlib_color": True,
    "default_matplotlib_style": True,
    "style": {
        "line_color": "royalblue",
        "grid_color": "#E0E0E0",
        "minor_grid_color": "#F5F5F5",
        "noeud_color": "#FF4444",
        "noeud_size": 100,
        "line_width": 2.5,
        "line_style": "-",
        "background_color": "#FFFFFF",
        "edge_color": "#2C3E50",
        "shadow_color": "gray",
        "shadow_alpha": 0.3,
        "title": "",
        "xlabel": "Length des travées",
        "ylabel": "Valeur",
        "axis_fontsize": 12,
        "font_family": "Times New Roman",
        "legend_position": "best",
        "marker_style": "o",
        "legend_fontsize": 10,
    },
}

text_format = {"family": "Times New Roman", "size": 20}


def get_context(base_dir: Optional[Path] = None) -> PlotContext:
    return load_plot_context(base_dir=base_dir)


PLOT_CONFIG_FILENAME = "plot_config.json"


def _deep_update(base: dict, overrides: dict) -> dict:
    """
    Met à jour récursivement un dictionnaire de base avec des valeurs
    provenant d'un dictionnaire utilisateur.
    """
    for key, value in overrides.items():
        if isinstance(value, dict) and isinstance(base.get(key), dict):
            _deep_update(base[key], value)
        else:
            base[key] = value
    return base


def get_config_path(base_dir: Optional[Path] = None) -> Path:
    """
    Retourne le chemin complet du fichier de configuration de plot,
    placé dans le dossier qui contient les données (celui retourné
    par `influence_line_dir()`).
    """
    root = base_dir or influence_line_dir()
    return root / PLOT_CONFIG_FILENAME


def load_plot_config(base_dir: Optional[Path] = None) -> dict:
    """
    Lit le fichier de configuration de plot situé dans le dossier des
    données. S'il n'existe pas, il est créé avec `DEFAULT_CONFIG`.

    La configuration lue est fusionnée avec `DEFAULT_CONFIG` pour
    garantir que les nouvelles clés ont toujours une valeur par défaut.
    """
    path = get_config_path(base_dir)

    if path.exists():
        try:
            with path.open("r", encoding="utf-8") as f:
                user_cfg = json.load(f)
        except Exception:
            # Fichier corrompu ou illisible : repartir d'une config vide.
            user_cfg = {}
    else:
        user_cfg = {}
        path.parent.mkdir(parents=True, exist_ok=True)

    cfg = deepcopy(DEFAULT_CONFIG)
    if isinstance(user_cfg, dict):
        _deep_update(cfg, user_cfg)

    # Réécrit toujours le fichier pour s'assurer qu'il est à jour
    with path.open("w", encoding="utf-8") as f:
        json.dump(cfg, f, ensure_ascii=False, indent=2)

    return cfg


def save_plot_config(config: dict, base_dir: Optional[Path] = None) -> Path:
    """
    Enregistre explicitement une configuration de plot personnalisée
    dans le dossier des données.
    """
    path = get_config_path(base_dir)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(config, f, ensure_ascii=False, indent=2)
    return path