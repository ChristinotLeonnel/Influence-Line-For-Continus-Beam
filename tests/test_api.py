"""Public Python API: input validation, error messages, lifecycle, exports."""
from __future__ import annotations

import json

import pytest

from conftest import make_output


# ---------------------------------------------------------------- validation
def test_mismatched_lengths(lib):
    with pytest.raises(ValueError, match="len\\(E\\)=2, len\\(I\\)=1"):
        lib.Output(E=[210e9, 210e9], I=[8e-4], L=[10, 10], steps=1.0)


@pytest.mark.parametrize("steps", [0.0, -1.0])
def test_non_positive_step(lib, steps):
    with pytest.raises(ValueError):
        lib.Output(E=[210e9], I=[8e-4], L=[10.0], steps=steps)


def test_non_numeric_input(lib):
    with pytest.raises(TypeError):
        lib.Output(E=["a"], I=[8e-4], L=[10.0], steps=1.0)


def test_load_inconsistent_lengths(lib):
    with pytest.raises(ValueError, match="len\\(length\\)"):
        lib.Load(intensity=[10.0, 20.0], length=[0.0], name="bad")


def test_set_loads_rejects_non_load(lib):
    out = make_output([10.0])
    with pytest.raises(TypeError, match="point_loads\\[0\\]"):
        out.set_loads(point_loads=[{"intensity": [1.0]}], distrib_loads=[])


def test_envelopes_require_loads(lib):
    out = make_output([10.0])
    with pytest.raises(RuntimeError, match="setLoads"):
        out.compute_load_envelopes()


# ---------------------------------------------------------------- lifecycle
def test_constructor_computes_nothing_and_results_compute_on_access(lib):
    """2.0 : le constructeur ne calcule rien ; lire un résultat le calcule."""
    out = lib.Output(E=[210e9], I=[8e-4], L=[10.0], steps=1.0)
    assert not out.is_computed
    assert len(out.BM[0]) == len(out.span_node_positions[0])   # BM[travée][section][…]
    assert out.is_computed


def test_envelopes_compute_on_access_and_follow_new_loads(lib):
    out = lib.Output(E=[210e9], I=[8e-4], L=[10.0], steps=0.5)
    out.set_loads(point_loads=[lib.Load(intensity=[100.0], length=[0.0], name="P")], distrib_loads=[])
    assert not out.is_load_envelopes_computed
    assert out.bending_moment_general_envelope["point_load"]["max"]["value"] == pytest.approx(250.0)
    out.set_loads(point_loads=[lib.Load(intensity=[200.0], length=[0.0], name="P")], distrib_loads=[])
    assert out.bending_moment_general_envelope["point_load"]["max"]["value"] == pytest.approx(500.0)


def test_analyze_does_everything(lib):
    out = lib.Output(E=[210e9], I=[8e-4], L=[10.0], steps=0.5)
    out.set_loads(point_loads=[lib.Load(intensity=[100.0], length=[0.0], name="P")], distrib_loads=[])
    out.analyze()
    assert out.is_computed and out.is_load_envelopes_computed


def test_compute_is_idempotent(lib):
    out = make_output([10.0, 12.0])
    bm = out.BM
    out.compute()
    assert out.BM == bm


def test_shapes(lib):
    out = make_output([10.0, 12.0], steps=1.0)
    n_total = len(out.X)
    assert n_total == sum(len(s) for s in out.span_node_positions)
    for tensor in (out.BM, out.Def, out.Rot):
        assert len(tensor) == 2
        for s, span in enumerate(tensor):
            assert len(span) == len(out.span_node_positions[s])
            assert all(len(row) == n_total for row in span)
    # shear rows carry one extra value (the jump at the section)
    assert all(len(row) == n_total + 1 for span in out.SF for row in span)
    assert len(out.support_moment) == 3
    assert list(out.node_lengths) == [0.0, 10.0, 22.0]


# ---------------------------------------------------------------- exports
def test_export_all_writes_expected_tree(lib, tmp_path):
    out = make_output([10.0, 12.0], steps=1.0, root=str(tmp_path))
    out.set_loads(point_loads=[lib.Load(intensity=[50.0], length=[0.0], name="P")],
                  distrib_loads=[lib.Load(intensity=[10.0], length=[0.0, 4.0], name="q")])
    out.export_all()
    expected = [
        "01_Input/structural_model.json",
        "02_Influence_Lines/bending_moment.json",
        "02_Influence_Lines/shear_force.json",
        "02_Influence_Lines/support_moment.json",
        "03_Critical_Values/bending_moment.json",
        "04_Load_Envelopes/Global/Point_Load/bending_moment.json",
        "04_Load_Envelopes/Critical_Section/Combined_Load/shear_force.json",
    ]
    for rel in expected:
        assert (tmp_path / rel).is_file(), rel
    model = json.loads((tmp_path / "01_Input/structural_model.json").read_text(encoding="utf-8"))
    assert model["spans"] == [10.0, 12.0]
    bm = json.loads((tmp_path / "02_Influence_Lines/bending_moment.json").read_text(encoding="utf-8"))
    assert bm == out.BM


def test_compute_writes_nothing(lib, tmp_path):
    out = make_output([10.0], root=str(tmp_path))
    out.set_loads(point_loads=[lib.Load(intensity=[50.0], length=[0.0], name="P")], distrib_loads=[])
    out.compute_load_envelopes()
    assert list(tmp_path.iterdir()) == []
