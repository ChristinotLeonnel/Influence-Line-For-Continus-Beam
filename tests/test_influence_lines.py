"""Influence lines of the continuous-beam solver vs an independent oracle.

These tests pass on the baseline: the focal-ratio solver is exact to machine
precision (docs/audit/03_numerique.md §1). They guard it during refactoring.
"""
from __future__ import annotations

import numpy as np
import pytest

from _reference import ContinuousBeamReference, max_rel_error
from conftest import make_output

TOL = 1e-12  # observed ≤ 1e-15; margin for compiler/flag differences

CASES = {
    "1 span": ([10.0], [210e9], [8e-4], 0.5),
    "2 equal spans": ([10.0, 10.0], [210e9] * 2, [8e-4] * 2, 1.0),
    "3 spans, variable EI": ([12.0, 16.0, 12.0], [35e9] * 3, [0.01, 0.0171, 0.01], 0.5),
    "4 spans, non-integer step": ([5.0, 7.3, 11.1, 4.0], [30e9, 210e9, 30e9, 70e9],
                                  [1e-3, 2e-4, 3e-3, 5e-4], 0.37),
    "fixed-pinned": ([0.0, 10.0], [210e9] * 2, [8e-4] * 2, 0.5),
    "fixed-fixed": ([0.0, 10.0, 0.0], [210e9] * 3, [8e-4] * 3, 0.5),
    "pinned-fixed, 2 spans": ([8.0, 9.0, 0.0], [210e9] * 3, [8e-4, 5e-4, 5e-4], 0.5),
}


@pytest.fixture(params=list(CASES), ids=list(CASES), scope="module")
def case(request):
    L, E, I, st = CASES[request.param]
    out = make_output(L, E, I, st)
    ref = ContinuousBeamReference(L, E, I, out.span_node_positions)
    return out, ref


def test_bending_moment(case):
    out, ref = case
    assert max_rel_error(out.BM, ref.bending_moment()) < TOL


def test_shear_force(case):
    out, ref = case
    assert max_rel_error(out.SF, ref.shear_force()) < TOL


def test_deflection(case):
    out, ref = case
    assert max_rel_error(out.Def, ref.deflection()) < TOL


def test_rotation(case):
    out, ref = case
    assert max_rel_error(out.Rot, ref.rotation()) < TOL


def test_support_moments(case):
    out, ref = case
    got = np.asarray(out.support_moment)
    exp = ref.support_moment_lines()
    assert got.shape == exp.shape
    assert np.max(np.abs(got - exp)) <= TOL * np.max(np.abs(exp)) + 1e-300


# ---------------------------------------------------------------- closed forms
def _at(out, x):
    return int(np.argmin(np.abs(np.asarray(out.X) - x)))


def test_two_equal_spans_midspan_load():
    """M_B = -3PL/32 for P at midspan of span 1 (two equal spans)."""
    out = make_output([10.0, 10.0], [1.0] * 2, [1.0] * 2, 0.5)
    assert out.support_moment[1][_at(out, 5.0)] == pytest.approx(-3 * 10 / 32, rel=1e-13)


@pytest.mark.parametrize("a", [1.0, 3.0, 5.0, 8.5])
def test_two_equal_spans_general_load(a):
    """M_B = -P a (L² - a²) / (4 L²) for P at a in span 1."""
    L = 10.0
    out = make_output([L, L], [1.0] * 2, [1.0] * 2, 0.5)
    assert out.support_moment[1][_at(out, a)] == pytest.approx(-a * (L * L - a * a) / (4 * L * L), rel=1e-13)


@pytest.mark.parametrize("a", [2.0, 3.0, 7.5])
def test_fixed_fixed_end_moments(a):
    """M_A = -a b² / L², M_B = -a² b / L²."""
    L = 10.0
    b = L - a
    out = make_output([0.0, L, 0.0], [1.0] * 3, [1.0] * 3, 0.5)
    k = _at(out, a)
    assert out.support_moment[0][k] == pytest.approx(-a * b * b / L**2, rel=1e-13)
    assert out.support_moment[1][k] == pytest.approx(-a * a * b / L**2, rel=1e-13)


def test_simple_span_midspan_deflection():
    """w(L/2; P at L/2) = -P L³ / (48 EI) per unit load."""
    E, I, L = 210e9, 8e-4, 10.0
    out = make_output([L], [E], [I], 0.5)
    k = _at(out, 5.0)
    assert out.Def[0][k][k] == pytest.approx(-L**3 / (48 * E * I), rel=1e-13)


# ---------------------------------------------------------------- physics
def test_maxwell_betti_deflection():
    """w(x_i ; unit load at x_j) == w(x_j ; unit load at x_i)."""
    out = make_output([5.0, 7.3, 11.1], [30e9, 210e9, 30e9], [1e-3, 2e-4, 3e-3], 0.37)
    W = np.vstack([np.asarray(s) for s in out.Def])
    assert np.max(np.abs(W - W.T)) <= 1e-12 * np.max(np.abs(W))


def test_moment_continuity_across_supports():
    """Moment at the end of span i equals moment at the start of span i+1."""
    out = make_output([6.0, 9.0, 7.0], steps=0.5)
    for s in range(len(out.BM) - 1):
        np.testing.assert_allclose(out.BM[s][-1], out.BM[s + 1][0], rtol=0, atol=1e-12)


def test_deflection_zero_on_supports():
    out = make_output([6.0, 9.0, 7.0], steps=0.5)
    for s in range(len(out.Def)):
        assert np.max(np.abs(out.Def[s][0])) < 1e-25
        assert np.max(np.abs(out.Def[s][-1])) < 1e-25


def test_results_are_reproducible():
    a = make_output([6.0, 9.0, 7.0], steps=0.37)
    b = make_output([6.0, 9.0, 7.0], steps=0.37)
    assert a.BM == b.BM and a.SF == b.SF and a.Def == b.Def and a.Rot == b.Rot
