"""
ex05_visualisation_complete.py — Démonstration complète du module plot
=======================================================================
Cet exemple montre l'utilisation de TOUTES les fonctions du module
Tsaraloha.LIPoutreContinue.plot :

1. plot_isostatique_influence_lines  (Poutre isostatique : M, V, w, theta)
2. plot_hyperstatique_influence_lines (Poutre hyperstatique : sections d'une travée)
3. plot_support_moments              (Moments hyperstatiques sur appuis)
4. plot_bm_envelopes                 (Enveloppe des moments à mi-travée)
5. plot_load_on_influence_line       (Charge ponctuelle à la position optimale)
6. plot_load_on_influence_line       (Charge répartie UDL à la position optimale)
7. plot_load_summary                 (Résumé 3 panneaux : ponctuel, UDL, combiné)
8. plot_output_full                  (Vue synthétique complète via Output)
"""

import matplotlib.pyplot as plt
import Tsaraloha.LIPoutreContinue as lipc
from Tsaraloha.LIPoutreContinue.plot import (
    plot_isostatique_influence_lines,
    plot_hyperstatique_influence_lines,
    plot_support_moments,
    plot_bm_envelopes,
    plot_load_on_influence_line,
    plot_load_summary,
    plot_output_full,
)

def main():
    print("=== Demonstration complete du module Tsaraloha.LIPoutreContinue.plot ===")

    # ---------------------------------------------------------------------------
    # 1. Poutre Isostatique (1 travée L = 12 m)
    # ---------------------------------------------------------------------------
    print("\n[1/4] Generation du graphe Isostatique (4 panneaux)...")
    poutre_iso = lipc.Isostatique(E=30e9, I=0.005, L=12.0, steps=0.25)
    x_etude = 6.0  # section centrale
    fig1 = plot_isostatique_influence_lines(
        poutre_iso,
        x=x_etude,
        title="[Iso] Lignes d'influence - Poutre simple L = 12 m (x = 6.0 m)"
    )

    # ---------------------------------------------------------------------------
    # 2. Poutre Hyperstatique (4 travées : 8.7m + 7.45m + 19.3m + 17.8m)
    # ---------------------------------------------------------------------------
    print("[2/4] Calcul de la poutre Hyperstatique et traces de base...")
    L_spans = [8.70, 7.45, 19.30, 17.80]
    n = len(L_spans)
    E = [30e9] * n
    I = [0.35 * 0.65**3 / 12] * n
    steps = 0.5

    hyper = lipc.Hyperstatique(E, I, L_spans, steps)
    BM = hyper.bending_moments()
    X = hyper.points_x_coordinates(hyper.span_node_positions)

    # 2a. L.I. Moment pour les sections de la travée 2 (indice 2, L = 19.3m)
    fig2 = plot_hyperstatique_influence_lines(
        hyper, BM, X,
        span=2,
        title="[Hyper] L.I. Moment flechissant - Travee 2 (19.3 m)"
    )

    # 2b. L.I. Moments sur les appuis intermédiaires
    fig3 = plot_support_moments(
        hyper, X,
        title="[Hyper] L.I. Moments hyperstatiques sur appuis"
    )

    # 2c. Enveloppe du moment à mi-travée de chaque travée
    fig4 = plot_bm_envelopes(
        X, BM,
        span_lengths=L_spans,
        title="[Hyper] Enveloppe L.I. Moment - mi-travee de chaque travee"
    )

    # ---------------------------------------------------------------------------
    # 3. Application des Charges & Recherche des Positions Optimales
    # ---------------------------------------------------------------------------
    print("[3/4] Placement des convois et surcharges (Loading)...")
    convoi_1 = lipc.Load(intensity=[70.0, 130.0, 130.0], length=[0.0, 1.8, 1.4], name="Convoi-Lourd")
    convoi_2 = lipc.Load(intensity=[50.0, 100.0], length=[0.0, 2.5], name="Convoi-Leger")
    udl = lipc.Load(intensity=[12.0, 20.0], length=[0.0, 6.0, 2.0], name="UDL-Mixte")

    ch = lipc.Loading(
        curves=BM,
        position=X,
        span_node_positions=hyper.span_node_positions,
        spans=hyper.L_spans,
        point_loads=[convoi_1, convoi_2],
        distrib_loads=[udl],
    )

    span_target = 2
    sec_target = len(BM[span_target]) // 2

    # Recherche position optimale pour convoi_1
    pos_opt_pt = ch.plural_point_load(
        intensity=convoi_1.intensity,
        length=convoi_1.length,
        span=span_target,
        section=sec_target,
    )

    # Recherche position optimale pour UDL
    pos_opt_rect = ch.plural_rectangular_load(
        intensity=udl.intensity,
        length=udl.length,
        span=span_target,
        section=sec_target,
    )

    # 3a. Superposition Convoi lourd sur L.I.
    fig5 = plot_load_on_influence_line(
        X,
        li_curve=BM[span_target][sec_target],
        loads=[convoi_1],
        alpha_opt=int(pos_opt_pt["max_position"]),
        span_lengths=L_spans,
        ylabel="Moment [kN.m]",
        title=f"[Loading] Convoi Lourd a la position optimale (Travee {span_target}, section {sec_target})",
        load_type="point",
    )

    # 3b. Superposition UDL multi-segments sur L.I.
    fig6 = plot_load_on_influence_line(
        X,
        li_curve=BM[span_target][sec_target],
        loads=[udl],
        alpha_opt=int(pos_opt_rect["max_position"]),
        span_lengths=L_spans,
        ylabel="Moment [kN.m]",
        title=f"[Loading] UDL Mixte a la position optimale (Travee {span_target}, section {sec_target})",
        load_type="rect",
    )

    # 3c. Résumé complet 3 panneaux (Ponctuel + UDL + Combiné)
    fig7 = plot_load_summary(
        X, BM, ch,
        span=span_target,
        section=sec_target,
        title=f"[Loading] Resume de Chargement Complet - Travee {span_target}, section {sec_target}"
    )

    # ---------------------------------------------------------------------------
    # 4. Vue Synthétique Complète via la classe Output
    # ---------------------------------------------------------------------------
    print("[4/4] Generation de la vue Output synthétique (4 panneaux)...")
    out = lipc.Output(E, I, L_spans, steps)
    out.compute()
    out.set_loads(point_loads=[convoi_1, convoi_2], distrib_loads=[udl])

    fig8 = plot_output_full(
        out,
        title="[Output] Vue synthetique globale (Moments appuis, M, V, w mi-travees)"
    )

    print("\n-> Les 8 figures de visualisation ont ete generees avec succes !")
    print("-> Affichage des figures Matplotlib en cours...")
    plt.show()

if __name__ == "__main__":
    main()
