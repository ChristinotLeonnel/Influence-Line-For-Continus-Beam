"""Shared fixtures.

The tests target the C++ engine, the single numerical implementation. The
package raises ImportError when the compiled extension is missing.
"""
from __future__ import annotations

import pytest

import Tsaraloha.LIPoutreContinue as lipc


def pytest_report_header(config):
    return f"Tsaraloha.LIPoutreContinue {lipc.__version__} ({lipc.__file__})"


@pytest.fixture(scope="session")
def lib():
    return lipc


def make_output(L, E=None, I=None, steps=0.5, **kw):
    n = len(L)
    out = lipc.Output(E=E or [210e9] * n, I=I or [8e-4] * n, L=list(L), steps=steps, **kw)
    out.compute()
    return out
