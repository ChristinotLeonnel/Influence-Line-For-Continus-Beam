"""
report.py — Note de calcul de Tsaraloha.LIPoutreContinue.

Produite à partir d'un objet :class:`~Tsaraloha.LIPoutreContinue.api.Results`
(``res.report("note.html")``). Règles :

* Toute valeur affichée provient du moteur de calcul ou d'une vérification
  **exécutée au moment de la génération** (équilibre, réciprocité,
  continuité, convergence optionnelle). Rien n'est saisi à la main.
* Chaque résultat est traçable : version de la librairie, méthode, pas,
  nombre de nœuds et données d'entrée complètes sont reproduits.
* Sortie HTML autonome (figures SVG intégrées). PDF si ``path`` se termine
  par ``.pdf`` : nécessite ``weasyprint`` (``pip install Tsaraloha[report]``).
"""
from __future__ import annotations

import datetime
import html
import io
import math
import warnings
from pathlib import Path
from typing import Optional, Union

import numpy as np

__all__ = ["generate_report", "run_checks", "generate_calculation_note", "generate_calculation_pdf"]

_UNITS = {"M": "kN·m", "V": "kN", "w": "m", "theta": "rad"}
_LABELS = {"M": "Moment fléchissant M", "V": "Effort tranchant V", "w": "Flèche w", "theta": "Rotation θ"}
_KIND_LABELS = {"point": "Charges ponctuelles", "distributed": "Charges réparties", "combined": "Combinaison"}


def _e(s) -> str:
    return html.escape(str(s))


def _num(v, digits=4) -> str:
    if v is None or (isinstance(v, float) and not math.isfinite(v)):
        return "—"
    v = float(v)
    if v == 0:
        return "0"
    if abs(v) >= 1e5 or abs(v) < 1e-3:
        return f"{v:.{digits - 1}e}"
    return f"{v:.{digits}g}" if abs(v) >= 1 else f"{v:.{digits}f}".rstrip("0").rstrip(".")


# =============================================================================
#  Vérifications exécutées
# =============================================================================
def run_checks(res) -> list:
    """Contrôles d'équilibre et de cohérence exécutés sur les résultats du
    moteur. Retourne une liste de dicts {name, measure, threshold, passed,
    description}. Ce sont des contrôles indépendants (Python/numpy) du
    calcul C++, pas un second moteur."""
    out = res.output
    checks = []
    R = np.asarray(out.support_reaction, dtype=float)
    X = np.asarray(out.X, dtype=float)
    xs = np.asarray(out.node_lengths, dtype=float)

    dev = float(np.max(np.abs(R.sum(axis=0) - 1.0)))
    checks.append(dict(name="Équilibre vertical ΣRᵢ = 1", measure=dev, threshold=1e-9, passed=dev < 1e-9,
                       description="Écart maximal, sur toutes les positions de la charge unité, entre la somme "
                                   "des réactions et la charge."))
    fixed = res.beam is not None and any(sp["type"] == "fixed" for sp in res.beam.to_dict()["supports"])
    if not fixed:
        dev = float(np.max(np.abs(R.T @ xs - X)))
        checks.append(dict(name="Équilibre des moments ΣRᵢ·xᵢ = x", measure=dev, threshold=1e-8, passed=dev < 1e-8,
                           description="Écart maximal [m] entre le moment des réactions et celui de la charge "
                                       "unité (appuis simples)."))
    W = np.vstack([np.asarray(s, dtype=float) for s in out.Def])
    asym = float(np.max(np.abs(W - W.T)) / max(np.max(np.abs(W)), 1e-300))
    checks.append(dict(name="Réciprocité de Maxwell-Betti (flèches)", measure=asym, threshold=1e-10,
                       passed=asym < 1e-10,
                       description="Asymétrie relative maximale de la matrice des coefficients d'influence "
                                   "w(xᵢ ; xⱼ) − w(xⱼ ; xᵢ)."))
    BM = out.BM
    cont = 0.0
    for s in range(len(BM) - 1):
        cont = max(cont, float(np.max(np.abs(np.asarray(BM[s][-1]) - np.asarray(BM[s + 1][0])))))
    checks.append(dict(name="Continuité du moment sur appuis", measure=cont, threshold=1e-10, passed=cont < 1e-10,
                       description="Écart maximal entre M en fin de travée i et en début de travée i+1 [m/kN]."))
    wsup = max(float(np.max(np.abs(np.asarray(out.Def[s][j])))) for s in range(len(out.Def)) for j in (0, -1))
    checks.append(dict(name="Flèche nulle aux appuis", measure=wsup, threshold=1e-15, passed=wsup < 1e-15,
                       description="Flèche maximale aux appuis [m/N]."))
    return checks


