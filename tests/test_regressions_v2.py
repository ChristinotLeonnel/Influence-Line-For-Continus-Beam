"""Regression tests for the defects N1–N9 of the 1.x engine
(docs/audit/03_numerique.md), all fixed in 2.0.

Each test states the correct expected result, derived independently of the
library (closed form or brute force). They were introduced as
xfail(strict=True) before the fix and turned into plain tests by the fix.
Do not delete or loosen them.
"""
from __future__ import annotations

import numpy as np
import pytest

from _reference import iso_shear_pair
from conftest import make_output



def envelopes(lib, L, steps, point=(), dist=(), E=None, I=None):
    out = make_output(L, E, I, steps)
    out.set_loads(point_loads=list(point), distrib_loads=list(dist))
    out.compute_load_envelopes()
    return out


# ---------------------------------------------------------------- N8 mesh
# Régression N8 (corrigé en 2.0) — ancien défaut : mesh built by accumulating `pas += steps` creates a sliver element
@pytest.mark.parametrize("L, h, n_expected", [(6.0, 0.1, 61), (1.0, 0.1, 11), (3.0, 0.3, 11)])
def test_mesh_has_no_sliver_element(L, h, n_expected):
    x = np.asarray(make_output([L], steps=h).span_node_positions[0])
    assert len(x) == n_expected
    assert np.diff(x).min() > 0.5 * h


def test_mesh_non_dividing_step_keeps_exact_endpoints():
    """Passes today; must keep passing after the N8 fix."""
    x = make_output([10.0], steps=0.37).span_node_positions[0]
    assert x[0] == 0.0 and x[-1] == 10.0
    assert np.diff(x).min() > 0


# ---------------------------------------------------------------- N6 UDL
# Régression N6 (corrigé en 2.0) — ancien défaut : OneRectangularLoad integrates i < end: last interval dropped
def test_udl_full_span_moment(lib):
    """q = 1 kN/m on a 10 m simple span: M_max = qL²/8 = 12.5 kN·m."""
    out = envelopes(lib, [10.0], 1.0, dist=[lib.Load(intensity=[1.0], length=[0.0, 10.0], name="q")])
    assert out.bending_moment_general_envelope["rectangular_load"]["maximum_value"] == pytest.approx(12.5, rel=1e-9)


# ---------------------------------------------------------------- N3 entry
# Régression N3 (corrigé en 2.0) — ancien défaut : convoy start positions are only swept over x >= 0 (no entry from the left)
def test_convoy_can_enter_from_the_left(lib):
    """[10 kN, 100 kN] spaced 20 m on a 10 m span: worst = 100 kN at midspan = 250 kN·m."""
    out = envelopes(lib, [10.0], 0.5, point=[lib.Load(intensity=[10.0, 100.0], length=[0.0, 20.0], name="T")])
    assert out.bending_moment_general_envelope["point_load"]["maximum_value"] == pytest.approx(250.0, rel=1e-9)


# ---------------------------------------------------------------- N4 length
# Régression N4 (corrigé en 2.0) — ancien défaut : CombinedLoad reads `length` as absolute offsets, PluralPointLoad as spacings
def test_combined_equals_point_without_udl(lib):
    pl = [lib.Load(intensity=[100.0, 100.0, 50.0], length=[0.0, 2.0, 3.0], name="T")]
    env = envelopes(lib, [8.0, 10.0], 0.5, point=pl).bending_moment_general_envelope
    assert env["combined_load"]["maximum_value"] == pytest.approx(env["point_load"]["maximum_value"], rel=1e-12)


# ---------------------------------------------------------------- N5 combined
# Régression N5 (corrigé en 2.0) — ancien défaut : combined envelope locks every load to one common start position
def test_combined_superposes_independent_loads(lib):
    """P = 1 kN and q = 1 kN/m over 10 m on a 10 m span: 2.5 + 12.5 = 15 kN·m."""
    out = envelopes(lib, [10.0], 1.0,
                    point=[lib.Load(intensity=[1.0], length=[0.0], name="P")],
                    dist=[lib.Load(intensity=[1.0], length=[0.0, 10.0], name="q")])
    assert out.bending_moment_general_envelope["combined_load"]["maximum_value"] == pytest.approx(15.0, rel=1e-9)


