from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Sequence

try:
    from json_io.json_loader import open_json
except ImportError:  # pragma: no cover
    from json_io.json_loader import open_json  # type: ignore


@dataclass(frozen=True)
class PlotContext:
    x_normal: Sequence[float]
    x_forces: Sequence
    nodes: Sequence[float]
    distances: List[str]


def load_plot_context(base_dir: Optional[Path] = None) -> PlotContext:
    x_normal = open_json("abscisse.json", base_dir=base_dir)
    x_forces = open_json("sheard abscisse.json", base_dir=base_dir)
    nodes = open_json("noeud lengths.json", base_dir=base_dir)
    distances = [f"{round(v, 5)}" for v in nodes]
    return PlotContext(x_normal=x_normal, x_forces=x_forces, nodes=nodes, distances=distances)