def _convergence(res) -> Optional[dict]:
    """Ré-exécute l'analyse avec 2h et h/2 et rapporte les valeurs gouvernantes."""
    if not res._has_loads or res.beam is None:
        return None
    rows = []
    for factor in (2.0, 1.0, 0.5):
        h = res.step * factor
        r = res if factor == 1.0 else res.beam.analyze(step=h, loads=res.loads)
        rows.append({"step": h, "nodes": len(r.x),
                     **{q: r.envelope(q).governing.value for q in ("M", "V", "w")}})
    # Écart relatif entre pas successifs (2h→h, h→h/2). Un « ordre observé »
    # sur 3 points n'est pas fiable ici : la section de l'extrême est un nœud
    # du maillage, d'où une erreur O(h²) à constante irrégulière.
    for prev, cur in zip(rows, rows[1:]):
        cur["delta"] = {q: abs(cur[q] - prev[q]) / abs(cur[q]) if cur[q] else 0.0 for q in ("M", "V", "w")}
    return {"rows": rows}


# =============================================================================
#  Figures
# =============================================================================
def _beam_svg(beam_dict: dict, width=760, height=150) -> str:
    L = beam_dict["total_length"] or 1.0
    m = 40
    sx = (width - 2 * m) / L
    y = 60
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" class="fig" role="img" '
             f'aria-label="Schéma de la poutre">',
             f'<line x1="{m}" y1="{y}" x2="{width - m}" y2="{y}" stroke="#1a1a1a" stroke-width="4"/>']
    for sup in beam_dict["supports"]:
        x = m + sup["x"] * sx
        if sup["type"] == "fixed":
            side = -1 if sup["x"] <= 1e-9 else 1
            parts.append(f'<line x1="{x}" y1="{y - 22}" x2="{x}" y2="{y + 22}" stroke="#1a1a1a" stroke-width="3"/>')
            for k in range(-20, 22, 8):
                parts.append(f'<line x1="{x}" y1="{y + k}" x2="{x + 8 * side}" y2="{y + k - 8}" stroke="#1a1a1a"/>')
        else:
            parts.append(f'<polygon points="{x},{y + 2} {x - 10},{y + 20} {x + 10},{y + 20}" fill="none" '
                         f'stroke="#1a1a1a" stroke-width="1.5"/>')
            if sup["type"] == "roller":
                parts.append(f'<line x1="{x - 12}" y1="{y + 25}" x2="{x + 12}" y2="{y + 25}" stroke="#1a1a1a"/>')
    c = 0.0
    for i, sp in enumerate(beam_dict["spans"]):
        x0, x1 = m + c * sx, m + (c + sp["length"]) * sx
        yd = y + 55
        parts.append(f'<line x1="{x0}" y1="{yd}" x2="{x1}" y2="{yd}" stroke="#555" marker-start="url(#a)" marker-end="url(#a)"/>')
        parts.append(f'<line x1="{x0}" y1="{yd - 6}" x2="{x0}" y2="{yd + 6}" stroke="#555"/>'
                     f'<line x1="{x1}" y1="{yd - 6}" x2="{x1}" y2="{yd + 6}" stroke="#555"/>')
        parts.append(f'<text x="{(x0 + x1) / 2}" y="{yd - 6}" text-anchor="middle" font-size="12">'
                     f'L{i + 1} = {_num(sp["length"])} m</text>')
        parts.append(f'<text x="{(x0 + x1) / 2}" y="{y - 14}" text-anchor="middle" font-size="11" fill="#555">'
                     f'travée {i + 1}</text>')
        c += sp["length"]
    parts.append("</svg>")
    return "".join(parts)


