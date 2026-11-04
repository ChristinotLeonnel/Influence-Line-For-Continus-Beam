"""
ex07_pont_beton_convois.py — Poutre en béton bi-encastrée de 2 travées
(154 m + 15 m) sous trois convois et une charge répartie par tronçons.

Reprend le cas de l'ancien brouillon exemple2.py avec l'API 2.0.

ATTENTION — données volontairement conservées : une section de 30 x 60 cm
sur 154 m de portée donne une flèche de plusieurs dizaines de mètres. Le
calcul linéaire est exact pour ces données, mais l'hypothèse des petits
déplacements est violée : la note de calcul le signale (« Domaine de
validité » NON CONFORME). C'est le comportement attendu d'un contrôle
physique.
"""
import Tsaraloha.LIPoutreContinue as tsa

if not hasattr(tsa, "Beam"):   # version 1.x installée (PyPI) au lieu de la 2.0 du dépôt
    raise SystemExit(f"Tsaraloha {tsa.__version__} est installée ; cet exemple requiert la 2.0. "
                     'Depuis la racine du dépôt :  python -m pip install ".[plot]"')

E = 30e9                       # béton, 30 GPa
I = 0.3 * 0.6**3 / 12          # section 30 x 60 cm : b h³ / 12  [m⁴]

beam = tsa.Beam()
beam.add_span(length=154.0, E=E, I=I).add_span(length=15.0, E=E, I=I)
beam.add_support(0.0, "fixed").add_support(154.0, "roller").add_support(169.0, "fixed")

loads = [
    tsa.PointLoadTrain(axles=[6, 12, 12], spacings=[2.5, 7.0], name="Convoi 1"),
    tsa.PointLoadTrain(axles=[6, 15, 12], spacings=[2.5, 8.0], name="Convoi 2"),
    tsa.PointLoadTrain(axles=[6, 14, 12], spacings=[2.5, 5.0], name="Convoi 3"),
    # 6 kN/m sur 2,5 m, puis 12 kN/m sur 5 m, puis 12 kN/m sur 4 m
    tsa.DistributedLoad(q=[6, 12, 12], length=[2.5, 5.0, 4.0], name="Convoi 4 (réparti)"),
]

res = beam.analyze(step=1.0, loads=loads)

for name, ex in [("M max", res.maximum_bending_moment), ("M min", res.minimum_bending_moment),
                 ("V", res.maximum_shear_force), ("w", res.maximum_deflection)]:
    print(f"{name:6s} = {ex.value: .4g} {ex.unit}  en x = {ex.x:.2f} m")
    for load, d in ex.loads.items():
        print(f"         {load:20s} {d['value']: .4g}  (origine {d['position']:.2f} m)")

for r in res.reactions():
    print(f"Appui {r['support']} (x = {r['x']:.1f} m) : R ∈ [{r['min']:.2f} ; {r['max']:.2f}] kN")

from Tsaraloha.LIPoutreContinue.report import run_checks
for c in run_checks(res):
    print(f"[{'OK ' if c['passed'] else 'NON CONFORME'}] {c['name']} : {c['measure']:.3g}")

res.report("note_ex07.html", project_title="Poutre bi-encastrée 154 + 15 m", convergence=True)
print("Note de calcul : note_ex07.html")
