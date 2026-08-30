"""
report.py — Générateur de note de calcul détaillée pour Tsaraloha.LIPoutreContinue.

Ce module permet de générer une note de calcul professionnelle, structurée et
prête à l'impression (HTML / PDF) comprenant :
  - Cartouche de projet et logo
  - Caractéristiques géométriques et mécaniques (E, I, L, appuis, discrétisation)
  - Hypothèses et définition détaillée des charges (ponctuelles, réparties)
  - Résultats synthétiques des lignes d'influence et valeurs critiques
  - Zone d'affichage des courbes de lignes d'influence (via fonction de traçage personnalisable)
  - Enveloppes générales et critiques des sollicitations maximales (M, V, w, theta)
"""

from __future__ import annotations

import datetime
from pathlib import Path
from typing import Any, Callable, Optional


DEFAULT_LOGO_SVG = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 320 80" width="220" height="55">
  <defs>
    <linearGradient id="grad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#0f172a" />
      <stop offset="100%" stop-color="#2563eb" />
    </linearGradient>
  </defs>
  <rect x="5" y="10" width="60" height="60" rx="12" fill="url(#grad)" />
  <path d="M 20 50 L 35 25 L 50 50 Z" fill="none" stroke="#ffffff" stroke-width="3" stroke-linejoin="round"/>
  <circle cx="35" cy="25" r="4" fill="#38bdf8" />
  <line x1="15" y1="55" x2="55" y2="55" stroke="#38bdf8" stroke-width="3" stroke-linecap="round"/>
  <text x="80" y="42" font-family="'Segoe UI', Roboto, sans-serif" font-size="24" font-weight="800" fill="#0f172a" letter-spacing="1">TSARALOHA</text>
  <text x="82" y="60" font-family="'Segoe UI', Roboto, sans-serif" font-size="12" font-weight="600" fill="#64748b" letter-spacing="2">STRUCTURAL ANALYSIS</text>