def _mpl_svg(draw) -> Optional[str]:
    try:
        import matplotlib
        matplotlib.use("Agg", force=False)
        import matplotlib.pyplot as plt
    except ImportError:
        return None
    fig, ax = plt.subplots(figsize=(9.5, 3.4))
    draw(ax)
    fig.tight_layout()
    buf = io.StringIO()
    fig.savefig(buf, format="svg")
    plt.close(fig)
    svg = buf.getvalue()
    return svg[svg.index("<svg"):]


def _figure_envelope(res, q):
    def draw(ax):
        res.plot(q, ax=ax)
    return _mpl_svg(draw)


def _figure_influence(res, q, x):
    def draw(ax):
        lx, v, xs = res.influence_line(q, x)
        ax.plot(lx, v, color="#1f4e79")
        ax.fill_between(lx, 0, v, color="#1f4e79", alpha=0.12)
        for s in res.support_positions:
            ax.axvline(s, color="0.6", linestyle=":", linewidth=0.8)
        ax.axhline(0, color="0.3", linewidth=0.8)
        ax.set_xlabel("position de la charge unité [m]")
        ax.set_ylabel(f"{_LABELS[q]} par kN")
        ax.set_title(f"Ligne d'influence de {_LABELS[q]} en x = {xs:.3f} m")
        ax.grid(alpha=0.3)
    return _mpl_svg(draw)


# =============================================================================
#  Génération
# =============================================================================
_CSS = """
body{font-family:'Segoe UI',Helvetica,Arial,sans-serif;color:#1a1a1a;max-width:900px;margin:0 auto;padding:24px;line-height:1.45;background:#fff}
h1{font-size:26px;margin:0 0 6px} h2{font-size:18px;border-bottom:2px solid #1f4e79;padding-bottom:4px;margin-top:34px;color:#1f4e79}
h3{font-size:15px;margin-top:20px}
table{border-collapse:collapse;width:100%;margin:10px 0;font-size:13px}
th,td{border:1px solid #c8ced6;padding:5px 8px;text-align:left;vertical-align:top}
th{background:#eef2f7} td.n{text-align:right;font-variant-numeric:tabular-nums;font-family:Consolas,monospace}
.cover{border:2px solid #1f4e79;padding:28px;margin-bottom:24px;page-break-after:always}
.eq{font-family:'Cambria Math',Cambria,serif;background:#f6f8fa;padding:8px 12px;margin:8px 0;border-left:3px solid #1f4e79}
.ok{color:#1e7b34;font-weight:600} .ko{color:#b3261e;font-weight:600}
.note{font-size:12px;color:#555} svg.fig,figure svg{width:100%;height:auto}
figure{margin:12px 0} figcaption{font-size:12px;color:#555;text-align:center}
@media print{body{max-width:none;padding:0} h2{page-break-after:avoid} figure,table{page-break-inside:avoid}}
"""


