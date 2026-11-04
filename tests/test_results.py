"""Results added in 2.0: support reactions, envelope curves, physical extrema."""
from __future__ import annotations

import json

import numpy as np
import pytest

from conftest import make_output


def _at(out, x):
    return int(np.argmin(np.abs(np.asarray(out.X) - x)))


# ---------------------------------------------------------------- reactions
@pytest.mark.parametrize("L, steps", [([10.0], 0.5), ([10.0, 10.0], 0.5), ([6.0, 9.3, 7.1, 4.0], 0.37),
                                      ([0.0, 10.0], 0.5), ([0.0, 8.0, 0.0], 0.25)])
def test_reactions_satisfy_equilibrium(L, steps):
    """For every unit-load position: ΣR = 1 (forces) and ΣR·x = x_load (moments
    about x = 0). Holds for fixed ends only if the fixed-end moments are added;
    they are, through the moment equation, so we check forces only there."""
    out = make_output(L, steps=steps, I=[1e-3 * (1 + i) for i in range(len(L))])
    R = np.asarray(out.support_reaction)
    X = np.asarray(out.X)
    xs = np.asarray(out.node_lengths)
    np.testing.assert_allclose(R.sum(axis=0), 1.0, atol=1e-12)
    if all(l > 0 for l in L):  # pinned supports only: no end moment
        np.testing.assert_allclose(R.T @ xs, X, atol=1e-9)


def test_reaction_two_equal_spans_closed_form():
    """Load P at midspan of span 1: R_B = 11P/16, R_A = 13P/32, R_C = -3P/32."""
    out = make_output([10.0, 10.0], [1.0] * 2, [1.0] * 2, 0.5)
    k = _at(out, 5.0)
    R = np.asarray(out.support_reaction)[:, k]
    np.testing.assert_allclose(R, [13 / 32, 11 / 16, -3 / 32], rtol=1e-13)


@pytest.mark.parametrize("a", [2.0, 3.0, 6.5])
def test_reaction_fixed_fixed_closed_form(a):
    """Fixed-fixed span: R_A = b²(3a + b)/L³."""
    L = 10.0
    b = L - a
    out = make_output([0.0, L, 0.0], [1.0] * 3, [1.0] * 3, 0.5)
    R = np.asarray(out.support_reaction)[:, _at(out, a)]
    assert R[0] == pytest.approx(b * b * (3 * a + b) / L**3, rel=1e-13)
    assert R[1] == pytest.approx(a * a * (a + 3 * b) / L**3, rel=1e-13)


def test_reaction_envelopes_single_load(lib):
    """One 100 kN load: envelope = 100 × (max, min) of each reaction line
    (a single load on a piecewise-linear line peaks at a node)."""
    out = make_output([10.0, 12.0, 8.0], steps=0.5)
    out.set_loads(point_loads=[lib.Load(intensity=[100.0], length=[0.0], name="P")], distrib_loads=[])
    out.compute_load_envelopes()
    R = np.asarray(out.support_reaction)
    envs = out.support_reaction_envelopes
    assert len(envs) == 4
    for i, env in enumerate(envs):
        assert env["support"] == i and env["x"] == pytest.approx(out.node_lengths[i])
        assert env["point_load"]["max"]["value"] == pytest.approx(100 * max(R[i].max(), 0.0), rel=1e-12)
        assert env["point_load"]["min"]["value"] == pytest.approx(100 * min(R[i].min(), 0.0), rel=1e-12)
    assert envs[1]["point_load"]["max"]["value"] == pytest.approx(100.0)   # load right on the support


def test_reaction_envelope_udl_converges_quadratically(lib):
    """q over both spans of 2 x L: exact R_B = 10qL/8. The reaction line is
    cubic in the load position between nodes; the piecewise-linear
    integration is O(h²) (documented limitation, docs/audit/03 N6b): the
    error must divide by ~4 when h is halved and stay below 1e-3 at h = 0.5."""
    L, q = 10.0, 4.0
    exact = 10 * q * L / 8
    errs = []
    for h in (1.0, 0.5, 0.25):
        out = make_output([L, L], steps=h)
        out.set_loads(point_loads=[], distrib_loads=[lib.Load(intensity=[q], length=[0.0, 2 * L], name="q")])
        out.compute_load_envelopes()
        errs.append(abs(out.support_reaction_envelopes[1]["rectangular_load"]["max"]["value"] - exact) / exact)
    assert errs[1] < 1e-3
    for coarse, fine in zip(errs, errs[1:]):
        assert coarse / fine == pytest.approx(4.0, rel=0.05)


# ---------------------------------------------------------------- curves
def test_section_envelope_curves(lib):
    out = make_output([10.0, 12.0], steps=0.5)
    out.set_loads(point_loads=[lib.Load(intensity=[60.0, 120.0], length=[0.0, 1.5], name="T")],
                  distrib_loads=[lib.Load(intensity=[9.0], length=[0.0, 6.0], name="q")])
    out.compute_load_envelopes()
    curves = out.section_envelopes
    for quantity, general in [("bending_moment", out.bending_moment_general_envelope),
                              ("shear_force", out.shear_force_general_envelope),
                              ("deflection", out.deflection_general_envelope),
                              ("rotation", out.rotation_general_envelope)]:
        c = curves[quantity]
        assert c["x"] == out.X
        for cat in ("point_load", "rectangular_load", "combined_load"):
            mx, mn = np.asarray(c[cat]["max"]), np.asarray(c[cat]["min"])
            assert len(mx) == len(out.X) and (mx >= 0).all() and (mn <= 0).all()
            assert mx.max() == pytest.approx(general[cat]["max"]["value"])
            assert mn.min() == pytest.approx(general[cat]["min"]["value"])
            assert general[cat]["max"]["section_x"] == pytest.approx(out.X[int(mx.argmax())])


def test_influence_line_extrema_physical_positions():
    """Simple span: max M influence = L/4 at midspan with the load at midspan."""
    out = make_output([10.0], [1.0], [1.0], 0.5)
    e = out.influence_line_extrema["bending_moment"]["max"]
    assert e["value"] == pytest.approx(2.5)
    assert e["section_x"] == pytest.approx(5.0) and e["load_x"] == pytest.approx(5.0)
    v = out.influence_line_extrema["shear_force"]
    assert v["max"]["value"] == pytest.approx(1.0) and v["min"]["value"] == pytest.approx(-1.0)


def test_export_contains_new_results(lib, tmp_path):
    out = make_output([10.0, 12.0], steps=1.0, root=str(tmp_path))
    out.set_loads(point_loads=[lib.Load(intensity=[50.0], length=[0.0], name="P")], distrib_loads=[])
    out.export_all()
    R = json.loads((tmp_path / "02_Influence_Lines/support_reaction.json").read_text(encoding="utf-8"))
    assert R == out.support_reaction
    reac = json.loads((tmp_path / "04_Load_Envelopes/support_reactions.json").read_text(encoding="utf-8"))
    assert [r["support"] for r in reac] == [0, 1, 2]
    curves = json.loads((tmp_path / "04_Load_Envelopes/envelope_curves.json").read_text(encoding="utf-8"))
    assert set(curves) == {"bending_moment", "shear_force", "deflection", "rotation"}
    env = json.loads((tmp_path / "04_Load_Envelopes/Global/Point_Load/bending_moment.json").read_text(encoding="utf-8"))
    assert {"maximum", "max", "min", "section_x", "load"} <= set(env)
