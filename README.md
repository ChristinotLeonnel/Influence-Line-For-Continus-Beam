# Tsaraloha — LIPoutreContinue

Bibliothèque de calcul des **poutres continues sous charges mobiles** : lignes d'influence (M, V, w, θ,
réactions), recherche exacte des positions les plus défavorables (convois et charges réparties),
enveloppes max/min et notes de calcul vérifiables.

- **Un seul moteur numérique, en C++20.** Python sert d'interface, de validation des entrées, de visualisation
  et de génération des notes de calcul.
- **Résultats vérifiés.** Les lignes d'influence coïncident à 10⁻¹⁵ près avec une résolution directe et
  indépendante des équations de Clapeyron. Les enveloppes sont testées contre des formes fermées et des
  recherches par force brute (voir [docs/VALIDATION.md](docs/VALIDATION.md)).
- Windows, Linux et macOS ; Python 3.9 à 3.14.

> **Version 2.0 :** les valeurs des enveloppes changent par rapport à 1.x (défauts corrigés). Voir le
> [CHANGELOG](CHANGELOG.md) et l'[audit](docs/audit/README.md).

## Installation

```bash
pip install Tsaraloha               # wheel précompilée (moteur C++ inclus)
pip install "Tsaraloha[plot]"       # + matplotlib (graphiques)
pip install "Tsaraloha[report]"     # + weasyprint (note de calcul en PDF)
```

Sur une plateforme sans wheel, pip compile depuis les sources : il faut alors un compilateur C++20 et
CMake ≥ 3.18.

## Démarrage rapide

```python
import Tsaraloha.LIPoutreContinue as tsa

beam = tsa.Beam()
beam.add_span(length=10.0, E=210e9, I=8e-4)          # E [Pa], I [m⁴], L [m]
beam.add_span(length=12.0, E=210e9, I=1.2e-3)
beam.add_support(0.0, "pin").add_support(10.0, "roller").add_support(22.0, "roller")

truck = tsa.PointLoadTrain(axles=[60, 120, 120], spacings=[1.5, 4.5], name="Camion")   # kN, m
lane  = tsa.DistributedLoad(q=9.0, length=10.0, name="Voie")                            # kN/m, m

res = beam.analyze(step=0.25, loads=[truck, lane])

m = res.maximum_bending_moment          # Extremum(value, x, span, unit, loads)
print(m.value, m.unit, m.x)             # 597.8 kN·m en x = 16.0 m
print(res.minimum_bending_moment)       # moment sur appui
env = res.envelope("M")                 # env.x, env.max, env.min (numpy)
res.reactions()                         # R max / min par appui [kN]
res.influence_line("M", x=16.0)         # (positions, valeurs par kN, section)
res.plot("M")                           # matplotlib
res.report("note.html", convergence=True)   # note de calcul (ou .pdf)
res.to_json("resultats.json")
```

Exemples complets : [`exemple/ex00_demarrage_rapide.py`](exemple/ex00_demarrage_rapide.py) et
[`exemple/ex07_pont_beton_convois.py`](exemple/ex07_pont_beton_convois.py).

## Modèle, unités et conventions

| | |
|---|---|
| Théorie | Euler-Bernoulli, élasticité linéaire, petits déplacements, EI constant par travée |
| Appuis | `pin` / `roller` (appui simple) à chaque extrémité de travée ; `fixed` (encastrement) aux deux extrémités de la poutre |
| Non modélisé | porte-à-faux, rotules, appuis élastiques, encastrement intérieur : ces cas lèvent une erreur explicite |
| Entrées | longueurs en m, E en Pa, I en m⁴, charges en kN et kN/m |
| Sorties | M en kN·m, V et R en kN, w en m, θ en rad |
| Signes | M > 0 : fibre inférieure tendue ; w > 0 vers le haut ; R > 0 vers le haut ; charges vers le bas |
| Charges mobiles | position la plus défavorable recherchée **exactement**, entrée et sortie partielles comprises ; charges indépendantes combinées par E⁺ = Σ max et E⁻ = Σ min |
| Précision | Lignes d'influence exactes aux nœuds. Enveloppes : erreur O(h²) pour les grandeurs hyperstatiques ; le paramètre `convergence=True` de la note de calcul la mesure |