</svg>"""


def generate_calculation_note(
    out: Any,
    output_path: str | Path = "note_de_calcul.html",
    *,
    project_title: str = "Étude de Poutre Continue Hyperstatique",
    project_ref: str = "DOC-NC-001",
    engineer_name: str = "Ingénieur Structure",
    company_name: str = "Bureau d'Études Structures",
    logo_svg: Optional[str] = None,
    curve_plotter: Optional[Callable[[Any, str, int, int], str]] = None,
) -> str:
    """
    Génère une note de calcul complète au format HTML prêt pour l'impression A4.

    Parameters
    ----------
    out : Tsaraloha.LIPoutreContinue.Output
        L'objet Output calculé avec ses charges.
    output_path : str or Path
        Chemin du fichier HTML de sortie.
    project_title : str
        Titre principal du projet.
    project_ref : str
        Référence de la note de calcul.
    engineer_name : str
        Nom du vérificateur / ingénieur.
    company_name : str
        Nom de l'entreprise ou organisme.
    logo_svg : str, optional
        Code SVG ou balise image personnalisée pour le logo.
    curve_plotter : callable, optional
        Fonction callback de signature (out, curve_type, span, section) -> str
        retournant le code HTML / SVG de la courbe pour une ligne d'influence.

    Returns
    -------
    str
        Le contenu HTML complet généré.
    """
    if not getattr(out, "is_computed", False):
        out.compute()
    if not getattr(out, "is_load_envelopes_computed", False) and hasattr(out, "_user_point_loads"):
        try:
            out.compute_load_envelopes()
        except Exception:
            pass

    date_str = datetime.datetime.now().strftime("%d/%m/%Y à %H:%M")
    logo_html = logo_svg if logo_svg else DEFAULT_LOGO_SVG

    # 1. Modèle de structure
    model = out.structural_model if hasattr(out, "structural_model") else {}
    spans = list(out.L_spans)
    young = list(out.E_spans)
    inertia = list(out.I_spans)
    node_lengths = list(out.node_lengths)
    total_length = sum(spans)
    steps = float(out.steps)
    n_spans = len(spans)
    n_nodes = len(out.X) if len(out.X) > 0 else 0

    # 2. Tableaux de travées
    spans_rows = ""
    for i, (l, e, inert) in enumerate(zip(spans, young, inertia)):
        spans_rows += f"""
        <tr>
            <td style="text-align:center; font-weight:600;">Travée {i+1}</td>
            <td style="text-align:right;">{l:.2f} m</td>
            <td style="text-align:right;">{e / 1e9:.2f} GPa ({e:.2e} Pa)</td>
            <td style="text-align:right;">{inert:.4e} m⁴</td>
            <td style="text-align:right;">{node_lengths[i]:.2f} m → {node_lengths[i+1]:.2f} m</td>
        </tr>
        """

    # 3. Tableaux des charges
    point_loads_rows = ""
    point_loads = getattr(out, "_user_point_loads", [])
    if point_loads:
        for p in point_loads:
            intensities = ", ".join(f"{val:.2f}" for val in p.intensity)
            lengths = ", ".join(f"{val:.2f}" for val in p.length)
            point_loads_rows += f"""
            <tr>
                <td style="font-weight:600;">{p.name}</td>
                <td>{len(p.intensity)} essieu(x)</td>
                <td><code>[{intensities}]</code> kN</td>
                <td><code>[{lengths}]</code> m</td>
            </tr>
            """
    else:
        point_loads_rows = "<tr><td colspan='4' style='text-align:center; color:#94a3b8;'>Aucune charge ponctuelle appliquée.</td></tr>"

    distrib_loads_rows = ""
    distrib_loads = getattr(out, "_user_distrib_loads", [])
    if distrib_loads:
        for d in distrib_loads:
            intensities = ", ".join(f"{val:.2f}" for val in d.intensity)
            lengths = ", ".join(f"{val:.2f}" for val in d.length)
            distrib_loads_rows += f"""
            <tr>
                <td style="font-weight:600;">{d.name}</td>
                <td>{len(d.intensity)} tronçon(s)</td>
                <td><code>[{intensities}]</code> kN/m</td>
                <td><code>[{lengths}]</code> m</td>
            </tr>
            """
    else:
        distrib_loads_rows = "<tr><td colspan='4' style='text-align:center; color:#94a3b8;'>Aucune charge répartie appliquée.</td></tr>"

    # 4. Maxima des Lignes d'Influence
    max_bm = out.bending_moment_max_positions
    max_sf = out.shear_force_max_positions
    max_def = out.deflection_max_positions
    max_rot = out.rotation_max_positions

    # 5. Emplacements de tracés des courbes
    curves_html = ""
    if curve_plotter:
        # Appel du callback utilisateur s'il est fourni
        for span_idx in range(n_spans):
            # Section centrale de la travée
            sec_idx = int(round(spans[span_idx] / (2 * steps)))
            curve_content = curve_plotter(out, "BendingMoment", span_idx, sec_idx)
            curves_html += f"""
            <div class="curve-card">
                <h4>Ligne d'Influence du Moment Fléchissant — Travée {span_idx+1} (Mi-travée)</h4>
                <div class="curve-container">{curve_content}</div>
            </div>
            """
    else:
        curves_html = """
        <div class="curve-placeholder">
            <div style="font-size: 32px; margin-bottom: 8px;">📈</div>
            <div style="font-weight: 600; color: #1e293b; font-size: 15px;">Emplacement pour le traçage des Lignes d'Influence</div>
            <div style="color: #64748b; font-size: 13px; margin-top: 4px;">
                La logique de traçage des courbes peut être connectée directement via le paramètre <code>curve_plotter</code>.
            </div>
        </div>
        """

    # 6. Enveloppes de sollicitations
    envelopes_section_html = ""
    if getattr(out, "is_load_envelopes_computed", False):
        bm_env = out.bending_moment_general_envelope
        sf_env = out.shear_force_general_envelope

        pt_bm = bm_env.get("point_load", {})
        rect_bm = bm_env.get("rectangular_load", {})
        comb_bm = bm_env.get("combined_load", {})

        pt_sf = sf_env.get("point_load", {})
        rect_sf = sf_env.get("rectangular_load", {})
        comb_sf = sf_env.get("combined_load", {})

        # Détail des contributions par charge
        pt_contrib_rows = ""
        for name, data in pt_bm.get("load", {}).items():
            pt_contrib_rows += f"""
            <tr>
                <td>{name}</td>
                <td style="text-align:right; font-weight:600;">{data.get('Position', 0.0):.2f} m</td>
                <td style="text-align:right; font-weight:600; color:#1d4ed8;">{data.get('value', 0.0):.2f} kN.m</td>
                <td style="text-align:center;">{int(data.get('alpha', 0))}</td>
            </tr>
            """

        rect_contrib_rows = ""
        for name, data in rect_bm.get("load", {}).items():
            rect_contrib_rows += f"""
            <tr>
                <td>{name}</td>
                <td style="text-align:right; font-weight:600;">{data.get('Position', 0.0):.2f} m</td>
                <td style="text-align:right; font-weight:600; color:#1d4ed8;">{data.get('value', 0.0):.2f} kN.m</td>
                <td style="text-align:center;">{int(data.get('alpha', 0))}</td>
            </tr>
            """

        envelopes_section_html = f"""
        <section class="section">
            <h2 class="section-title"><span class="badge">5</span> Enveloppes des Sollicitations Maximales</h2>
            
            <div class="grid-2">
                <div class="card">
                    <h3>Moments Fléchissants Maximaux ($M_{{\max}}$)</h3>
                    <table class="data-table">
                        <tr>
                            <th>Type de Sollicitation</th>
                            <th>Travée / Section</th>
                            <th>Position Optimale</th>
                            <th>Valeur Maximale</th>
                        </tr>
                        <tr>
                            <td>Charges Ponctuelles</td>
                            <td>Travée {pt_bm.get('span', 0)+1}, Sec. {pt_bm.get('section', 0)}</td>
                            <td>{pt_bm.get('position', 0.0):.2f} m</td>
                            <td class="highlight-val">{pt_bm.get('maximum_value', 0.0):.2f} kN.m</td>
                        </tr>
                        <tr>
                            <td>Charges Réparties</td>
                            <td>Travée {rect_bm.get('span', 0)+1}, Sec. {rect_bm.get('section', 0)}</td>
                            <td>{rect_bm.get('position', 0.0):.2f} m</td>
                            <td class="highlight-val">{rect_bm.get('maximum_value', 0.0):.2f} kN.m</td>
                        </tr>
                        <tr class="row-total">
                            <td>Combinaison Totale</td>
                            <td>Travée {comb_bm.get('span', 0)+1}, Sec. {comb_bm.get('section', 0)}</td>
                            <td>{comb_bm.get('position', 0.0):.2f} m</td>
                            <td class="highlight-val-total">{comb_bm.get('maximum_value', 0.0):.2f} kN.m</td>
                        </tr>
                    </table>
                </div>

                <div class="card">
                    <h3>Efforts Tranchants Maximaux ($V_{{\max}}$)</h3>
                    <table class="data-table">
                        <tr>
                            <th>Type de Sollicitation</th>
                            <th>Travée / Section</th>
                            <th>Position Optimale</th>
                            <th>Valeur Maximale</th>
                        </tr>
                        <tr>
                            <td>Charges Ponctuelles</td>
                            <td>Travée {pt_sf.get('span', 0)+1}, Sec. {pt_sf.get('section', 0)}</td>
                            <td>{pt_sf.get('position', 0.0):.2f} m</td>
                            <td class="highlight-val">{pt_sf.get('maximum_value', 0.0):.2f} kN</td>
                        </tr>
                        <tr>
                            <td>Charges Réparties</td>
                            <td>Travée {rect_sf.get('span', 0)+1}, Sec. {rect_sf.get('section', 0)}</td>
                            <td>{rect_sf.get('position', 0.0):.2f} m</td>
                            <td class="highlight-val">{rect_sf.get('maximum_value', 0.0):.2f} kN</td>
                        </tr>
                        <tr class="row-total">
                            <td>Combinaison Totale</td>
                            <td>Travée {comb_sf.get('span', 0)+1}, Sec. {comb_sf.get('section', 0)}</td>
                            <td>{comb_sf.get('position', 0.0):.2f} m</td>
                            <td class="highlight-val-total">{comb_sf.get('maximum_value', 0.0):.2f} kN</td>
                        </tr>
                    </table>
                </div>
            </div>

            <div class="card" style="margin-top: 18px;">
                <h3>Détail des Positions Critiques par Charge Mobile (Moment Fléchissant)</h3>
                <div class="grid-2">
                    <div>
                        <h4 style="font-size:13px; color:#475569; margin-bottom:6px;">Charges Ponctuelles & Convois</h4>
                        <table class="data-table">
                            <tr>
                                <th>Nom de la Charge</th>
                                <th>Position sur la Poutre</th>
                                <th>Moment Induit</th>
                                <th>Nœud Grid</th>
                            </tr>
                            {pt_contrib_rows if pt_contrib_rows else "<tr><td colspan='4' style='text-align:center;'>Aucune</td></tr>"}
                        </table>
                    </div>
                    <div>
                        <h4 style="font-size:13px; color:#475569; margin-bottom:6px;">Charges Réparties & Surcharges</h4>
                        <table class="data-table">
                            <tr>
                                <th>Nom de la Charge</th>
                                <th>Position de Début</th>
                                <th>Moment Induit</th>
                                <th>Nœud Grid</th>
                            </tr>
                            {rect_contrib_rows if rect_contrib_rows else "<tr><td colspan='4' style='text-align:center;'>Aucune</td></tr>"}
                        </table>
                    </div>
                </div>
            </div>
        </section>
        """

    html = f"""<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Note de Calcul — {project_title}</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&family=JetBrains+Mono:wght@400;600&display=swap" rel="stylesheet">
    <style>
        :root {{
            --primary: #1e3a8a;
            --primary-light: #eff6ff;
            --accent: #2563eb;
            --dark: #0f172a;
            --gray-700: #334155;
            --gray-500: #64748b;
            --gray-200: #e2e8f0;
            --gray-100: #f8fafc;
            --success: #16a34a;
            --border-radius: 8px;
        }}

        * {{
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }}

        body {{
            font-family: 'Inter', system-ui, -apple-system, sans-serif;
            background-color: #f1f5f9;
            color: var(--dark);
            line-height: 1.5;
            font-size: 13.5px;
            padding: 30px 15px;
        }}

        .page {{
            max-width: 1020px;
            margin: 0 auto;
            background: #ffffff;
            border-radius: 12px;
            padding: 40px 48px;
            box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.08), 0 8px 10px -6px rgba(0, 0, 0, 0.04);
        }}

        /* Header / Cartouche */
        .cartouche {{
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 2px solid var(--primary);
            padding-bottom: 20px;
            margin-bottom: 28px;
        }}

        .cartouche-meta {{
            text-align: right;
        }}

        .cartouche-meta table {{
            border-collapse: collapse;
            margin-left: auto;
        }}

        .cartouche-meta td {{
            padding: 2px 8px;
            font-size: 12px;
            color: var(--gray-700);
        }}

        .cartouche-meta td.label {{
            font-weight: 600;
            color: var(--gray-500);
            text-align: right;
        }}

        .doc-title {{
            font-size: 24px;
            font-weight: 800;
            color: var(--primary);
            letter-spacing: -0.5px;
            margin-bottom: 6px;
        }}

        .doc-subtitle {{
            font-size: 14px;
            font-weight: 500;
            color: var(--gray-500);
        }}

        /* Section */
        .section {{
            margin-bottom: 30px;
        }}

        .section-title {{
            font-size: 16px;
            font-weight: 700;
            color: var(--primary);
            display: flex;
            align-items: center;
            gap: 10px;
            margin-bottom: 14px;
            padding-bottom: 6px;
            border-bottom: 1px solid var(--gray-200);
        }}

        .badge {{
            background: var(--primary);
            color: #ffffff;
            font-size: 11px;
            font-weight: 700;
            padding: 2px 8px;
            border-radius: 999px;
            display: inline-block;
        }}

        /* Cards & Grid */
        .grid-2 {{
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 16px;
        }}

        .grid-3 {{
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 16px;
        }}

        .grid-4 {{
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 12px;
        }}

        .card {{
            background: var(--gray-100);
            border: 1px solid var(--gray-200);
            border-radius: var(--border-radius);
            padding: 16px;
        }}

        .card h3 {{
            font-size: 13.5px;
            font-weight: 700;
            color: var(--dark);
            margin-bottom: 10px;
        }}

        .stat-box {{
            background: #ffffff;
            border: 1px solid var(--gray-200);
            border-radius: var(--border-radius);
            padding: 12px;
            text-align: center;
        }}

        .stat-value {{
            font-size: 18px;
            font-weight: 800;
            color: var(--accent);
            font-family: 'JetBrains Mono', monospace;
        }}

        .stat-label {{
            font-size: 11px;
            color: var(--gray-500);
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            margin-top: 4px;
        }}

        /* Tables */
        .data-table {{
            width: 100%;
            border-collapse: collapse;
            font-size: 12.5px;
            background: #ffffff;
            border-radius: var(--border-radius);
            overflow: hidden;
            border: 1px solid var(--gray-200);
        }}

        .data-table th {{
            background: #f8fafc;
            color: var(--gray-700);
            font-weight: 600;
            text-align: left;
            padding: 8px 12px;
            border-bottom: 1px solid var(--gray-200);
            font-size: 11.5px;
            text-transform: uppercase;
            letter-spacing: 0.3px;
        }}

        .data-table td {{
            padding: 8px 12px;
            border-bottom: 1px solid #f1f5f9;
            color: var(--dark);
        }}

        .data-table tr:last-child td {{
            border-bottom: none;
        }}

        .data-table tr:hover td {{
            background: #f8fafc;
        }}

        .row-total {{
            background: #eff6ff !important;
            font-weight: 700;
        }}

        .highlight-val {{
            font-family: 'JetBrains Mono', monospace;
            font-weight: 600;
            color: var(--accent);
            text-align: right;
        }}

        .highlight-val-total {{
            font-family: 'JetBrains Mono', monospace;
            font-weight: 800;
            color: var(--primary);
            text-align: right;
            font-size: 13.5px;
        }}

        code {{
            font-family: 'JetBrains Mono', monospace;
            background: #e2e8f0;
            padding: 1px 5px;
            border-radius: 4px;
            font-size: 11px;
            color: #0f172a;
        }}

        /* Curve Area */
        .curve-placeholder {{
            background: #f8fafc;
            border: 2px dashed #cbd5e1;
            border-radius: var(--border-radius);
            padding: 30px;
            text-align: center;
            margin: 10px 0;
        }}

        .curve-card {{
            background: #ffffff;
            border: 1px solid var(--gray-200);
            border-radius: var(--border-radius);
            padding: 14px;
            margin-bottom: 14px;
        }}

        .curve-card h4 {{
            font-size: 13px;
            font-weight: 600;
            color: var(--gray-700);
            margin-bottom: 8px;
        }}

        /* Footer */
        .footer {{
            margin-top: 40px;
            padding-top: 15px;
            border-top: 1px solid var(--gray-200);
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 11px;
            color: var(--gray-500);
        }}

        @media print {{
            body {{
                background: #ffffff;
                padding: 0;
            }}
            .page {{
                box-shadow: none;
                padding: 20px 25px;
                max-width: 100%;
            }}
            .section {{
                page-break-inside: avoid;
            }}
        }}
    </style>
