"""
ex01_isostatique.py — Ligne d'influence : poutre ISOSTATIQUE simple
====================================================================
Cas : poutre bi-articulée de 10 m, section rectangulaire (béton 30 GPa).
Objectif : calculer et tracer les lignes d'influence de M, V, w, θ
           pour la section centrale (x = L/2).
"""

import matplotlib.pyplot as plt
import Tsaraloha.LIPoutreContinue as lipc

# ── 1. Propriétés physiques ────────────────────────────────────────────────────
E     = 30e9              # Module de Young béton [Pa]
I     = 0.3 * 0.6**3 / 12  # Inertie rectangulaire b×h³/12 [m⁴]
L     = 10.0              # Longueur de la travée [m]
steps = 0.25              # Pas de discrétisation [m]

# ── 2. Instanciation et calcul ─────────────────────────────────────────────────
poutre = lipc.Isostatique(E, I, L, steps)

alpha = poutre.node_positions          # Positions de la charge unité [m]
x_mid = L / 2                          # Section d'étude : mi-travée

LI_M    = poutre.eq_bending_moment(x_mid)          # L.I. du moment [kN·m/kN]
LI_V    = poutre.eq_shear_force(x_mid, False)      # L.I. de V (valeurs)
alpha_V = poutre.eq_shear_force(x_mid, True)       # Abscisse propre à V (discontinuités)
LI_w    = poutre.eq_deflection(x_mid)              # L.I. de la flèche [m/kN]
LI_th   = poutre.eq_rotation(x_mid)               # L.I. de la rotation [rad/kN]

# ── 3. Tracé ───────────────────────────────────────────────────────────────────
fig, axes = plt.subplots(2, 2, figsize=(12, 7))
fig.suptitle(f"Lignes d'influence — Poutre isostatique L = {L} m  (x = L/2)", fontsize=13)

# Tranchant tracé avec son abscisse propre (alpha_V), les autres avec alpha
datasets = [
    (axes[0, 0], alpha,   LI_M,  "Moment fléchissant M [kN·m/kN]", "tab:blue"),
    (axes[0, 1], alpha_V, LI_V,  "Effort tranchant V [kN/kN]",      "tab:red"),
    (axes[1, 0], alpha,   LI_w,  "Flèche w [m/kN]",                 "tab:green"),
    (axes[1, 1], alpha,   LI_th, "Rotation θ [rad/kN]",             "tab:orange"),
]

for ax, absc, data, title, color in datasets:
    ax.plot(absc, data, color=color, linewidth=2)
    ax.axhline(0, color="k", linewidth=0.8, linestyle="--")
    ax.fill_between(absc, data, alpha=0.15, color=color)
    ax.set_title(title, fontsize=10)
    ax.set_xlabel("Position charge α [m]")
    ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.show()
