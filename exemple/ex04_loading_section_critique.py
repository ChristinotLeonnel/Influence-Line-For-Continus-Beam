"""
ex04_loading_section_critique.py — Classe Loading : analyse section par section
===============================================================================
Cas : pont 3 travées (10 m + 14 m + 10 m), convois lourds (ponctuel + réparti).
Objectif : utiliser la classe Loading directement pour :
  - interroger une section précise (span, section)
  - trouver la section critique d'une travée
  - combiner charges ponctuelles et réparties

Utile quand on veut un contrôle fin sans passer par Output.
"""

from Tsaraloha import LIPoutreContinue
import matplotlib.pyplot as plt
import Tsaraloha.LIPoutreContinue as lipc


# ── 1. Construction du modèle via Hyperstatique ────────────────────────────────
L_spans = [8.70, 0, 19.30, 17.8] 
E_val   = 30e9
I_val   = 0.35 * 0.65**3 / 12
steps   = 0.5

n = len(L_spans)
E = [E_val] * n
I = [I_val] * n

hyper = lipc.Hyperstatique(E, I, L_spans, steps)
BM    = hyper.bending_moments()
X     = hyper.points_x_coordinates(hyper.span_node_positions)

# ── 2. Définition des charges ──────────────────────────────────────────────────
convoi_1 = lipc.Load(intensity=[70.0, 130.0, 130.0], length=[0.0, 1.8, 1.4], name="Convoi-Lourd")
convoi_2 = lipc.Load(intensity=[50.0, 100.0],        length=[0.0, 2.5],       name="Convoi-Leger")
udl      = lipc.Load(intensity=[12.0,20.0],               length=[0.0, 6.0,2.0],       name="UDL-12kNm")

# ── 3. Instanciation de Loading ────────────────────────────────────────────────
ch = lipc.Loading(
    curves              = BM,
    position            = X,
    span_node_positions = hyper.span_node_positions,
    spans               = hyper.L_spans,
    point_loads         = [convoi_1, convoi_2],
    distrib_loads       = [udl],
)
#Toky Victore 
# ── 4a. Charge ponctuelle à une position précise ──────────────────────────────
span, section, alpha = 1, 10, 5       # Travée 1, section 10, position alpha 5
val_ponct = ch.one_point_load(intensity=100.0, span=span, section=section, alpha=alpha)
print(f"Charge unitaire 100 kN en alpha={alpha} -> M = {val_ponct:.3f} kN.m")

# ── 4b. Convoi optimal sur la section mi-travée 1 ────────────────────────────
mid_sec_1 = len(BM[1]) // 2
pos_opt = ch.plural_point_load(
    intensity = convoi_1.intensity,
    length    = convoi_1.length,
    span      = 1,
    section   = mid_sec_1,
)
print(f"\nConvoi lourd - position optimale sur travée 1, section {mid_sec_1} :")
print(f"  -> alpha_opt = {pos_opt['max_position']}  |  M = {pos_opt['value']:.2f} kN.m")

# ── 4c. Charge répartie optimale ──────────────────────────────────────────────
pos_udl = ch.plural_rectangular_load(
    intensity = udl.intensity,
    length    = udl.length,
    span      = 1,
    section   = mid_sec_1,
)
print(f"\nUDL 12 kN/m - position optimale :")
print(f"  -> alpha_opt = {pos_udl['max_position']}  |  M = {pos_udl['value']:.2f} kN.m")

# ── 4d. Combinaison ponctuelle + répartie ─────────────────────────────────────
combined = ch.combined_load_at(span=1, section=mid_sec_1)
print(f"\nCharge combinée (ponctuelle + répartie) sur travée 1, section {mid_sec_1} :")
print(f"  -> position = {combined['position']:.2f} m  |  M combiné = {combined['value']:.2f} kN.m")
for nom, detail in combined['addition'].items():
    val = detail.get('value', detail.get('Value', None))
    if val is not None:
        print(f"     * {nom:20s} contribution = {float(val):.2f} kN.m")
    else:
        print(f"     * {nom:20s} -> {detail}")

# ── 4e. Section critique de la travée 1 ───────────────────────────────────────
crit = ch.compute_critical_section(span=1)
print(f"\nSection critique travée 1 :")
print(f"  Ponctuel  -> section {crit['point']['section']}  M = {crit['point']['maximum_value']:.2f} kN.m")
print(f"  Réparti   -> section {crit['rect']['section']}   M = {crit['rect']['maximum_value']:.2f} kN.m")
print(f"  Combiné   -> section {crit['combined']['section']} M = {crit['combined']['maximum_value']:.2f} kN.m")

# ── 5. Visualisation ──────────────────────────────────────────────────────────
from Tsaraloha.LIPoutreContinue.plot import (
    plot_bm_envelopes,
    plot_support_moments,
    plot_load_on_influence_line,
    plot_load_summary
)

# 5a. Enveloppe moment mi-travée de chaque travée
fig1 = plot_bm_envelopes(X, BM, span_lengths=L_spans,
                          title="L.I. Moment fléchissant — mi-travée (3 travées)")

# 5b. Moments sur appuis
fig2 = plot_support_moments(hyper, X)

# 5c. Convoi lourd sur la L.I. de la section mi-travée 1 (position optimale)
fig3 = plot_load_on_influence_line(
    X,
    li_curve  = BM[1][mid_sec_1],
    loads     = [convoi_1],
    alpha_opt = int(pos_opt["max_position"]),
    span_lengths = L_spans,
    title     = f"Convoi lourd — L.I. Moment travée 1, section {mid_sec_1}",
    load_type = "point",
)

# 5d. UDL sur la L.I. de la section mi-travée 1
fig4 = plot_load_on_influence_line(
    X,
    li_curve  = BM[1][mid_sec_1],
    loads     = [udl],
    alpha_opt = int(pos_udl["max_position"]),
    span_lengths = L_spans,
    title     = f"UDL 12 kN/m — L.I. Moment travée 1, section {mid_sec_1}",
    load_type = "rect",
)

# 5e. Résumé complet pour la section critique de la travée 1
fig5 = plot_load_summary(
    X, BM, ch,
    span    = 1,
    section = mid_sec_1,
    title   = f"Résumé de chargement — Travée 1, section {mid_sec_1}",
)

plt.show()
