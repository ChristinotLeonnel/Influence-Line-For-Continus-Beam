"""High-level API (Beam / PointLoadTrain / DistributedLoad / Results)."""
from __future__ import annotations

import json
import math

import numpy as np
import pytest

import Tsaraloha.LIPoutreContinue as tsa


def two_spans():
    b = tsa.Beam()
    b.add_span(length=10.0, E=210e9, I=8e-4).add_span(length=12.0, E=210e9, I=1.2e-3)
    b.add_support(0.0, "pin").add_support(10.0, "roller").add_support(22.0, "roller")
    return b


def test_example_from_the_brief():
    res = two_spans().analyze(step=0.25, loads=[tsa.PointLoadTrain([60, 120, 120], [1.5, 4.5], name="Camion"),
                                                tsa.DistributedLoad(q=9.0, length=10.0, name="Voie")])
    m = res.maximum_bending_moment
    assert m.value > 0 and m.unit == "kN·m" and 0 <= m.x <= 22 and set(m.loads) == {"Camion", "Voie"}
    assert res.minimum_bending_moment.value < 0
    # hogging governs at the interior support
    assert res.minimum_bending_moment.x == pytest.approx(10.0)


def test_matches_low_level_engine():
    """The facade adds no computation: same numbers as Output."""
    truck = tsa.PointLoadTrain([100.0, 100.0], [3.0])
    res = two_spans().analyze(step=0.5, loads=[truck])
    out = tsa.Output(E=[210e9] * 2, I=[8e-4, 1.2e-3], L=[10.0, 12.0], steps=0.5)
    out.compute()
    out.set_loads(point_loads=[tsa.Load(intensity=[100.0, 100.0], length=[0.0, 3.0], name="x")], distrib_loads=[])
    out.compute_load_envelopes()
    assert res.envelope("M", "point").max_extremum.value == out.bending_moment_general_envelope["point_load"]["max"]["value"]
    assert res.envelope("w").min_extremum.value == out.deflection_general_envelope["combined_load"]["min"]["value"]


def test_simple_span_values_and_units():
    b = tsa.Beam().add_span(10.0, 210e9, 8e-4)          # supports added automatically
    res = b.analyze(step=0.5, loads=[tsa.PointLoadTrain([100.0])])
    assert res.maximum_bending_moment.value == pytest.approx(250.0)
    assert res.maximum_bending_moment.x == pytest.approx(5.0)
    assert res.maximum_deflection.value == pytest.approx(-100e3 * 1000 / (48 * 210e9 * 8e-4))
    assert res.maximum_deflection.unit == "m"


def test_fixed_end_mapping():
    """Fixed-fixed beam through the API equals the zero-length-span engine model."""
    b = tsa.Beam().add_span(9.0, 1.0, 1.0)
    b.add_support(0.0, "fixed").add_support(9.0, "fixed")
    res = b.analyze(step=0.25, loads=[tsa.PointLoadTrain([100.0])])
    assert res.minimum_bending_moment.value == pytest.approx(-4 * 100 * 9 / 27, rel=1e-12)
    assert b.to_dict()["supports"] == [{"x": 0.0, "type": "fixed"}, {"x": 9.0, "type": "fixed"}]


def test_distributed_load_defaults_to_full_length():
    b = tsa.Beam().add_span(10.0, 1.0, 1.0)
    res = b.analyze(step=1.0, loads=[tsa.DistributedLoad(q=2.0)])
    assert res.maximum_bending_moment.value == pytest.approx(2.0 * 100 / 8)


def test_influence_line_accessors():
    res = tsa.Beam().add_span(10.0, 210e9, 8e-4).analyze(step=0.5)
    xl, m, xs = res.influence_line("M", 5.0)
    assert xs == pytest.approx(5.0) and m.max() == pytest.approx(2.5) and xl[int(m.argmax())] == pytest.approx(5.0)
    _, w, _ = res.influence_line("w", 5.0)
    assert w.min() == pytest.approx(-1e3 * 1000 / (48 * 210e9 * 8e-4))   # per kN
    _, v, _ = res.influence_line("V", 5.0)
    assert v.max() == pytest.approx(0.5) and v.min() == pytest.approx(-0.5)
    xr, r = res.reaction_influence_line(0)
    assert r[0] == pytest.approx(1.0) and r[-1] == pytest.approx(0.0)


def test_reactions_summary():
    res = two_spans().analyze(step=0.5, loads=[tsa.PointLoadTrain([100.0])])
    rs = res.reactions()
    assert [r["support"] for r in rs] == [0, 1, 2]
    assert rs[1]["max"] == pytest.approx(100.0)


def test_to_json_roundtrip(tmp_path):
    res = two_spans().analyze(step=0.5, loads=[tsa.PointLoadTrain([100.0]), tsa.DistributedLoad(5.0, 4.0)])
    p = res.to_json(tmp_path / "r.json")
    d = json.loads(p.read_text(encoding="utf-8"))
    assert d["units"]["M"] == "kN·m"
    assert d["envelopes"]["M"]["max"]["value"] == pytest.approx(res.maximum_bending_moment.value)
    assert len(d["reactions"]) == 3


def test_plot_returns_axes():
    pytest.importorskip("matplotlib")
    import matplotlib
    matplotlib.use("Agg")
    res = two_spans().analyze(step=0.5, loads=[tsa.PointLoadTrain([100.0])])
    ax = res.plot("M")
    assert ax.get_xlabel() == "x [m]"


# ---------------------------------------------------------------- errors
def test_errors_are_explicit():
    with pytest.raises(ValueError, match="aucune travée"):
        tsa.Beam().analyze()
    with pytest.raises(ValueError, match="strictement positif"):
        tsa.Beam().add_span(-1.0, 1.0, 1.0)
    with pytest.raises(ValueError, match="Type d'appui"):
        tsa.Beam().add_support(0.0, "hinge")
    b = tsa.Beam().add_span(5.0, 1.0, 1.0).add_span(5.0, 1.0, 1.0)
    b.add_support(0.0).add_support(5.0)
    with pytest.raises(NotImplementedError, match="porte-à-faux"):
        b.analyze()
    b.add_support(7.0)
    with pytest.raises(ValueError, match="extrémités des travées"):
        b.analyze()
    with pytest.raises(ValueError, match="entraxe"):
        tsa.PointLoadTrain([10.0, 10.0], [])
    with pytest.raises(RuntimeError, match="Aucune charge"):
        tsa.Beam().add_span(5.0, 1.0, 1.0).analyze().envelope("M")
    with pytest.raises(ValueError, match="Grandeur inconnue"):
        tsa.Beam().add_span(5.0, 1.0, 1.0).analyze(loads=[tsa.PointLoadTrain([1.0])]).envelope("Q")


def test_reversed_train():
    t = tsa.PointLoadTrain([60, 120, 90], [1.5, 4.0], name="T")
    r = t.reversed()
    assert r.axles == (90.0, 120.0, 60.0) and r.spacings == (4.0, 1.5)