# ---------------------------------------------------------------- N2 signs
# Régression N2 (corrigé en 2.0) — ancien défaut : per-load signed max-|.| values are summed; opposite signs cancel
def test_envelope_does_not_cancel_opposite_signs(lib):
    """Two independent point loads: at EVERY section the library envelopes
    E+ and E- must equal sum of per-load maxima / minima, obtained by an
    independent brute force (fine sweep of the load origin, entry from both
    ends, loads allowed off the beam)."""
    out = make_output([10.0, 10.0], [1.0] * 2, [1.0] * 2, 0.5)
    loads = [([100.0], [0.0]), ([60.0, 60.0], [0.0, 9.0])]
    ld = lib.Loading(curves=out.BM, position=out.X, span_node_positions=out.span_node_positions,
                     spans=out.L_spans,
                     point_loads=[lib.Load(intensity=i, length=l, name=f"L{k}") for k, (i, l) in enumerate(loads)],
                     distrib_loads=[])
    env = ld.section_envelopes["point_load"]
    X = np.asarray(out.X)
    sweep = np.arange(X[0] - 10.0, X[-1] + 0.0005, 0.25)   # contains every node - offset
    for s in range(2):
        for sec in range(len(out.span_node_positions[s])):
            il = np.asarray(out.BM[s][sec])
            hi = lo = 0.0
            for inten, lens in loads:
                offs = np.cumsum(lens)
                vals = [sum(p * np.interp(a0 + d, X, il, left=0.0, right=0.0) for p, d in zip(inten, offs))
                        for a0 in sweep]
                hi += max(0.0, max(vals))
                lo += min(0.0, min(vals))
            assert env["max"][s][sec] == pytest.approx(hi, abs=1e-9)
            assert env["min"][s][sec] == pytest.approx(lo, abs=1e-9)


def test_global_envelope_reports_both_signs(lib):
    """Interior support of 2 spans: hogging (min) governs, sagging (max) is
    still reported separately."""
    out = envelopes(lib, [10.0, 10.0], 0.5, point=[lib.Load(intensity=[100.0], length=[0.0], name="P")])
    env = out.bending_moment_general_envelope["point_load"]
    assert env["max"]["value"] > 0 > env["min"]["value"]
    assert env["maximum_value"] in (env["max"]["value"], env["min"]["value"])
    assert abs(env["maximum_value"]) == max(env["max"]["value"], -env["min"]["value"])


# ---------------------------------------------------------------- N7 shear
# Régression N7 (corrigé en 2.0) — ancien défaut : SF rows (N+1 values) are indexed with the X grid (N values): one-node shift
def test_shear_critical_position(lib):
    """Section x = 3 m of a 10 m span, single 100 kN: V⁺ = 70 kN with the load at x = 3 m."""
    out = make_output([10.0], [1.0], [1.0], 1.0)
    ld = lib.Loading(curves=out.SF, position=out.X, span_node_positions=out.span_node_positions,
                     spans=out.L_spans, point_loads=[], distrib_loads=[])
    r = ld.plural_point_load([100.0], [0.0], 0, 3)
    assert r["value"] == pytest.approx(70.0)
    assert out.X[r["max_position"]] == pytest.approx(3.0)


def test_shear_value_two_axles_matches_brute_force(lib):
    """Passes today (the value is right, only the position is shifted)."""
    L = 10.0
    out = make_output([L], [1.0], [1.0], 1.0)
    out.set_loads(point_loads=[lib.Load(intensity=[100.0, 100.0], length=[0.0, 3.0], name="T")], distrib_loads=[])
    out.compute_load_envelopes()
    got = out.shear_force_general_envelope["point_load"]["maximum_value"]
    nodes = out.span_node_positions[0]
    best = 0.0
    for x in nodes:
        for pick in (0, -1):
            for a0 in nodes:
                v = sum(100 * iso_shear_pair(x, a0 + d, L)[pick] for d in (0, 3) if a0 + d <= L)
                best = v if abs(v) > abs(best) else best
    assert abs(got) == pytest.approx(abs(best))


# ---------------------------------------------------------------- N9 snapping
# Régression N9 (corrigé en 2.0) — ancien défaut : axles are snapped to the nearest node: convoy geometry not respected
def test_axle_spacing_not_multiple_of_step(lib):
    """2 x 100 kN at 1.5 m on a 10 m span, step 1 m. At x = 5 m the exact
    worst M is 100·2.5 + 100·1.75 = 425 kN·m (second axle at 3.5 or 6.5 m)."""
    out = make_output([10.0], [1.0], [1.0], 1.0)
    ld = lib.Loading(curves=out.BM, position=out.X, span_node_positions=out.span_node_positions,
                     spans=out.L_spans, point_loads=[], distrib_loads=[])
    assert ld.plural_point_load([100.0, 100.0], [0.0, 1.5], 0, 5)["value"] == pytest.approx(425.0, rel=1e-9)


# ---------------------------------------------------------------- N1 units
# Régression N1 (corrigé en 2.0) — ancien défaut : E in Pa with loads in kN: deflections 1000x too small
def test_deflection_envelope_units(lib):
    """100 kN at midspan, L = 10 m, E = 210 GPa, I = 8e-4 m⁴: |w| = PL³/48EI = 1.240e-2 m."""
    E, I, L = 210e9, 8e-4, 10.0
    out = envelopes(lib, [L], 0.5, point=[lib.Load(intensity=[100.0], length=[0.0], name="P")], E=[E], I=[I])
    w = out.deflection_general_envelope["point_load"]["maximum_value"]
    assert abs(w) == pytest.approx(100e3 * L**3 / (48 * E * I), rel=1e-9)
