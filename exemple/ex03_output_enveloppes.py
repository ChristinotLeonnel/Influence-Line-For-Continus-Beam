"""
ex03_output_enveloppes.py — Classe Output : calcul complet + enveloppes de charges
===================================================================================
Cas : pont 2 travées (15 m + 20 m), soumis à des convois routiers (charges
      ponctuelles) et une surcharge répartie (trottoir).
Objectif : démontrer le workflow complet via la classe Output (point d'entrée
           principal), du calcul en RAM jusqu'à l'export des enveloppes.

Workflow :
    1. Output(E, I, L, steps)       → géométrie + discrétisation
    2. .compute()                   → lignes d'influence en RAM
    3. .set_loads(points, distribs) → injection des charges mobiles
    4. .compute_load_envelopes()    → enveloppes max en RAM
    5. .export_all()                → écriture JSON/TXT sur disque
"""

import matplotlib.pyplot as plt
import Tsaraloha.LIPoutreContinue as lipc

# ── 1. Définition de la structure ─────────────────────────────────────────────
E_val   = 32e9                           # Béton C32/40 [Pa]
I_val   = 0.40 * 0.75**3 / 12           # Section rectangulaire 40×75 cm [m⁴]
L_spans = [15.0, 20.0]                   # Deux travées [m]
steps   = 0.5                            # Pas de 50 cm

n = len(L_spans)
E = [E_val] * n
I = [I_val] * n

out = lipc.Output(E, I, L_spans, steps, root="./resultats_ex03")

# ── 2. Calcul des lignes d'influence ──────────────────────────────────────────
out.compute()

# Extrema globaux des L.I. (sans charge)
bm_max = out.bending_moment_max_positions
sf_max = out.shear_force_max_positions
print("=== Extrema des lignes d'influence ===")
print(f"  Moment max    : {bm_max['val']:+.4f} kN·m  (travée {bm_max['i']}, section {bm_max['j']})")
print(f"  Tranchant max : {sf_max['val']:+.4f} kN    (travée {sf_max['i']}, section {sf_max['j']})")

# ── 3. Définition des charges mobiles ─────────────────────────────────────────
# Convoi 1 : camion 2 essieux (60 kN + 120 kN, entraxe 2.5 m)
camion_A = lipc.Load(intensity=[60.0, 120.0], length=[0.0, 2.5], name="Camion-A")

# Convoi 2 : semi-remorque 3 essieux (60 / 140 / 140 kN)
semi_B   = lipc.Load(intensity=[60.0, 140.0, 140.0], length=[0.0, 2.0, 1.4], name="Semi-B")

# Surcharge répartie : trottoir 10 kN/m sur 8 m
trottoir = lipc.Load(intensity=[10.0], length=[0.0, 8.0], name="Trottoir-10kNm")

# ── 4. Calcul des enveloppes ──────────────────────────────────────────────────
out.set_loads(point_loads=[camion_A, semi_B], distrib_loads=[trottoir])
out.compute_load_envelopes()

env_bm = out.bending_moment_general_envelope
env_sf = out.shear_force_general_envelope

print("\n=== Enveloppe du Moment Fléchissant ===")
for load_type in ("point_load", "rectangular_load", "combined_load"):
    data = env_bm[load_type]
    print(f"  [{load_type:18s}]  max = {data['maximum_value']:+.2f} kN·m"
          f"  @ travée {data['span']}, section {data['section']}")

print("\n=== Enveloppe de l'Effort Tranchant ===")
for load_type in ("point_load", "rectangular_load", "combined_load"):
    data = env_sf[load_type]
    print(f"  [{load_type:18s}]  max = {data['maximum_value']:+.2f} kN"
          f"  @ travée {data['span']}, section {data['section']}")

# ── 5. Export sur disque (JSON + TXT) ─────────────────────────────────────────
out.export_all()
print(f"\n✓ Exports écrits dans : {out.paths['root']}")

# ── 6. Visualisation ──────────────────────────────────────────────────────────
X = out.X

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8), sharex=True)
fig.suptitle("Enveloppes — Output 2 travées (15 m + 20 m)", fontsize=13)

# Moments sur appuis (ligne d'influence)
SM = out.support_moment
for k, sm in enumerate(SM):
    ax1.plot(X, sm, label=f"M appui {k}")
ax1.axhline(0, color="k", linewidth=0.8, linestyle="--")
ax1.set_ylabel("Moment hyperstatique [kN·m]")
ax1.legend(fontsize=8)
ax1.grid(True, alpha=0.3)

# Moment en mi-travée de la travée 0
mid = len(out.BM[0]) // 2
ax2.plot(X, out.BM[0][mid], "b-", linewidth=2, label="M mi-travée 0")
ax2.plot(X, out.BM[1][len(out.BM[1]) // 2], "r-", linewidth=2, label="M mi-travée 1")
ax2.axhline(0, color="k", linewidth=0.8, linestyle="--")
ax2.set_ylabel("Moment fléchissant [kN·m]")
ax2.set_xlabel("Abscisse globale x [m]")
ax2.legend(fontsize=8)
ax2.grid(True, alpha=0.3)

plt.tight_layout()
plt.show()