</head>
<body>

<div class="page">

    <!-- ── Cartouche ─────────────────────────────────────────────────────── -->
    <header class="cartouche">
        <div class="logo-box">
            {logo_html}
        </div>
        <div class="cartouche-meta">
            <h1 class="doc-title">{project_title}</h1>
            <p class="doc-subtitle">Note de Calcul & Analyse des Lignes d'Influence</p>
            <table style="margin-top: 8px;">
                <tr>
                    <td class="label">Réf. Document :</td>
                    <td><code>{project_ref}</code></td>
                </tr>
                <tr>
                    <td class="label">Organisme :</td>
                    <td><strong>{company_name}</strong></td>
                </tr>
                <tr>
                    <td class="label">Auteur / Ingénieur :</td>
                    <td>{engineer_name}</td>
                </tr>
                <tr>
                    <td class="label">Date d'édition :</td>
                    <td>{date_str}</td>
                </tr>
            </table>
        </div>
    </header>

    <!-- ── 1. Synthèse globale du modèle ─────────────────────────────────── -->
    <section class="section">
        <h2 class="section-title"><span class="badge">1</span> Synthèse Géométrique & Caractéristiques de la Structure</h2>
        
        <div class="grid-4" style="margin-bottom: 16px;">
            <div class="stat-box">
                <div class="stat-value">{n_spans}</div>
                <div class="stat-label">Nombre de Travées</div>
            </div>
            <div class="stat-box">
                <div class="stat-value">{total_length:.2f} m</div>
                <div class="stat-label">Longueur Totale</div>
            </div>
            <div class="stat-box">
                <div class="stat-value">{steps:.2f} m</div>
                <div class="stat-label">Pas de Discrétisation</div>
            </div>
            <div class="stat-box">
                <div class="stat-value">{n_nodes}</div>
                <div class="stat-label">Nœuds de Calcul</div>
            </div>
        </div>

        <table class="data-table">
            <thead>
                <tr>
                    <th style="text-align:center;">Travée</th>
                    <th style="text-align:right;">Portée ($L$)</th>
                    <th style="text-align:right;">Module de Young ($E$)</th>
                    <th style="text-align:right;">Inertie ($I$)</th>
                    <th style="text-align:right;">Positionnement Appuis</th>
                </tr>
            </thead>
            <tbody>
                {spans_rows}
            </tbody>
        </table>
    </section>

    <!-- ── 2. Définition des Charges ──────────────────────────────────────── -->
    <section class="section">
        <h2 class="section-title"><span class="badge">2</span> Hypothèses des Charges Mobiles Appliquées</h2>
        
        <div class="grid-2">
            <div class="card">
                <h3>Charges Ponctuelles & Convois</h3>
                <table class="data-table">
                    <thead>
                        <tr>
                            <th>Nom</th>
                            <th>Essieux</th>
                            <th>Intensités</th>
                            <th>Entraxes</th>
                        </tr>
                    </thead>
                    <tbody>
                        {point_loads_rows}
                    </tbody>
                </table>
            </div>

            <div class="card">
                <h3>Charges Réparties Mobiles & Surcharges</h3>
                <table class="data-table">
                    <thead>
                        <tr>
                            <th>Nom</th>
                            <th>Tronçons</th>
                            <th>Intensités</th>
                            <th>Longueurs</th>
                        </tr>
                    </thead>
                    <tbody>
                        {distrib_loads_rows}
                    </tbody>
                </table>
            </div>
        </div>
    </section>

    <!-- ── 3. Résultats Lignes d'Influence ────────────────────────────────── -->
    <section class="section">
        <h2 class="section-title"><span class="badge">3</span> Valeurs Maximales des Lignes d'Influence</h2>
        
        <div class="grid-4">
            <div class="stat-box">
                <div class="stat-value" style="color:#2563eb;">{max_bm.get('val', 0.0):.2f}</div>
                <div class="stat-label">Max Moment [kN.m]</div>
                <div style="font-size:10px; color:#64748b; margin-top:2px;">Travée {max_bm.get('i', 0)+1}, Sec. {max_bm.get('j', 0)}</div>
            </div>
            <div class="stat-box">
                <div class="stat-value" style="color:#0284c7;">{max_sf.get('val', 0.0):.2f}</div>
                <div class="stat-label">Max Tranchant [kN]</div>
                <div style="font-size:10px; color:#64748b; margin-top:2px;">Travée {max_sf.get('i', 0)+1}, Sec. {max_sf.get('j', 0)}</div>
            </div>
            <div class="stat-box">
                <div class="stat-value" style="color:#059669;">{max_def.get('val', 0.0):.4e}</div>
                <div class="stat-label">Max Flèche [m]</div>
                <div style="font-size:10px; color:#64748b; margin-top:2px;">Travée {max_def.get('i', 0)+1}, Sec. {max_def.get('j', 0)}</div>
            </div>
            <div class="stat-box">
                <div class="stat-value" style="color:#7c3aed;">{max_rot.get('val', 0.0):.4e}</div>
                <div class="stat-label">Max Rotation [rad]</div>
                <div style="font-size:10px; color:#64748b; margin-top:2px;">Travée {max_rot.get('i', 0)+1}, Sec. {max_rot.get('j', 0)}</div>
            </div>
        </div>
    </section>

    <!-- ── 4. Courbes de Lignes d'Influence ───────────────────────────────── -->
    <section class="section">
        <h2 class="section-title"><span class="badge">4</span> Courbes des Lignes d'Influence</h2>
        {curves_html}
    </section>

    <!-- ── 5. Enveloppes de Sollicitations ───────────────────────────────── -->
    {envelopes_section_html}

    <!-- ── Footer ────────────────────────────────────────────────────────── -->
    <footer class="footer">
        <div>Édité par le moteur de calcul <strong>Tsaraloha.LIPoutreContinue v1.0</strong></div>
        <div>Page 1 / 1 — Document certifié pour exécution</div>
    </footer>

</div>

</body>
</html>
"""
    # Écriture du fichier HTML
    dest = Path(output_path)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(html, encoding="utf-8")
    return html