Détail des équations et de la méthode : [docs/VALIDATION.md](docs/VALIDATION.md) et
[docs/NOTES_DE_CALCUL.md](docs/NOTES_DE_CALCUL.md).

## API bas niveau (compatibilité 1.x)

L'API historique reste disponible et produit les mêmes nombres que l'API de haut niveau :

```python
out = tsa.Output(E=[210e9] * 2, I=[8e-4, 1.2e-3], L=[10.0, 12.0], steps=0.25)
out.compute()                                   # lignes d'influence en mémoire
out.BM, out.SF, out.Def, out.Rot                # [travée][section][charge]
out.support_reaction                            # [appui][charge]
out.set_loads(point_loads=[tsa.Load(intensity=[60, 120], length=[0.0, 1.5], name="T")],
              distrib_loads=[tsa.Load(intensity=[9.0], length=[0.0, 10.0], name="q")])
out.compute_load_envelopes()
out.bending_moment_general_envelope["combined_load"]["max"]   # {value, span, section, section_x, load}
out.section_envelopes["bending_moment"]                       # courbes E⁺(x), E⁻(x)
out.export_all()                                              # JSON (si root fourni)
```

Format de `Load.length` : `[d0, s1, s2, …]` pour un convoi (essieu i à origine + d0 + s1 + … + si) et
`[d0, l1, …, ln]` pour une charge répartie (tronçon k de longueur lk). Les lignes d'influence de flèche et de
rotation (`Def`, `Rot`) sont données pour une charge unité de **1 N** ; les enveloppes sont en unités de sortie.
Référence complète : [DOCUMENTATION.md](DOCUMENTATION.md).

## Utilisation en C++

```cpp
#include <LIPoutreContinue/StructuralAnalysis.h>

std::vector<double> E{210e9, 210e9}, I{8e-4, 1.2e-3}, L{10.0, 12.0};
double h = 0.25;
Output out(E, I, L, h);            // E, I, L doivent survivre à `out`
out.compute();                     // out.BM, out.SF, out.Def, out.Rot, out.SupportReaction, out.X
out.setLoads({ load{{60, 120, 120}, {0.0, 1.5, 4.5}, "Camion"} },
             { load{{9.0}, {0.0, 10.0}, "Voie"} });
out.computeLoadEnvelopes();        // out.BendingMomentGeneralLoadEnvelope.combinedLoad.max_value, …
```

Exemples : [`LIPoutreContinue/examples/`](LIPoutreContinue/examples/) (option CMake `TSARALOHA_BUILD_EXAMPLES=ON`).

## Compiler et tester

```bash
cmake -S . -B build -DTSARALOHA_BUILD_TESTS=ON -DTSARALOHA_BUILD_EXAMPLES=ON
cmake --build build --config Release
ctest --test-dir build -C Release                         # tests natifs C++
PYTHONPATH=build/python python -m pytest                  # PowerShell : $env:PYTHONPATH="build/python"
```

| Option CMake | Défaut | Rôle |
|---|---|---|
| `TSARALOHA_BUILD_PYTHON` | `ON` | Extension Python (pybind11, téléchargé automatiquement si absent) |
| `TSARALOHA_BUILD_EXAMPLES` | `OFF` | Exemples C++ |
| `TSARALOHA_BUILD_TESTS` | `OFF` | Tests natifs (`ctest`) |

## Documentation

| Document | Contenu |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Modules, responsabilités, flux de données |
| [docs/VALIDATION.md](docs/VALIDATION.md) | Équations, conventions, campagne de validation et précision |
| [docs/NOTES_DE_CALCUL.md](docs/NOTES_DE_CALCUL.md) | Génération et contenu des notes de calcul |
| [DOCUMENTATION.md](DOCUMENTATION.md) | Référence de l'API bas niveau |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Règles de contribution |
| [PUBLISHING.md](PUBLISHING.md) | Publication sur PyPI |
| [docs/audit/](docs/audit/README.md) | Audit technique et scientifique 2.0, benchmarks |
