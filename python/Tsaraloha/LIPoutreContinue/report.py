"""
report.py — Générateur de note de calcul style RDM6 pour Tsaraloha.LIPoutreContinue.

Ce module génère une note de calcul respectant la structure, la rigueur et le
formalisme des rapports d'ingénierie RDM6 / RDM Flexion (IUT Le Mans) :
  - Cartouche normalisé d'ingénierie RDM6 (Projet, Affaire, Auteur, Date)
  - Schéma cinématique de la structure cotée avec nœuds (1, 2, ...) et symboles d'appuis
  - Tableaux des nœuds, travées, caractéristiques mécaniques (E, Iz, S)
  - Conditions aux limites (liaisons, degrés de liberté)
  - Définition détaillée des cas de charges (forces nodales, convois, charges réparties)
  - Synthèse des lignes d'influence et valeurs critiques
  - Zone de diagrammes et courbes de lignes d'influence
  - Enveloppes des sollicitations maximales (Mf, Ty, flèches, rotations)
"""

from __future__ import annotations

import datetime
from pathlib import Path
from typing import Any, Callable, Optional


def _generate_beam_svg(spans: list[float], node_lengths: list[float]) -> str:
    """Génère le schéma structurel coté de la poutre avec symboles d'appuis RDM6."""
    n_spans = len(spans)
    total_l = sum(spans)
    if total_l <= 0:
        return ""

    width = 820
    height = 140
    margin_x = 50
    draw_width = width - 2 * margin_x
    beam_y = 65

    scale = draw_width / total_l

    svg_parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="100%" height="{height}" style="background:#fcfdfd; border:1px solid #d1d5db; border-radius:4px;">',
        '<defs>',
        '  <marker id="arrow" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">',
        '    <path d="M 0 1.5 L 10 5 L 0 8.5 z" fill="#1e3a8a"/>',
        '  </marker>',
        '  <pattern id="hatch" width="6" height="6" patternTransform="rotate(45 0 0)" patternUnits="userSpaceOnUse">',
        '    <line x1="0" y1="0" x2="0" y2="6" stroke="#64748b" stroke-width="1.2" />',
        '  </pattern>',
        '</defs>',
    ]

    # Ligne neutre de la poutre (Poutre continue continue)
    svg_parts.append(
        f'<line x1="{margin_x}" y1="{beam_y}" x2="{margin_x + draw_width}" y2="{beam_y}" stroke="#1e293b" stroke-width="5" stroke-linecap="round"/>'
    )

    # Appuis et Nœuds
    for i, x_cumul in enumerate(node_lengths):
        cx = margin_x + x_cumul * scale
        
        # Symbole Appui Simple / Articulé RDM6 (Triangle + hachures sol)
        app_top_y = beam_y + 2
        app_bot_y = beam_y + 26
        svg_parts.append(
            f'<polygon points="{cx},{app_top_y} {cx-11},{app_bot_y} {cx+11},{app_bot_y}" fill="#0284c7" stroke="#0f172a" stroke-width="1.5"/>'
        )
        svg_parts.append(
            f'<line x1="{cx-16}" y1="{app_bot_y+1}" x2="{cx+16}" y2="{app_bot_y+1}" stroke="#0f172a" stroke-width="2"/>'
        )
        svg_parts.append(
            f'<rect x="{cx-15}" y="{app_bot_y+2}" width="30" height="6" fill="url(#hatch)" stroke="#94a3b8" stroke-width="0.5"/>'
        )

        # Repère du Nœud (cercle avec numéro)
        svg_parts.append(
            f'<circle cx="{cx}" cy="{beam_y}" r="4" fill="#ffffff" stroke="#0f172a" stroke-width="1.5"/>'
        )
        svg_parts.append(
            f'<text x="{cx}" y="{beam_y - 12}" font-family="Consolas, monospace" font-size="12" font-weight="bold" fill="#1e3a8a" text-anchor="middle">({i+1})</text>'
        )

    # Cotes des travées (Lignes de cote style RDM6)
    cote_y = beam_y + 48
    for i, span_len in enumerate(spans):
        x_start = margin_x + node_lengths[i] * scale
        x_end = margin_x + node_lengths[i+1] * scale
        mid_x = (x_start + x_end) / 2

        # Ligne de cote avec flèches
        svg_parts.append(
            f'<line x1="{x_start+4}" y1="{cote_y}" x2="{x_end-4}" y2="{cote_y}" stroke="#1e3a8a" stroke-width="1.2" marker-start="url(#arrow)" marker-end="url(#arrow)"/>'
        )
        # Traits de rappel
        svg_parts.append(
            f'<line x1="{x_start}" y1="{beam_y+10}" x2="{x_start}" y2="{cote_y+6}" stroke="#94a3b8" stroke-width="0.8" stroke-dasharray="2,2"/>'
        )
        svg_parts.append(
            f'<line x1="{x_end}" y1="{beam_y+10}" x2="{x_end}" y2="{cote_y+6}" stroke="#94a3b8" stroke-width="0.8" stroke-dasharray="2,2"/>'
        )
        # Texte de la cote
        svg_parts.append(
            f'<rect x="{mid_x-32}" y="{cote_y-8}" width="64" height="16" fill="#fcfdfd"/>'
        )
        svg_parts.append(
            f'<text x="{mid_x}" y="{cote_y+4}" font-family="Consolas, monospace" font-size="11" font-weight="600" fill="#0f172a" text-anchor="middle">L{i+1} = {span_len:.2f}m</text>'
        )

    svg_parts.append('</svg>')
    return "".join(svg_parts)


