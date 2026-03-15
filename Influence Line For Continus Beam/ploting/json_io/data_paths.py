from __future__ import annotations

import os
from pathlib import Path


def influence_line_dir() -> Path:
    """
    Root folder that contains `Curves/` and `Analysis/`.

    Override with env var for portability:
    - MATRIX_ONE_INFLUENCE_LINE_DIR="C:\\path\\to\\Influence Line"
    """
    env = os.getenv("MATRIX_ONE_INFLUENCE_LINE_DIR")
    if env:
        return Path(env).expanduser()
    return Path.home() / "Documents" / "Matrix One" / "New Folder" / "Influence Line"

