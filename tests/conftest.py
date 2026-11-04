"""Shared fixtures.

The tests target the C++ engine (the reference implementation). They refuse
to run silently on the pure-Python fallback: set
TSARALOHA_ALLOW_PY_BACKEND=1 to run them on it deliberately.
"""
from __future__ import annotations

import os
import warnings

import pytest

with warnings.catch_warnings():
    warnings.simplefilter("ignore", RuntimeWarning)  # fallback warning, checked below
    import Tsaraloha.LIPoutreContinue as lipc


def pytest_report_header(config):
    return f"Tsaraloha.LIPoutreContinue {lipc.__version__} — backend: {lipc._BACKEND} ({lipc.__file__})"


def pytest_configure(config):
    if lipc._BACKEND != "cpp" and os.environ.get("TSARALOHA_ALLOW_PY_BACKEND") != "1":
        raise pytest.UsageError(
            "The compiled C++ extension was not imported (backend="
            f"{lipc._BACKEND!r}). Build it (cmake -B build && cmake --build build "
            "--config Release) and run with PYTHONPATH=build/python, or "
            "`pip install .`. Set TSARALOHA_ALLOW_PY_BACKEND=1 to test the fallback."
        )


@pytest.fixture(scope="session")
def lib():
    return lipc


def make_output(L, E=None, I=None, steps=0.5, **kw):
    n = len(L)
    out = lipc.Output(E=E or [210e9] * n, I=I or [8e-4] * n, L=list(L), steps=steps, **kw)
    out.compute()
    return out