def generate_report(res, path: Union[str, Path] = "note_de_calcul.html", *,
                    project_title: str = "Étude d'une poutre continue",
                    project_ref: str = "", author: str = "", company: str = "",
                    convergence: bool = False) -> Path:
    """Génère la note de calcul (HTML, ou PDF si ``path`` finit par ``.pdf``).

    ``convergence=True`` ré-exécute l'analyse avec les pas 2h et h/2 et
    ajoute l'étude de convergence (coût : deux calculs supplémentaires).
    """
    from . import __version__

    path = Path(path)
    beam = res.beam
    bd = beam.to_dict()
    out = res.output
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    checks = run_checks(res)
    conv = _convergence(res) if convergence else None
    S = []

    # 1-2. Page de garde + identification
    S.append(f"""<div class="cover"><h1>{_e(project_title)}</h1>
<p>Note de calcul — poutre continue sous charges mobiles</p>
<table><tr><th>Référence</th><td>{_e(project_ref) or '—'}</td></tr>
<tr><th>Auteur</th><td>{_e(author) or '—'}</td></tr><tr><th>Organisme</th><td>{_e(company) or '—'}</td></tr>
<tr><th>Date de génération</th><td>{now}</td></tr>
<tr><th>Logiciel</th><td>Tsaraloha.LIPoutreContinue {__version__} (moteur C++)</td></tr></table>
<p class="note">Document généré automatiquement à partir des données d'entrée et des résultats calculés
reproduits ci-dessous. Il appartient à l'ingénieur de vérifier l'adéquation du modèle au projet.</p></div>""")

    # 3-6. Structure, géométrie, matériaux, appuis
    rows = "".join(f"<tr><td>{i + 1}</td><td class=n>{_num(s['length'])}</td><td class=n>{_num(s['E'])}</td>"
                   f"<td class=n>{_num(s['I'])}</td><td class=n>{_num(s['E'] * s['I'])}</td></tr>"
                   for i, s in enumerate(bd["spans"]))
    sup = "".join(f"<tr><td>{i}</td><td class=n>{_num(s['x'])}</td><td>{ {'pin': 'appui simple (articulé)', 'roller': 'appui simple (glissant)', 'fixed': 'encastrement'}[s['type']] }</td></tr>"
                  for i, s in enumerate(bd["supports"]))
    S.append(f"""<h2>1. Description de la structure</h2>
<p>Poutre continue de {len(bd['spans'])} travée(s), longueur totale {_num(bd['total_length'])} m.</p>
<figure>{_beam_svg(bd)}<figcaption>Figure 1 — Schéma de la poutre et des appuis</figcaption></figure>
<h3>Géométrie et propriétés mécaniques</h3>
<table><tr><th>Travée</th><th>L [m]</th><th>E [Pa]</th><th>I [m⁴]</th><th>EI [N·m²]</th></tr>{rows}</table>
<h3>Conditions d'appui</h3><table><tr><th>Appui</th><th>x [m]</th><th>Type</th></tr>{sup}</table>""")

    # 7-10. Hypothèses, modèle, méthode, équations
    S.append("""<h2>2. Hypothèses et modèle mathématique</h2>
<ul><li>Poutre d'Euler-Bernoulli, matériau élastique linéaire, petits déplacements ; déformation d'effort
tranchant négligée ; EI constant par travée.</li>
<li>Appuis ponctuels indéformables verticalement ; encastrement = rotation bloquée (modélisé par une travée de
souplesse nulle).</li>
<li>Charges verticales descendantes, superposition linéaire des effets.</li>
<li>Conventions : M &gt; 0 fibre inférieure tendue ; V selon la statique de la travée ; w &gt; 0 vers le haut ;
R &gt; 0 vers le haut. Unités : m, kN, kN·m, Pa, m⁴, rad.</li></ul>
<h3>Équations fondamentales</h3>
<p>Équation des trois moments (Clapeyron) à l'appui i, pour une charge dans les travées adjacentes :</p>
<div class="eq">bᵢ Mᵢ₋₁ + (cᵢ + aᵢ₊₁) Mᵢ + bᵢ₊₁ Mᵢ₊₁ = ω′ᵢ₊₁ − ω″ᵢ &nbsp;&nbsp; avec aᵢ = cᵢ = Lᵢ/(3EIᵢ), bᵢ = Lᵢ/(6EIᵢ)</div>
<p>ω′, ω″ : rotations d'extrémité de la travée isostatique associée (charge unité en a, b = L − a) :</p>
<div class="eq">ω′ = −a b (L + b) / (6 E I L) &nbsp;&nbsp;&nbsp; ω″ = a b (L + a) / (6 E I L)</div>
<p>Résolution par la méthode des foyers (rapports φᵢ, φ′ᵢ) :</p>
<div class="eq">φ₁ = 0, &nbsp; φᵢ = bᵢ / (aᵢ + cᵢ₋₁ − bᵢ₋₁ φᵢ₋₁)</div>
<p>Sollicitations en x d'une travée (M₀, V₀, w₀ : solution isostatique) :</p>
<div class="eq">M(x) = M₀(x) + Mᵢ₋₁ (1 − x/L) + Mᵢ x/L &nbsp;&nbsp; V(x) = V₀(x) + (Mᵢ − Mᵢ₋₁)/L</div>
<div class="eq">Rᵢ = R⁰ᵢ + (Mᵢ₋₁ − Mᵢ)/Lᵢ₋₁ + (Mᵢ₊₁ − Mᵢ)/Lᵢ</div>""")

    n_per_span = [len(s) for s in out.span_node_positions]
    S.append(f"""<h2>3. Méthode numérique et discrétisation</h2>
<p>Les lignes d'influence sont calculées exactement aux nœuds de calcul (pas h = {_num(res.step)} m,
{len(res.x)} nœuds, soit {', '.join(map(str, n_per_span))} par travée). Pour les charges mobiles, chaque ligne
d'influence est l'interpolant linéaire par morceaux de ses valeurs nodales. La position la plus défavorable est
recherchée <b>exactement</b> sur cet interpolant : un essieu sur chaque nœud pour les convois, et intégration
exacte avec annulation de la dérivée pour les charges réparties. Toutes les positions sont explorées, y compris
l'entrée et la sortie partielles.</p>
<p>Enveloppes de charges indépendantes : E⁺ = Σ max, E⁻ = Σ min (une charge peut être absente).</p>
<p class="note">Erreur de discrétisation : nulle pour M et V isostatiques ; O(h²) pour les grandeurs
hyperstatiques, dont les lignes d'influence sont cubiques entre les nœuds.</p>""")

    # 12. Charges
    lrows = []
    for ld in getattr(res, "loads", ()):
        if hasattr(ld, "axles"):
            lrows.append(f"<tr><td>{_e(ld.name)}</td><td>Convoi</td><td>{', '.join(_num(a) for a in ld.axles)} kN</td>"
                         f"<td>{', '.join(_num(s) for s in ld.spacings) or '—'} m</td></tr>")
        else:
            qs, ls = ld._segments(beam.total_length)
            lrows.append(f"<tr><td>{_e(ld.name)}</td><td>Répartie</td><td>{', '.join(_num(q) for q in qs)} kN/m</td>"
                         f"<td>{', '.join(_num(l) for l in ls)} m</td></tr>")
    S.append("<h2>4. Charges mobiles</h2>" + (
        "<table><tr><th>Nom</th><th>Type</th><th>Intensités</th><th>Entraxes / longueurs</th></tr>"
        + "".join(lrows) + "</table>" if lrows else "<p>Aucune charge mobile : seules les lignes d'influence sont présentées.</p>"))

    # 13. Lignes d'influence
    ile = out.influence_line_extrema
    irows = "".join(
        f"<tr><td>{_LABELS[q]}</td><td class=n>{_num(ile[k]['max']['value'] * f)}</td><td class=n>{_num(ile[k]['max']['section_x'])}</td>"
        f"<td class=n>{_num(ile[k]['max']['load_x'])}</td><td class=n>{_num(ile[k]['min']['value'] * f)}</td>"
        f"<td class=n>{_num(ile[k]['min']['section_x'])}</td><td class=n>{_num(ile[k]['min']['load_x'])}</td></tr>"
        for q, k, f in (("M", "bending_moment", 1), ("V", "shear_force", 1), ("w", "deflection", 1e3), ("theta", "rotation", 1e3)))
    S.append(f"""<h2>5. Lignes d'influence</h2>
<p>Valeurs extrêmes des lignes d'influence par kN de charge unité (section x_s, position de la charge x_c).</p>
<table><tr><th>Grandeur</th><th>max</th><th>x_s [m]</th><th>x_c [m]</th><th>min</th><th>x_s [m]</th><th>x_c [m]</th></tr>{irows}</table>""")
    fig = _figure_influence(res, "M", ile["bending_moment"]["min"]["section_x"])
    if fig:
        S.append(f"<figure>{fig}<figcaption>Ligne d'influence du moment à la section du moment minimal</figcaption></figure>")

    # 14-17. Enveloppes, positions critiques, valeurs caractéristiques
    if res._has_loads:
        erows, crows = [], []
        for q in ("M", "V", "w", "theta"):
            for kind in ("point", "distributed", "combined"):
                e = res.envelope(q, kind)
                erows.append(f"<tr><td>{_LABELS[q]}</td><td>{_KIND_LABELS[kind]}</td>"
                             f"<td class=n>{_num(e.max_extremum.value)}</td><td class=n>{_num(e.max_extremum.x)}</td>"
                             f"<td class=n>{_num(e.min_extremum.value)}</td><td class=n>{_num(e.min_extremum.x)}</td><td>{_UNITS[q]}</td></tr>")
            e = res.envelope(q)
            for label, ex in (("max", e.max_extremum), ("min", e.min_extremum)):
                det = "; ".join(f"{_e(n)} : {_num(d['value'])} {_UNITS[q]} (origine {_num(d['position'])} m)"
                                for n, d in ex.loads.items())
                crows.append(f"<tr><td>{_LABELS[q]} {label}</td><td class=n>{_num(ex.value)} {_UNITS[q]}</td>"
                             f"<td class=n>{_num(ex.x)}</td><td>{det}</td></tr>")
        S.append("<h2>6. Enveloppes des sollicitations</h2>")
        for i, q in enumerate(("M", "V", "w")):
            fig = _figure_envelope(res, q)
            if fig:
                S.append(f"<figure>{fig}<figcaption>Enveloppe de {_LABELS[q]} (combinaison)</figcaption></figure>")
        S.append("<table><tr><th>Grandeur</th><th>Charges</th><th>max</th><th>x [m]</th><th>min</th><th>x [m]</th><th>Unité</th></tr>"
                 + "".join(erows) + "</table>")
        S.append("<h2>7. Positions critiques des charges</h2><p>Combinaison ; « origine » = abscisse du premier essieu "
                 "ou du début du bloc réparti. « — » : la charge est absente de la configuration la plus défavorable.</p>"
                 "<table><tr><th>Extrême</th><th>Valeur</th><th>Section x [m]</th><th>Contributions</th></tr>"
                 + "".join(crows) + "</table>")
        rrows = "".join(f"<tr><td>{r['support']}</td><td class=n>{_num(r['x'])}</td><td class=n>{_num(r['max'])}</td>"
                        f"<td class=n>{_num(r['min'])}</td></tr>" for r in res.reactions())
        S.append("<h2>8. Réactions d'appui</h2><table><tr><th>Appui</th><th>x [m]</th><th>R max [kN]</th><th>R min [kN]</th></tr>"
                 + rrows + "</table>")

    # 18. Vérifications
    vrows = "".join(f"<tr><td>{_e(c['name'])}</td><td class=n>{_num(c['measure'])}</td><td class=n>&lt; {_num(c['threshold'])}</td>"
                    f"<td class={'ok' if c['passed'] else 'ko'}>{'conforme' if c['passed'] else 'NON CONFORME'}</td>"
                    f"<td class=note>{_e(c['description'])}</td></tr>" for c in checks)
    S.append("<h2>9. Vérifications d'équilibre et de cohérence</h2><p>Contrôles exécutés lors de la génération "
             "de ce document sur les résultats du moteur.</p><table><tr><th>Contrôle</th><th>Mesure</th>"
             "<th>Seuil</th><th>Résultat</th><th>Définition</th></tr>" + vrows + "</table>")

    # 19. Convergence
    if conv:
        def cell(r, q):
            d = r.get("delta", {}).get(q)
            return f"<td class=n>{_num(r[q], 6)}</td><td class=n>{_num(d, 2) if d is not None else '—'}</td>"
        crow = "".join(f"<tr><td class=n>{_num(r['step'])}</td><td class=n>{r['nodes']}</td>"
                       + "".join(cell(r, q) for q in ("M", "V", "w")) + "</tr>" for r in conv["rows"])
        S.append("<h2>10. Étude de convergence</h2><p>Analyse ré-exécutée avec les pas 2h, h et h/2 (valeurs "
                 "gouvernantes des enveloppes combinées). Δ = écart relatif par rapport au pas précédent.</p>"
                 "<table><tr><th>h [m]</th><th>Nœuds</th><th>M [kN·m]</th><th>Δ</th><th>V [kN]</th><th>Δ</th>"
                 f"<th>w [m]</th><th>Δ</th></tr>{crow}</table>"
                 "<p class=note>L'extrême est recherché aux sections de calcul (nœuds) : l'erreur de discrétisation "
                 "décroît en O(h²) mais avec une constante irrégulière, selon la position de l'extrême vrai entre "
                 "deux nœuds. Δ mesure directement la sensibilité au pas retenu.</p>")
    else:
        S.append("<h2>10. Étude de convergence</h2><p>Non exécutée pour ce document "
                 "(option <code>convergence=True</code>).</p>")

    # 20. Conclusion
    n_ko = sum(not c["passed"] for c in checks)
    S.append(f"""<h2>11. Conclusion et limites du modèle</h2>
<p>{'Toutes les vérifications exécutées sont conformes.' if n_ko == 0 else f'<span class=ko>{n_ko} vérification(s) non conforme(s) : les résultats ne doivent pas être utilisés en l’état.</span>'}</p>
<ul><li>Modèle linéaire élastique d'Euler-Bernoulli, sans déformation d'effort tranchant ni effets du second ordre.</li>
<li>Porte-à-faux, rotules, appuis élastiques et inertie variable dans une travée ne sont pas modélisés.</li>
<li>Charges réparties : bloc rigide de longueur fixée (le chargement des seules zones défavorables n'est pas
recherché). Convois : un seul sens de circulation par charge déclarée.</li>
<li>Erreur de discrétisation O(h²) pour les grandeurs hyperstatiques (voir § 3 et § 10).</li>
<li>Aucune vérification réglementaire (résistance, ELS, coefficients partiels) n'est effectuée par ce document.</li></ul>""")

    doc = (f"<!doctype html><html lang=fr><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
           f"<title>{_e(project_title)}</title><style>{_CSS}</style></head><body>{''.join(S)}</body></html>")

    if path.suffix.lower() == ".pdf":
        try:
            import weasyprint
        except ImportError as exc:
            raise ImportError("La sortie PDF nécessite weasyprint : pip install \"Tsaraloha[report]\". "
                              "Sinon, générez le HTML (extension .html) et imprimez-le en PDF.") from exc
        weasyprint.HTML(string=doc).write_pdf(str(path))
    else:
        path.write_text(doc, encoding="utf-8")
    return path


