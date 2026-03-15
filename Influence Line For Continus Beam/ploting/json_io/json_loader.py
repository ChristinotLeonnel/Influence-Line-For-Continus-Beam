from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Optional

# Support both package import (`from .data_paths`) and running as scripts in-folder.
try:
    from json_io.data_paths import influence_line_dir
except ImportError:  # pragma: no cover
    from json_io.data_paths import influence_line_dir  # type: ignore


def open_json(
    filename: str = "abscisse.json",
    folder: str = "Curves",
    base_dir: Optional[Path] = None,
) -> Any:
    root = base_dir or influence_line_dir()
    file_path = root / folder / filename
    with open(file_path, "r", encoding="utf-8") as file:
        return json.load(file)