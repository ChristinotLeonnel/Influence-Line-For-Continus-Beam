"""
ex00_demarrage_rapide.py — Les 10 lignes pour commencer (API 2.0).

Poutre de 2 travées (10 m + 12 m), un camion de 3 essieux et une charge
de voie : enveloppes, valeurs maximales et note de calcul.
"""
import Tsaraloha.LIPoutreContinue as tsa

if not hasattr(tsa, "Beam"):   # version 1.x installée (PyPI) au lieu de la 2.0 du dépôt
    raise SystemExit(f"Tsaraloha {tsa.__version__} est installée ; cet exemple requiert la 2.0. "
                     'Depuis la racine du dépôt :  python -m pip install ".[plot]"')

beam = tsa.Beam()
beam.add_span(length=10.0, E=210e9, I=8e-4)          # E [Pa], I [m⁴]
beam.add_span(length=12.0, E=210e9, I=1.2e-3)
beam.add_support(0.0, "pin").add_support(10.0, "roller").add_support(22.0, "roller")

truck = tsa.PointLoadTrain(axles=[60, 120, 120], spacings=[1.5, 4.5], name="Camion")   # kN, m
lane = tsa.DistributedLoad(q=9.0, length=10.0, name="Voie")                            # kN/m, m

res = beam.analyze(step=0.25, loads=[truck, lane])

m = res.maximum_bending_moment
print(f"M max = {m.value:.1f} kN·m en x = {m.x:.2f} m")
print(f"M min = {res.minimum_bending_moment.value:.1f} kN·m (sur appui)")
print(f"Flèche extrême = {res.maximum_deflection.value * 1000:.2f} mm")
for r in res.reactions():
    print(f"Appui {r['support']} : R max = {r['max']:.1f} kN, R min = {r['min']:.1f} kN")

res.to_json("resultats_ex00.json")
res.report("note_ex00.html", project_title="Démarrage rapide")
try:
    import matplotlib.pyplot as plt
    res.plot("M")
    plt.show()
except ImportError:
    pass
