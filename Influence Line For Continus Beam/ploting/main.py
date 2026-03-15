"""
main.py — Pipeline de génération des résultats visuels (plots + animations).

Flux d'exécution :
  1. Animations GIF + MP4 des courbes structurelles  [PARALLÈLE - Threads]
  2. Plots statiques par courbe                       [PARALLÈLE - Threads]
  3. Animations de courbure GIF + MP4                [PARALLÈLE - Threads]

NOTE : ThreadPoolExecutor est utilisé à la place de ProcessPoolExecutor
       car PyInstaller (one-file) sur Windows ne supporte pas le spawn
       de sous-processus depuis un .exe — cela génère une boucle infinie.
       Les tâches étant I/O-bound (écriture GIF/MP4), les threads suffisent.
"""

import sys
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

_ROOT = Path(__file__).resolve().parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from animations.animate_results import (
    ALL_CURVES,
    SAVE_DIR_GIF,
    SAVE_DIR_MP4,
    build_and_save_animation,
    convert_gif_to_mp4,
)
from plots.plot_results import plot_analysis_results

import matplotlib
matplotlib.use("Agg")  # Backend non-interactif — indispensable en multiprocessing
import matplotlib.pyplot as plt

# =============================================================================
#  Répertoires de sortie
# =============================================================================

_BASE = Path.home() / "Documents/Matrix One/Influence Line"

SAVE_DIR_PLOT          = _BASE / "Plots"
SAVE_DIR_CURVATURE     = _BASE / "Animation/Curvature/GIF"
SAVE_DIR_CURVATURE_MP4 = _BASE / "Animation/Curvature/MP4"

for _dir in (SAVE_DIR_PLOT / "Maximum",
             SAVE_DIR_PLOT / "All",
             SAVE_DIR_GIF,
             SAVE_DIR_MP4,
             SAVE_DIR_CURVATURE,
             SAVE_DIR_CURVATURE_MP4):
    _dir.mkdir(parents=True, exist_ok=True)


# =============================================================================
#  Utilitaire
# =============================================================================

def _curve_stem(curve: str) -> str:
    return Path(curve).stem


# =============================================================================
#  Workers (fonctions top-level, sérialisables par pickle)
# =============================================================================

def _worker_animation(curve: str) -> tuple[str, str | None]:
    """Phase 1 : génère GIF puis convertit en MP4."""
    name = _curve_stem(curve)
    try:
        build_and_save_animation(curve, is_save=True, is_show=False)
        convert_gif_to_mp4(
            str(SAVE_DIR_GIF / f"{name}.gif"),
            str(SAVE_DIR_MP4 / f"{name}.mp4"),
        )
        return name, None
    except Exception as e:
        return name, str(e)


def _worker_plot(curve: str) -> tuple[str, str | None]:
    """Phase 2 : génère les deux plots statiques."""
    name = _curve_stem(curve)
    try:
        plot_analysis_results(curve, show_maximum=True,
                              is_save=True, is_show=False,
                              save_dir=SAVE_DIR_PLOT / "Maximum")
        plot_analysis_results(curve,
                              is_save=True, is_show=False,
                              save_dir=SAVE_DIR_PLOT / "All")
        plt.close("all")
        return name, None
    except Exception as e:
        return name, str(e)


def _worker_curvature(curve: str) -> tuple[str, str | None]:
    """Phase 3 : génère l'animation de courbure GIF + MP4."""
    name = _curve_stem(curve)
    try:
        from animations.animate_single_curve import animate_curvature, ANIMATION_FPS
        from utils.plot_utils import open_json

        max_positions = open_json(f"{name} Max Positions.json", "Analysis")
        anim = animate_curvature(curve,
                                 span=max_positions["span"],
                                 section=max_positions["section"])

        gif_path = SAVE_DIR_CURVATURE / f"{name}.gif"
        anim.save(str(gif_path), writer="pillow", fps=ANIMATION_FPS)
        plt.close("all")

        convert_gif_to_mp4(str(gif_path),
                           str(SAVE_DIR_CURVATURE_MP4 / f"{name}.mp4"))
        return name, None
    except Exception as e:
        return name, str(e)


# =============================================================================
#  Exécuteur parallèle générique
# =============================================================================

def _run_parallel(phase_name: str, worker, curves: list[str]) -> None:
    """Lance `worker` sur chaque courbe en parallèle et affiche le résultat."""
    print(f"=== {phase_name} ===")
    # max_workers=None → autoscale sur nb de CPU disponibles
    with ThreadPoolExecutor(max_workers=len(curves)) as pool:
        futures = {pool.submit(worker, c): _curve_stem(c) for c in curves}
        for future in as_completed(futures):
            name, err = future.result()
            if err:
                print(f"    ERR  {name} : {err}")
            else:
                print(f"    OK   {name}")
    print(f"{phase_name} terminée.\n")


# =============================================================================
#  Pipeline principal
# =============================================================================

def run() -> None:
    _run_parallel("Phase 1 : Animations structurelles", _worker_animation, ALL_CURVES)
    _run_parallel("Phase 2 : Plots statiques",          _worker_plot,      ALL_CURVES)
    _run_parallel("Phase 3 : Animations de courbure",   _worker_curvature, ALL_CURVES)


if __name__ == "__main__":
    run()