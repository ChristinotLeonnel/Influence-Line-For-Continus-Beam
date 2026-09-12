"""
ex05_loading_exact_position.py — Imposer une position de charge exacte
======================================================================
Cet exemple montre comment positionner explicitement un convoi ou une 
charge à une position arbitraire (exacte) sur la ligne d'influence, 
plutôt que de chercher la position critique.
"""

import matplotlib.pyplot as plt
import Tsaraloha.LIPoutreContinue as lipc
import numpy as np
from Tsaraloha.LIPoutreContinue.plot import plot_load_on_influence_line

# ── 1. Construction du modèle via Hyperstatique ────────────────────────────────
L_spans = [8.70, 19.30, 17.8] 
E_val   = 30e9
I_val   = 0.35 * 0.65**3 / 12
steps   = 0.5

n = len(L_spans)
E = [E_val] * n
I = [I_val] * n

hyper = lipc.Hyperstatique(E, I, L_spans, steps)
BM    = hyper.deflection() 
X     = hyper.points_x_coordinates(hyper.span_node_positions)

# ── 2. Définition des charges ──────────────────────────────────────────────────
convoi = lipc.Load(intensity=[70.0, 130.0, 130.0], length=[0.0, 1.8, 1.4], name="Convoi-Lourd")
udl    = lipc.Load(intensity=[12.0, 20.0], length=[0.0, 6.0, 2.0], name="UDL-12kNm")

# On veut étudier la section au milieu de la travée 1 (index 1)
span_etude = 1
mid_sec = len(BM[span_etude]) // 2
courbe_li = BM[span_etude][mid_sec]

# ── 3. Trouver l'index (alpha) pour une position exacte ─────────────────────────
# Disons qu'on veut placer le début de notre charge exactement à 5 mètres depuis 
# le début de la travée 1 (sans se soucier si c'est la position la plus critique).
# Comme X contient les coordonnées globales depuis le début du pont, 
# la position globale est : longueur(Travée 0) + 5.0m
position_metres_locale = 5.0
position_globale = L_spans[0] + position_metres_locale

# On cherche l'index le plus proche dans les coordonnées globales X
alpha_exact = (np.abs(np.array(X) - position_globale)).argmin()

print(f"Position souhaitée (locale travée 1) : {position_metres_locale} m")
print(f"Position globale : {position_globale} m")
print(f"Index (alpha) correspondant trouvé : {alpha_exact} (valeur réelle: {X[alpha_exact]:.2f} m)")

# ── 4. Visualisation à la position imposée ─────────────────────────────────────

# 4a. On dessine le convoi lourd à la position exacte
fig1 = plot_load_on_influence_line(
    X,
    li_curve  = courbe_li,\
    loads     = [convoi],
    alpha_opt = int(alpha_exact),    # <--- ICI ON IMPOSE LA POSITION !
    span_lengths = L_spans,
    title     = f"Convoi lourd imposé à x = {X[alpha_exact]:.2f} m (Travée {span_etude}, section {mid_sec})",
    load_type = "point",
)

# 4b. On dessine la charge répartie (UDL) à une autre position exacte (ex: 2 mètres sur travée 1)
position_globale_udl = L_spans[0] + 2.0
alpha_exact_udl = (np.abs(np.array(X) - position_globale_udl)).argmin()

fig2 = plot_load_on_influence_line(
    X,
    li_curve  = courbe_li,
    loads     = [udl],
    alpha_opt = int(alpha_exact_udl), # <--- ICI ON IMPOSE LA POSITION POUR L'UDL !
    span_lengths = L_spans,
    title     = f"UDL imposée à x = {X[alpha_exact_udl]:.2f} m (Travée {span_etude}, section {mid_sec})",
    load_type = "rect",
)

plt.show()