def generate_calculation_note(
    out: Any,
    output_path: str | Path = "note_de_calcul_rdm6.html",
    *,
    project_title: str = "Étude de Flexion de Poutre Continue Hyperstatique",
    project_ref: str = "RDM6-TSARALOHA-01",
    engineer_name: str = "Ingénieur Structure",
    company_name: str = "Département Génie Mécanique & Civil",
    logo_svg: Optional[str] = None,
    curve_plotter: Optional[Callable[[Any, str, int, int], str]] = None,
) -> str:
    """
    Génère une note de calcul complète conforme au standard RDM6 / RDM Le Mans.
    """
    if not getattr(out, "is_computed", False):
        out.compute()
    if not getattr(out, "is_load_envelopes_computed", False) and hasattr(out, "_user_point_loads"):
        try:
            out.compute_load_envelopes()
        except Exception:
            pass

    date_str = datetime.datetime.now().strftime("%d/%m/%Y")
    heure_str = datetime.datetime.now().strftime("%H:%M:%S")

    # Géométrie
    spans = list(out.L_spans)
    young = list(out.E_spans)
    inertia = list(out.I_spans)
    node_lengths = list(out.node_lengths)
    total_length = sum(spans)
    steps = float(out.steps)
    n_spans = len(spans)
    n_nodes = len(node_lengths)
    total_mesh_nodes = len(out.X) if len(out.X) > 0 else 0

    # Schéma structurel coté
    beam_schema_svg = _generate_beam_svg(spans, node_lengths)

    # 1. Tableau des Nœuds (Format RDM6)
    nodes_table_rows = ""
    for i, x_pos in enumerate(node_lengths):
        liaison_desc = "Appui simple (Ty = 0)" if (i == 0 or i == n_nodes - 1) else "Appui intermédiaire (Ty = 0)"
        nodes_table_rows += f"""
        <tr>
            <td style="text-align:center; font-weight:bold;">{i+1}</td>
            <td style="text-align:right; font-family:Consolas, monospace;">{x_pos:.3f}</td>
            <td style="text-align:right; font-family:Consolas, monospace;">0.000</td>
            <td style="text-align:left;">{liaison_desc}</td>
        </tr>
        """

    # 2. Tableau des Éléments / Travées (Format RDM6)
    elements_table_rows = ""
    for i, (l, e, inert) in enumerate(zip(spans, young, inertia)):
        e_gpa = e / 1e9
        e_mpa = e / 1e6
        iz_cm4 = inert * 1e8
        elements_table_rows += f"""
        <tr>
            <td style="text-align:center; font-weight:bold;">{i+1}</td>
            <td style="text-align:center;">{i+1}</td>
            <td style="text-align:center;">{i+2}</td>
            <td style="text-align:right; font-family:Consolas, monospace;">{l:.3f}</td>
            <td style="text-align:right; font-family:Consolas, monospace;">{e_mpa:.1f} MPa ({e_gpa:.2f} GPa)</td>
            <td style="text-align:right; font-family:Consolas, monospace;">{inert:.4e} m⁴ ({iz_cm4:.1f} cm⁴)</td>
        </tr>
        """

    # 3. Tableau des Charges
    point_loads = getattr(out, "_user_point_loads", [])
    point_loads_rows = ""
    if point_loads:
        for idx, p in enumerate(point_loads):
            intensities_str = " ; ".join(f"P{k+1} = {val:.2f} kN" for k, val in enumerate(p.intensity))
            lengths_str = " ; ".join(f"d{k+1} = {val:.2f} m" for k, val in enumerate(p.length))
            point_loads_rows += f"""
            <tr>
                <td style="text-align:center; font-weight:bold;">CP-{idx+1}</td>
                <td style="font-weight:600;">{p.name}</td>
                <td>Convoi / Ponctuelle</td>
                <td style="font-family:Consolas, monospace;">{intensities_str}</td>
                <td style="font-family:Consolas, monospace;">{lengths_str}</td>
            </tr>
            """
    else:
        point_loads_rows = "<tr><td colspan='5' style='text-align:center; color:#6b7280;'>Aucune charge ponctuelle appliquée.</td></tr>"

    distrib_loads = getattr(out, "_user_distrib_loads", [])
    distrib_loads_rows = ""
    if distrib_loads:
        for idx, d in enumerate(distrib_loads):
            intensities_str = " ; ".join(f"q{k+1} = {val:.2f} kN/m" for k, val in enumerate(d.intensity))
            lengths_str = f"Départ = {d.length[0]:.2f} m | " + " ; ".join(f"L{k+1} = {val:.2f} m" for k, val in enumerate(d.length[1:]))
            distrib_loads_rows += f"""
            <tr>
                <td style="text-align:center; font-weight:bold;">CR-{idx+1}</td>
                <td style="font-weight:600;">{d.name}</td>
                <td>Répartie Mobile</td>
                <td style="font-family:Consolas, monospace;">{intensities_str}</td>
                <td style="font-family:Consolas, monospace;">{lengths_str}</td>
            </tr>
            """
    else:
        distrib_loads_rows = "<tr><td colspan='5' style='text-align:center; color:#6b7280;'>Aucune charge répartie appliquée.</td></tr>"

    # 4. Maxima L.I.
    max_bm = out.bending_moment_max_positions
    max_sf = out.shear_force_max_positions
    max_def = out.deflection_max_positions
    max_rot = out.rotation_max_positions

    # 5. Emplacements de courbes
    curves_html = ""
    if curve_plotter:
        for span_idx in range(n_spans):
            sec_idx = int(round(spans[span_idx] / (2 * steps)))
            curve_content = curve_plotter(out, "BendingMoment", span_idx, sec_idx)
            curves_html += f"""
            <div class="rdm-block">
                <div class="rdm-block-header">DIAGRAMME DE LA LIGNE D'INFLUENCE — MOMENT FLÉCHISSANT Mz (Travée {span_idx+1}, Mi-travée)</div>
                <div style="padding: 12px; background:#ffffff; text-align:center;">{curve_content}</div>
            </div>
            """
    else:
        curves_html = """
        <div class="rdm-curve-placeholder">
            <div style="font-weight:bold; font-size:13px; color:#1e3a8a;">[ ZONE GRAPHIQUE — DIAGRAMMES DES LIGNES D'INFLUENCE ]</div>
            <div style="font-size:12px; color:#4b5563; margin-top:4px;">
                Connectez votre fonction de traçage de courbes (SVG / Matplotlib / Canvas) via <code>curve_plotter</code>.
            </div>
        </div>
        """

    # 6. Enveloppes
    envelopes_html = ""
    if getattr(out, "is_load_envelopes_computed", False):
        bm_env = out.bending_moment_general_envelope
        sf_env = out.shear_force_general_envelope

        pt_bm = bm_env.get("point_load", {})
        rect_bm = bm_env.get("rectangular_load", {})
        comb_bm = bm_env.get("combined_load", {})

        pt_sf = sf_env.get("point_load", {})
        rect_sf = sf_env.get("rectangular_load", {})
        comb_sf = sf_env.get("combined_load", {})

        # Détails par charge
        pt_contrib_rows = ""
        for name, data in pt_bm.get("load", {}).items():
            pt_contrib_rows += f"""
            <tr>
                <td style="font-weight:600;">{name}</td>
                <td style="text-align:right; font-family:Consolas, monospace;">{data.get('Position', 0.0):.3f} m</td>
                <td style="text-align:right; font-family:Consolas, monospace; font-weight:bold; color:#000080;">{data.get('value', 0.0):.2f} kN.m</td>
                <td style="text-align:center; font-family:Consolas, monospace;">{int(data.get('alpha', 0))}</td>
            </tr>
            """

        rect_contrib_rows = ""
        for name, data in rect_bm.get("load", {}).items():
            rect_contrib_rows += f"""
            <tr>
                <td style="font-weight:600;">{name}</td>
                <td style="text-align:right; font-family:Consolas, monospace;">{data.get('Position', 0.0):.3f} m</td>
                <td style="text-align:right; font-family:Consolas, monospace; font-weight:bold; color:#000080;">{data.get('value', 0.0):.2f} kN.m</td>
                <td style="text-align:center; font-family:Consolas, monospace;">{int(data.get('alpha', 0))}</td>
            </tr>
            """

        envelopes_html = f"""
        <div class="rdm-section-title">5. ENVELOPPES DES SOLLICITATIONS MAXIMALES & EXTREMA</div>
        
        <div class="rdm-grid-2">
            <div class="rdm-block">
                <div class="rdm-block-header">MOMENT FLÉCHISSANT MAXIMAL | Mz,max (kN.m)</div>
                <table class="rdm-table">
                    <thead>
                        <tr>
                            <th>Cas de Charge</th>
                            <th>Travée / Sec.</th>
                            <th>Position x</th>
                            <th style="text-align:right;">Valeur Extrême</th>
                        </tr>
                    </thead>
                    <tbody>
                        <tr>
                            <td>Charges Ponctuelles</td>
                            <td style="text-align:center;">T{pt_bm.get('span', 0)+1} / S{pt_bm.get('section', 0)}</td>
                            <td style="text-align:right; font-family:Consolas, monospace;">{pt_bm.get('position', 0.0):.2f} m</td>
                            <td style="text-align:right; font-family:Consolas, monospace; font-weight:bold;">{pt_bm.get('maximum_value', 0.0):.2f} kN.m</td>
                        </tr>
                        <tr>
                            <td>Charges Réparties</td>
                            <td style="text-align:center;">T{rect_bm.get('span', 0)+1} / S{rect_bm.get('section', 0)}</td>
                            <td style="text-align:right; font-family:Consolas, monospace;">{rect_bm.get('position', 0.0):.2f} m</td>
                            <td style="text-align:right; font-family:Consolas, monospace; font-weight:bold;">{rect_bm.get('maximum_value', 0.0):.2f} kN.m</td>
                        </tr>
                        <tr style="background:#eef2ff; font-weight:bold;">
                            <td>COMBINAISON ENVELOPPE</td>
                            <td style="text-align:center;">T{comb_bm.get('span', 0)+1} / S{comb_bm.get('section', 0)}</td>
                            <td style="text-align:right; font-family:Consolas, monospace;">{comb_bm.get('position', 0.0):.2f} m</td>
                            <td style="text-align:right; font-family:Consolas, monospace; color:#000080; font-size:13px;">{comb_bm.get('maximum_value', 0.0):.2f} kN.m</td>
                        </tr>
                    </tbody>
                </table>
            </div>

            <div class="rdm-block">
                <div class="rdm-block-header">EFFORT TRANCHANT MAXIMAL | Ty,max (kN)</div>
                <table class="rdm-table">
                    <thead>
                        <tr>
                            <th>Cas de Charge</th>
                            <th>Travée / Sec.</th>
                            <th>Position x</th>
                            <th style="text-align:right;">Valeur Extrême</th>
                        </tr>
                    </thead>
                    <tbody>
                        <tr>
                            <td>Charges Ponctuelles</td>
                            <td style="text-align:center;">T{pt_sf.get('span', 0)+1} / S{pt_sf.get('section', 0)}</td>
                            <td style="text-align:right; font-family:Consolas, monospace;">{pt_sf.get('position', 0.0):.2f} m</td>
                            <td style="text-align:right; font-family:Consolas, monospace; font-weight:bold;">{pt_sf.get('maximum_value', 0.0):.2f} kN</td>
                        </tr>
                        <tr>
                            <td>Charges Réparties</td>
                            <td style="text-align:center;">T{rect_sf.get('span', 0)+1} / S{rect_sf.get('section', 0)}</td>
                            <td style="text-align:right; font-family:Consolas, monospace;">{rect_sf.get('position', 0.0):.2f} m</td>
                            <td style="text-align:right; font-family:Consolas, monospace; font-weight:bold;">{rect_sf.get('maximum_value', 0.0):.2f} kN</td>
                        </tr>
                        <tr style="background:#eef2ff; font-weight:bold;">
                            <td>COMBINAISON ENVELOPPE</td>
                            <td style="text-align:center;">T{comb_sf.get('span', 0)+1} / S{comb_sf.get('section', 0)}</td>
                            <td style="text-align:right; font-family:Consolas, monospace;">{comb_sf.get('position', 0.0):.2f} m</td>
                            <td style="text-align:right; font-family:Consolas, monospace; color:#000080; font-size:13px;">{comb_sf.get('maximum_value', 0.0):.2f} kN</td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </div>

        <div class="rdm-block" style="margin-top:14px;">
            <div class="rdm-block-header">DÉTAIL DES POSITIONS PHYSIQUES CRITIQUES PAR CHARGE</div>
            <div class="rdm-grid-2" style="padding: 10px; gap: 12px;">
                <div>
                    <div style="font-weight:bold; font-size:11px; color:#1e3a8a; margin-bottom:4px;">Charges Ponctuelles / Convois</div>
                    <table class="rdm-table">
                        <thead>
                            <tr>
                                <th>Charge</th>
                                <th>Position x</th>
                                <th>Contribution Mz</th>
                                <th>Nœud Grid</th>
                            </tr>
                        </thead>
                        <tbody>
                            {pt_contrib_rows if pt_contrib_rows else "<tr><td colspan='4' style='text-align:center;'>Aucune</td></tr>"}
                        </tbody>
                    </table>
                </div>
                <div>
                    <div style="font-weight:bold; font-size:11px; color:#1e3a8a; margin-bottom:4px;">Charges Réparties / Surcharges</div>
                    <table class="rdm-table">
                        <thead>
                            <tr>
                                <th>Charge</th>
                                <th>Position Début</th>
                                <th>Contribution Mz</th>
                                <th>Nœud Grid</th>
                            </tr>
                        </thead>
                        <tbody>
                            {rect_contrib_rows if rect_contrib_rows else "<tr><td colspan='4' style='text-align:center;'>Aucune</td></tr>"}
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
        """

    logo_display = logo_svg if logo_svg else """
    <div style="font-family:'Courier New', monospace; font-weight:900; font-size:20px; color:#000080; letter-spacing:1px;">
        [ RDM6 - TSARALOHA ]
    </div>
    <div style="font-size:10px; color:#4b5563; font-family:sans-serif;">RDM Éléments Finis & Flexion Poutres</div>
    """

    html = f"""<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RDM6 — Note de Calcul — {project_title}</title>
    <style>
        @page {{
            size: A4 portrait;
            margin: 12mm 15mm 15mm 15mm;
        }}

        * {{
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }}

        body {{
            font-family: Arial, Helvetica, sans-serif;
            background-color: #e5e7eb;
            color: #111827;
            font-size: 12px;
            line-height: 1.4;
            padding: 20px 10px;
        }}

        .rdm-page {{
            max-width: 960px;
            margin: 0 auto;
            background: #ffffff;
            border: 2px solid #000080;
            padding: 20px 24px;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
        }}

        /* Cartouche RDM6 Normalisé */
        .rdm-cartouche {{
            display: table;
            width: 100%;
            border: 2px solid #000080;
            border-collapse: collapse;
            margin-bottom: 16px;
        }}

        .rdm-cartouche-row {{
            display: table-row;
        }}

        .rdm-cartouche-cell {{
            display: table-cell;
            border: 1px solid #000080;
            padding: 6px 10px;
            vertical-align: middle;
        }}

        .rdm-cartouche-header {{
            background: #f0f4f8;
            font-weight: bold;
            color: #000080;
            text-align: center;
            font-size: 14px;
            padding: 8px;
        }}

        /* Titres de section RDM6 */
        .rdm-section-title {{
            background: #000080;
            color: #ffffff;
            font-weight: bold;
            font-size: 12px;
            padding: 4px 8px;
            margin-top: 18px;
            margin-bottom: 8px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }}

        .rdm-block {{
            border: 1px solid #94a3b8;
            margin-bottom: 12px;
            background: #ffffff;
        }}

        .rdm-block-header {{
            background: #f1f5f9;
            color: #000080;
            font-weight: bold;
            font-size: 11px;
            padding: 4px 8px;
            border-bottom: 1px solid #94a3b8;
        }}

        /* Tableaux RDM6 */
        .rdm-table {{
            width: 100%;
            border-collapse: collapse;
            font-size: 11px;
        }}

        .rdm-table th {{
            background: #f8fafc;
            color: #0f172a;
            border: 1px solid #cbd5e1;
            padding: 5px 6px;
            font-weight: bold;
            text-align: center;
        }}

        .rdm-table td {{
            border: 1px solid #cbd5e1;
            padding: 4px 6px;
            color: #1e293b;
        }}

        .rdm-table tr:nth-child(even) {{
            background-color: #fafafa;
        }}

        /* Grilles */
        .rdm-grid-2 {{
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
        }}

        .rdm-grid-4 {{
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 8px;
        }}

        .rdm-stat {{
            border: 1px solid #cbd5e1;
            padding: 6px;
            text-align: center;
            background: #f8fafc;
        }}

        .rdm-stat-val {{
            font-family: Consolas, monospace;
            font-size: 14px;
            font-weight: bold;
            color: #000080;
        }}

        .rdm-stat-lbl {{
            font-size: 10px;
            color: #475569;
            text-transform: uppercase;
            margin-top: 2px;
        }}

        .rdm-curve-placeholder {{
            border: 1px dashed #000080;
            background: #f8fafc;
            padding: 24px;
            text-align: center;
            margin: 8px 0;
        }}

        .rdm-footer {{
            margin-top: 24px;
            border-top: 1px solid #94a3b8;
            padding-top: 8px;
            font-size: 10px;
            color: #64748b;
            display: flex;
            justify-content: space-between;
        }}

        @media print {{
            body {{
                background: #ffffff;
                padding: 0;
            }}
            .rdm-page {{
                border: none;
                box-shadow: none;
                padding: 0;
                max-width: 100%;
            }}
        }}
    </style>
</head>
<body>

<div class="rdm-page">

    <!-- ── Cartouche RDM6 ────────────────────────────────────────────────── -->
    <div class="rdm-cartouche">
        <div class="rdm-cartouche-row">
            <div class="rdm-cartouche-cell" style="width: 28%; text-align:center;">
                {logo_display}
            </div>
            <div class="rdm-cartouche-cell rdm-cartouche-header" style="width: 44%;">
                NOTE DE CALCUL — FLEXION DES POUTRES CONTINUES
                <div style="font-size:11px; font-weight:normal; color:#334155; margin-top:3px;">
                    {project_title}
                </div>
            </div>
            <div class="rdm-cartouche-cell" style="width: 28%; font-size:11px;">
                <div><strong>Réf :</strong> {project_ref}</div>
                <div><strong>Date :</strong> {date_str} à {heure_str}</div>
                <div><strong>Auteur :</strong> {engineer_name}</div>
                <div><strong>Affaire :</strong> {company_name}</div>
            </div>
        </div>
    </div>

    <!-- ── Schéma Cinématique Coté RDM6 ─────────────────────────────────── -->
    <div class="rdm-block">
        <div class="rdm-block-header">MODÉLISATION GÉOMÉTRIQUE & SCHÉMA CINÉMATIQUE DE LA STRUCTURE</div>
        <div style="padding: 10px 14px;">
            {beam_schema_svg}
        </div>
    </div>

    <!-- ── 1. Nœuds et Conditions aux Limites ────────────────────────────── -->
    <div class="rdm-section-title">1. NŒUDS ET CONDITIONS AUX LIMITES (LIAISONS)</div>
    <div class="rdm-grid-2">
        <div>
            <div class="rdm-block-header">TABLEAU DES NŒUDS & COORDONNÉES</div>
            <table class="rdm-table">
                <thead>
                    <tr>
                        <th>Nœud</th>
                        <th>X (m)</th>
                        <th>Y (m)</th>
                        <th>Liaison / Condition Limite</th>
                    </tr>
                </thead>
                <tbody>
                    {nodes_table_rows}
                </tbody>
            </table>
        </div>
        <div>
            <div class="rdm-block-header">PARAMÈTRES GLOBAUX DU MODÈLE</div>
            <div class="rdm-grid-2" style="padding:8px; gap:8px;">
                <div class="rdm-stat">
                    <div class="rdm-stat-val">{n_spans}</div>
                    <div class="rdm-stat-lbl">Travées</div>
                </div>
                <div class="rdm-stat">
                    <div class="rdm-stat-val">{total_length:.2f} m</div>
                    <div class="rdm-stat-lbl">Longueur Totale</div>
                </div>
                <div class="rdm-stat">
                    <div class="rdm-stat-val">{steps:.2f} m</div>
                    <div class="rdm-stat-lbl">Pas d'Échantillonnage</div>
                </div>
                <div class="rdm-stat">
                    <div class="rdm-stat-val">{total_mesh_nodes}</div>
                    <div class="rdm-stat-lbl">Points de Maillage</div>
                </div>
            </div>
        </div>
    </div>

    <!-- ── 2. Éléments et Caractéristiques Mécaniques ────────────────────── -->
    <div class="rdm-section-title">2. ÉLÉMENTS ET CARACTÉRISTIQUES MÉCANIQUES DES POUTRES</div>
    <table class="rdm-table">
        <thead>
            <tr>
                <th>Élément</th>
                <th>Nœud Origine</th>
                <th>Nœud Extrémité</th>
                <th>Longueur L (m)</th>
                <th>Module de Young E</th>
                <th>Inertie de Flexion Iz</th>
            </tr>
        </thead>
        <tbody>
            {elements_table_rows}
        </tbody>
    </table>

    <!-- ── 3. Cas de Charges ─────────────────────────────────────────────── -->
    <div class="rdm-section-title">3. HYPOTHÈSES DES CHARGES MOBILES (INVENTAIRE DU CONVOI)</div>
    
    <div class="rdm-block">
        <div class="rdm-block-header">CHARGES PONCTUELLES & CONVOIS D'ESSIEUX</div>
        <table class="rdm-table">
            <thead>
                <tr>
                    <th>Réf.</th>
                    <th>Nom du Convoi</th>
                    <th>Type</th>
                    <th>Forces par Essieu</th>
                    <th>Entraxes / Distances relatives</th>
                </tr>
            </thead>
            <tbody>
                {point_loads_rows}
            </tbody>
        </table>
    </div>

    <div class="rdm-block">
        <div class="rdm-block-header">CHARGES RÉPARTIES MOBILES & SURCHARGES</div>
        <table class="rdm-table">
            <thead>
                <tr>
                    <th>Réf.</th>
                    <th>Nom de la Surcharge</th>
                    <th>Type</th>
                    <th>Intensités par Tronçon</th>
                    <th>Position Départ & Longueurs</th>
                </tr>
            </thead>
            <tbody>
                {distrib_loads_rows}
            </tbody>
        </table>
    </div>

    <!-- ── 4. Lignes d'Influence ─────────────────────────────────────────── -->
    <div class="rdm-section-title">4. ANALYSE DES LIGNES D'INFLUENCE & VALEURS REMARQUABLES</div>
    
    <div class="rdm-grid-4">
        <div class="rdm-stat">
            <div class="rdm-stat-val">{max_bm.get('val', 0.0):.2f} kN.m</div>
            <div class="rdm-stat-lbl">Moment Max (L.I.)</div>
            <div style="font-size:9px; color:#64748b;">Travée {max_bm.get('i', 0)+1}, Sec. {max_bm.get('j', 0)}</div>
        </div>
        <div class="rdm-stat">
            <div class="rdm-stat-val">{max_sf.get('val', 0.0):.2f} kN</div>
            <div class="rdm-stat-lbl">Tranchant Max (L.I.)</div>
            <div style="font-size:9px; color:#64748b;">Travée {max_sf.get('i', 0)+1}, Sec. {max_sf.get('j', 0)}</div>
        </div>
        <div class="rdm-stat">
            <div class="rdm-stat-val">{max_def.get('val', 0.0):.3e} m</div>
            <div class="rdm-stat-lbl">Flèche Max (L.I.)</div>
            <div style="font-size:9px; color:#64748b;">Travée {max_def.get('i', 0)+1}, Sec. {max_def.get('j', 0)}</div>
        </div>
        <div class="rdm-stat">
            <div class="rdm-stat-val">{max_rot.get('val', 0.0):.3e} rad</div>
            <div class="rdm-stat-lbl">Rotation Max (L.I.)</div>
            <div style="font-size:9px; color:#64748b;">Travée {max_rot.get('i', 0)+1}, Sec. {max_rot.get('j', 0)}</div>
        </div>
    </div>

    <!-- Zone Graphique -->
    <div style="margin-top:10px;">
        {curves_html}
    </div>

    <!-- ── 5. Enveloppes ─────────────────────────────────────────────────── -->
    {envelopes_html}

    <!-- ── Cartouche de Fin RDM6 ─────────────────────────────────────────── -->
    <div class="rdm-footer">
        <div>Logiciel : <strong>RDM6 / Tsaraloha.LIPoutreContinue Core Engine</strong></div>
        <div>Méthode des Éléments Finis & Équation des Trois Moments (Clapeyron)</div>
        <div>Page 1/1</div>
    </div>

</div>

</body>
</html>
"""
    dest = Path(output_path)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(html, encoding="utf-8")
    return html
