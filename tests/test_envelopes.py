"""Moving-load envelopes (2.0 semantics) against closed-form references."""
from __future__ import annotations

import math

import numpy as np
import pytest

from conftest import make_output


def run(lib, L, steps, point=(), dist=(), E=None, I=None):
    out = make_output(L, E, I, steps)
    out.set_loads(point_loads=list(point), distrib_loads=list(dist))
    out.compute_load_envelopes()
    return out


def section_loading(lib, out, curves, point=(), dist=()):
    return lib.Loading(curves=curves, position=out.X, span_node_positions=out.span_node_positions,
                       spans=out.L_spans, point_loads=list(point), distrib_loads=list(dist))


def test_partial_udl_optimum_between_nodes(lib):
    """q = 1 kN/m over 4 m, simple span L = 10 m, step 0.7 m, section x = 4.9 m.
    The optimum window starts where IL(u) = IL(u + 4): u = x(1 - 4/L) = 2.94 m,
    which is NOT a node (nodes are multiples of 0.7). The M influence line of
    a simple span is exactly piecewise linear, so the result must be exact."""
    L, x, w = 10.0, 4.9, 4.0
    out = make_output([L], [1.0], [1.0], 0.7)
    sec = int(np.argmin(np.abs(np.asarray(out.span_node_positions[0]) - x)))
    assert out.span_node_positions[0][sec] == pytest.approx(x)
    u = x * (1 - w / L)
    exact = (1 - x / L) * (x**2 - u**2) / 2 + x * ((u + w) - x) - x * ((u + w) ** 2 - x**2) / (2 * L)
    r = section_loading(lib, out, out.BM).plural_rectangular_load([1.0], [0.0, w], 0, sec)
    assert r["value"] == pytest.approx(exact, rel=1e-12)
    assert r["position"] == pytest.approx(u, abs=1e-9)


def test_udl_longer_than_beam(lib):
    """A 30 m UDL on a 10 m span covers it entirely: M_max = qL²/8."""
    out = run(lib, [10.0], 0.5, dist=[lib.Load(intensity=[2.0], length=[0.0, 30.0], name="q")])
    assert out.bending_moment_general_envelope["rectangular_load"]["max"]["value"] == pytest.approx(2.0 * 100 / 8, rel=1e-12)


def test_multi_segment_udl_equals_sum_when_contiguous(lib):
    """[q, q] over [3 m, 4 m] behaves exactly like q over 7 m."""
    out = make_output([12.0, 9.0], [1.0] * 2, [1.0] * 2, 0.5)
    a = section_loading(lib, out, out.BM, dist=[lib.Load(intensity=[5.0, 5.0], length=[0.0, 3.0, 4.0], name="a")])
    b = section_loading(lib, out, out.BM, dist=[lib.Load(intensity=[5.0], length=[0.0, 7.0], name="b")])
    flat = lambda spans: np.concatenate([np.asarray(r) for r in spans])  # spans are ragged
    for key in ("max", "min"):
        np.testing.assert_allclose(flat(a.section_envelopes["rectangular_load"][key]),
                                   flat(b.section_envelopes["rectangular_load"][key]), rtol=1e-12, atol=1e-12)


def test_rotation_envelope_units_and_value(lib):
    """Max end rotation of a simple span under a moving P:
    θ = P L² / (9√3 EI), attained for a = L(1 - 1/√3). Rotation IL is cubic in
    the load position, so the piecewise-linear search is O(h²) accurate."""
    E, I, L, P = 210e9, 8e-4, 10.0, 100.0
    out = run(lib, [L], 0.05, point=[lib.Load(intensity=[P], length=[0.0], name="P")], E=[E], I=[I])
    theta = out.rotation_general_envelope["point_load"]["maximum_value"]
    assert abs(theta) == pytest.approx(P * 1e3 * L**2 / (9 * math.sqrt(3) * E * I), rel=1e-4)


def test_combined_is_sum_of_point_and_distributed(lib):
    out = run(lib, [8.0, 11.0, 7.0], 0.5,
              point=[lib.Load(intensity=[60.0, 120.0], length=[0.0, 1.5], name="T")],
              dist=[lib.Load(intensity=[9.0], length=[0.0, 6.0], name="q")])
    for env in (out.bending_moment_general_envelope, out.shear_force_general_envelope):
        assert env["combined_load"]["max"]["value"] <= env["point_load"]["max"]["value"] + env["rectangular_load"]["max"]["value"] + 1e-9
        assert env["combined_load"]["min"]["value"] >= env["point_load"]["min"]["value"] + env["rectangular_load"]["min"]["value"] - 1e-9


def test_section_envelopes_bound_global_result(lib):
    out = make_output([8.0, 11.0], [1.0] * 2, [1.0] * 2, 0.5)
    ld = section_loading(lib, out, out.BM, point=[lib.Load(intensity=[100.0], length=[0.0], name="P")])
    env = ld.section_envelopes["point_load"]
    assert ld.point_load["max"]["value"] == pytest.approx(max(max(r) for r in env["max"]))
    assert ld.point_load["min"]["value"] == pytest.approx(min(min(r) for r in env["min"]))
    assert all(v >= 0 for r in env["max"] for v in r)
    assert all(v <= 0 for r in env["min"] for v in r)


def test_envelopes_with_fixed_supports(lib):
    """Fixed-fixed span: hogging at the ends governs, -PL/8 for P at midspan
    is the end moment for a central load; the true worst end moment is
    -4PL/27 (a = L/3)."""
    L, P = 9.0, 100.0
    out = run(lib, [0.0, L, 0.0], 0.25, point=[lib.Load(intensity=[P], length=[0.0], name="P")])
    env = out.bending_moment_general_envelope["point_load"]
    assert env["min"]["value"] == pytest.approx(-4 * P * L / 27, rel=1e-12)


def test_critical_section_envelope_is_at_the_influence_line_maximum(lib):
    out = run(lib, [10.0, 10.0], 0.5, point=[lib.Load(intensity=[100.0], length=[0.0], name="P")])
    pos = out.bending_moment_max_positions
    crit = out.bending_moment_critical_envelope["point_load"]
    i, j = (pos["i"], pos["j"]) if isinstance(pos, dict) else (pos.i, pos.j)
    assert (crit["span"], crit["section"]) == (i, j)


def test_point_position_is_exact_origin(lib):
    """Single axle, section x = 3.0 m on a 10 m span: M_max with the load at 3.0 m."""
    out = make_output([10.0], [1.0], [1.0], 1.0)
    r = section_loading(lib, out, out.BM).plural_point_load([100.0], [0.0], 0, 3)
    assert r["value"] == pytest.approx(100 * 3 * 0.7)
    assert r["position"] == pytest.approx(3.0)