# =============================================================================
#  Compatibilité 1.x (Output.generate_calculation_note / _pdf)
# =============================================================================
def _results_from_output(output):
    from .api import Beam, Results
    E, I, L = getattr(output, "_declared_model", (list(output.E_spans), list(output.I_spans), list(output.L_spans)))
    beam = Beam()
    for e, i, l in zip(E, I, L):
        if l > 0:
            beam.add_span(l, e, i)
    bounds = beam.boundaries
    for x in bounds:
        beam.add_support(x, "pin")
    if L and L[0] == 0:
        beam.add_support(bounds[0], "fixed")
    if L and L[-1] == 0:
        beam.add_support(bounds[-1], "fixed")
    has_loads = bool(output.is_load_envelopes_computed)
    res = Results(beam, output, float(output.steps), has_loads)
    res.loads = ()
    return res


def generate_calculation_note(output, output_path="note_de_calcul.html", *, project_title="Étude de poutre continue",
                              project_ref="", engineer_name="", company_name="", logo_svg=None, curve_plotter=None):
    """Compatibilité 1.x : préférez ``Beam.analyze(...).report(path)``."""
    warnings.warn("generate_calculation_note(output, ...) est déprécié : utilisez Beam(...).analyze(...).report(path).",
                  DeprecationWarning, stacklevel=2)
    if logo_svg is not None or curve_plotter is not None:
        warnings.warn("logo_svg et curve_plotter ne sont plus pris en charge et sont ignorés.", stacklevel=2)
    return str(generate_report(_results_from_output(output), output_path, project_title=project_title,
                               project_ref=project_ref, author=engineer_name, company=company_name))


def generate_calculation_pdf(output, output_path="note_de_calcul.pdf", **kwargs):
    """Compatibilité 1.x : préférez ``Beam.analyze(...).report("note.pdf")``."""
    path = str(output_path)
    if not path.lower().endswith(".pdf"):
        path += ".pdf"
    return generate_calculation_note(output, path, **kwargs)
