"""
ex09_courbes.py — Voir toutes les courbes d'une poutre continue.

Poutre de 3 travées (10 + 14 + 10 m) sous un camion et une charge de voie.
Le script ouvre 6 fenêtres matplotlib :

  1. Synthèse            poutre chargée, enveloppes M / V / w, réactions, extrêmes
  2. Lignes d'influence  M en plusieurs sections (même graphique) + V
  3. Réactions           ligne d'influence de chaque réaction d'appui
  4. Enveloppes          M, V, w, θ — max(x) et min(x) le long de la poutre
  5. Position critique   charges placées pour le moment positif maximal
  6. Position critique   charges placées pour le moment sur appui (négatif)

    python ex09_courbes.py            # affiche les fenêtres
    python ex09_courbes.py --save     # enregistre ex09_*.png sans rien afficher

Lecture des courbes :
  * le moment est tracé côté fibre tendue : M > 0 (travée) vers le BAS ;
  * une ligne d'influence donne l'effet en UNE section d'une charge de 1 kN
    placée en x ; l'enveloppe donne, en CHAQUE section, l'effet le plus
    défavorable du convoi placé au pire endroit pour cette section.
"""
import sys

import Tsaraloha.LIPoutreContinue as tsa

if not hasattr(tsa, "Beam"):   # version 1.x installée (PyPI) au lieu de la 2.0 du dépôt
    raise SystemExit(f"Tsaraloha {tsa.__version__} est installée ; cet exemple requiert la 2.0. "
                     'Depuis la racine du dépôt :  python -m pip install ".[plot]"')

SAVE = "--save" in sys.argv
import matplotlib
if SAVE:
    matplotlib.use("Agg")
import matplotlib.pyplot as plt
from Tsaraloha.LIPoutreContinue import plot

# ── Modèle et calcul ────────────────────────────────────────────────────────
beam = tsa.Beam.continuous([10.0, 14.0, 10.0], E=210e9, I=[8e-4, 1.2e-3, 8e-4])
truck = tsa.PointLoadTrain(axles=[60, 120, 120], spacings=[1.5, 4.5], name="Camion")
lane = tsa.DistributedLoad(q=9.0, length=12.0, name="Voie")
res = beam.analyze(step=0.1, loads=[truck, lane])

figures = {}

# 1. Synthèse ---------------------------------------------------------------
figures["synthese"] = res.dashboard()

# 2. Lignes d'influence de M en plusieurs sections, puis de V ---------------
fig, (a1, a2) = plt.subplots(2, 1, figsize=(11, 7.5), constrained_layout=True)
sections = {"mi-travée 1 (x = 4 m)": 4.0, "appui B (x = 10 m)": 10.0,
            "mi-travée 2 (x = 17 m)": 17.0, "appui C (x = 24 m)": 24.0}
for (label, xs), color in zip(sections.items(), ["#c0392b", "#1f5fa8", "#d35400", "#16a085"]):
    lx, v, sx = res.influence_line("M", xs)
    a1.plot(lx, v, lw=1.7, color=color, label=label)
    a1.axvline(sx, color=color, lw=0.8, ls=":")
a1.axhline(0, color="black", lw=0.9)
for xs in res.support_positions:
    a1.plot([xs], [0], marker="^", color="0.3", markersize=9)
a1.set_title("Lignes d'influence du moment fléchissant", loc="left", fontweight="bold")
a1.set_xlabel("position de la charge unité [m]")
a1.set_ylabel("M [kN·m / kN]")
a1.grid(alpha=0.3)
a1.legend(fontsize=8, frameon=False, ncols=2)
plot.influence_line(res, "V", x=10.5, ax=a2)     # juste à droite de l'appui B : saut de la LI
figures["lignes_influence"] = fig

# 3. Lignes d'influence des réactions --------------------------------------
fig, ax = plt.subplots(figsize=(11, 4.5), constrained_layout=True)
for k, xs in enumerate(res.support_positions):
    lx, r = res.reaction_influence_line(k)
    ax.plot(lx, r, lw=1.7, label=f"R{k} (appui en x = {xs:g} m)")
ax.axhline(0, color="black", lw=0.9)
ax.set_title("Lignes d'influence des réactions d'appui (ΣR = 1 pour toute position)",
             loc="left", fontweight="bold")
ax.set_xlabel("position de la charge unité [m]")
ax.set_ylabel("R [kN / kN]")
ax.grid(alpha=0.3)
ax.set_ylim(top=1.3)                             # place pour la légende, au-dessus des courbes
ax.legend(fontsize=8, frameon=False, ncols=4, loc="upper center")
figures["reactions"] = fig

# 4. Enveloppes des 4 grandeurs ---------------------------------------------
fig, axes = plt.subplots(2, 2, figsize=(14, 8), constrained_layout=True)
for q, ax in zip(("M", "V", "w", "theta"), axes.flat):
    plot.envelope(res, q, ax=ax)
figures["enveloppes"] = fig

# 5–6. Positions critiques ----------------------------------------------------
figures["critique_travee"] = res.plot_critical("M", which="max")
figures["critique_appui"] = res.plot_critical("M", which="min")

# ── Valeurs lues sur les courbes ────────────────────────────────────────────
m, n = res.maximum_bending_moment, res.minimum_bending_moment
print(f"M max = {m.value:.1f} kN·m en x = {m.x:.2f} m  |  M min = {n.value:.1f} kN·m en x = {n.x:.2f} m")
print(f"Flèche extrême = {res.maximum_deflection.value * 1e3:.1f} mm en x = {res.maximum_deflection.x:.2f} m")

if SAVE:
    for name, f in figures.items():
        f.savefig(f"ex09_{name}.png", dpi=150)
    print("Figures enregistrées : " + ", ".join(f"ex09_{n}.png" for n in figures))
else:
    plt.show()          # fermez les fenêtres pour terminer le script
