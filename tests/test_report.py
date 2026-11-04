"""Calculation note: structure, traceability, executed checks, compatibility."""
from __future__ import annotations

import re

import pytest

import Tsaraloha.LIPoutreContinue as tsa
from Tsaraloha.LIPoutreContinue.report import _num, run_checks


def analysed(fixed=False):
    b = tsa.Beam().add_span(10.0, 210e9, 8e-4).add_span(12.0, 210e9, 1.2e-3)
    b.add_support(0.0, "fixed" if fixed else "pin").add_support(10.0, "roller").add_support(22.0, "roller")
    return b.analyze(step=0.5, loads=[tsa.PointLoadTrain([60, 120, 120], [1.5, 4.5], name="Camion"),
                                      tsa.DistributedLoad(9.0, 10.0, name="Voie")])


@pytest.fixture(scope="module", autouse=True)
def _agg():
    try:
        import matplotlib
        matplotlib.use("Agg")
    except ImportError:
        pass


def test_report_sections_and_traceability(tmp_path):
    res = analysed()
    p = res.report(tmp_path / "note.html", project_title="Pont test", project_ref="REF-1", author="A. Auteur")
    doc = p.read_text(encoding="utf-8")
    for heading in ["Description de la structure", "Hypothèses et modèle mathématique", "Méthode numérique",
                    "Charges mobiles", "Lignes d'influence", "Enveloppes des sollicitations",
                    "Positions critiques", "Réactions d'appui", "Vérifications", "Étude de convergence",
                    "Conclusion et limites"]:
        assert heading in doc, heading
    assert "RDM6" not in doc
    assert f"Tsaraloha.LIPoutreContinue {tsa.__version__}" in doc
    assert "Pont test" in doc and "REF-1" in doc and "A. Auteur" in doc
    assert f"{len(res.x)} nœuds" in doc
    # the governing values printed are the computed ones
    assert _num(res.maximum_bending_moment.value) in doc
    assert _num(res.minimum_bending_moment.value) in doc
    assert "NON CONFORME" not in doc and "Toutes les vérifications exécutées sont conformes" in doc
    assert doc.count("<svg") >= 1                       # beam schematic always present


def test_checks_are_executed_and_pass():
    checks = run_checks(analysed())
    names = [c["name"] for c in checks]
    assert any("ΣRᵢ = 1" in n for n in names) and any("Maxwell" in n for n in names)
    assert all(c["passed"] for c in checks), checks
    assert all(c["measure"] < c["threshold"] for c in checks)


def test_fixed_end_skips_moment_equilibrium_but_passes(tmp_path):
    res = analysed(fixed=True)
    checks = run_checks(res)
    assert not any("ΣRᵢ·xᵢ" in c["name"] for c in checks)
    assert all(c["passed"] for c in checks)
    doc = res.report(tmp_path / "n.html").read_text(encoding="utf-8")
    assert "encastrement" in doc


def test_convergence_study(tmp_path):
    res = analysed()
    doc = res.report(tmp_path / "c.html", convergence=True).read_text(encoding="utf-8")
    section = doc[doc.index("10. Étude de convergence"):doc.index("11. Conclusion")]
    assert section.count("<tr>") == 1 + 3              # header + 3 steps (2h, h, h/2)
    assert "écart relatif" in section
    assert "Non exécutée" not in section


def test_no_loads_report(tmp_path):
    res = tsa.Beam().add_span(8.0, 210e9, 8e-4).analyze(step=0.5)
    doc = res.report(tmp_path / "i.html").read_text(encoding="utf-8")
    assert "Aucune charge mobile" in doc and "Enveloppes des sollicitations" not in doc


def test_legacy_output_entry_point(tmp_path):
    out = tsa.Output(E=[210e9] * 3, I=[8e-4] * 3, L=[0.0, 10.0, 12.0], steps=0.5)
    out.compute()
    out.set_loads(point_loads=[tsa.Load(intensity=[100.0], length=[0.0], name="P")], distrib_loads=[])
    out.compute_load_envelopes()
    with pytest.warns(DeprecationWarning):
        p = out.generate_calculation_note(str(tmp_path / "legacy.html"))
    doc = open(p, encoding="utf-8").read()
    assert "encastrement" in doc and "RDM6" not in doc


def test_pdf_requires_weasyprint(tmp_path):
    try:
        import weasyprint  # noqa: F401
        pytest.skip("weasyprint installé : chemin d'erreur non testable ici")
    except ImportError:
        pass
    with pytest.raises(ImportError, match="weasyprint"):
        analysed().report(tmp_path / "n.pdf")
