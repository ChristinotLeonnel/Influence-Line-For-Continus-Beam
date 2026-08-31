"""
ex02_hyperstatique.py — Poutre continue HYPERSTATIQUE (3 travées)
=================================================================
Cas : pont-route 3 travées (12 m + 16 m + 12 m), section rectangulaire
      variée (travée centrale plus inerte).
Objectif : calculer les lignes d'influence complètes et afficher les
           moments sur appuis + enveloppe du moment fléchissant.
"""

import matplotlib.pyplot as plt
import Tsaraloha.LIPoutreContinue as lipc

# ── 1. Géométrie et matériaux ──────────────────────────────────────────────────
L_spans = [12.0, 16.0, 12.0]                  # Longueurs des travées [m]
E_val   = 35e9                                  # Béton HP 35 GPa
I_rive  = 0.35 * 0.70**3 / 12                  # Inertie travées de rive [m⁴]
I_cent  = 0.40 * 0.80**3 / 12                  # Inertie travée centrale [m⁴]
steps   = 0.5                                   # Pas 50 cm

E = [E_val] * len(L_spans)
I = [I_rive, I_cent, I_rive]

# ── 2. Calcul des lignes d'influence ──────────────────────────────────────────
poutre = lipc.Hyperstatique(E, I, L_spans, steps)

BM  = poutre.bending_moments()   # [travée][section][alpha] en kN·m
SM  = poutre.support_moment      # [appui][section]          en kN·m
X   = poutre.points_x_coordinates(poutre.span_node_positions)

print(f"Nombre de travées : {poutre.number_of_spans}")
print(f"Appuis intermédiaires cumulés : {poutre.L_spans}")
print(f"Coefficients de flexibilité a : {[f'{a:.2e}' for a in poutre.a_spans]}")

# ── 3. Tracé — moments sur appuis (section critique travée 0) ──────────────────
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
fig.suptitle("Lignes d'influence — Poutre hyperstatique 3 travées", fontsize=13)

# Moment sur appui intermédiaire (appui 1)
ax1.plot(X, SM[1], "b-", linewidth=2, label="M appui 1")
ax1.axhline(0, color="k", linewidth=0.8, linestyle="--")
ax1.fill_between(X, SM[1], alpha=0.15, color="blue")
ax1.set_title("L.I. Moment sur appui intermédiaire [kN·m]")
ax1.set_xlabel("Position charge α [m]")
ax1.legend()
ax1.grid(True, alpha=0.3)

# Enveloppe du moment en mi-travée 0 (section centrale)
mid_sec = len(BM[0]) // 2
ax2.plot(X, BM[0][mid_sec], "r-", linewidth=2, label=f"M mi-travée 0 (sec. {mid_sec})")
ax2.axhline(0, color="k", linewidth=0.8, linestyle="--")
ax2.fill_between(X, BM[0][mid_sec], alpha=0.15, color="red")
ax2.set_title("L.I. Moment en mi-travée 0 [kN·m]")
ax2.set_xlabel("Position charge α [m]")
ax2.legend()
ax2.grid(True, alpha=0.3)

plt.tight_layout()
plt.show()
