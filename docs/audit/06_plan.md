# 06 — Architecture cible et plan de modernisation

## 1. Principes

1. **Le solveur hyperstatique est exact : on le garde.** On le restructure (mémoire, API), sans réécrire les équations.
2. **Les corrections numériques précèdent les optimisations.** Chaque correction est verrouillée par un test qui échoue avant et passe après.
3. **Un seul moteur numérique (C++).** Python sert d'interface, de validation des entrées, de visualisation et de rapports. L'oracle Clapeyron de `reproduce/` devient l'outil de *test*, pas un moteur.
4. **Compatibilité :** l'API actuelle (`Output`, `Load`, `Hyperstatique`, …) reste disponible pendant au moins une version mineure, sous forme de façade au-dessus du nouveau cœur, avec des `DeprecationWarning`.

## 2. Architecture cible (adaptée à l'existant)

```
LIPoutreContinue/
  include/LIPoutreContinue/
    core/       Units.h, Errors.h                        ← types, unités SI, exceptions
    model/      Beam.h  (travées, appuis dont encastrement explicite, E, I)
                Mesh.h  (maillage déterministe, N8)
    solver/     ContinuousBeamSolver.h                    ← foyers / Clapeyron (code actuel de Hyperstatique)
                → stocke SupportMoment [S+1][N] (O(S·N)) + évaluation de toute L.I. en O(N)
    influence/  InfluenceLine.h  (M, V⁻/V⁺, w, θ, R ; tampon contigu)
    loads/      PointLoadTrain.h, DistributedLoad.h, LoadGroup.h  (convention unique, N4)
    envelopes/  EnvelopeSolver.h (max⁺/min⁻, entrée complète, interpolation, intégration exacte — N2,N3,N5,N6,N7,N9)
    results/    Results.h        (valeurs + abscisses + unités + charge responsable — N10)
    io/         Json.h (optionnel, seul consommateur de nlohmann), ProjectPaths.h
    compat/     Output.h, Loading.h, Hyperstatique.h, Isostatique.h  ← façades de l'ancienne API
  bindings/     numpy sans copie (buffer protocol)
python/Tsaraloha/LIPoutreContinue/
  __init__.py   API haut niveau : Beam, Analysis, Results (+ réexports compat)
  _validation.py
  plot.py       consomme Results
  report/       gabarit HTML (Jinja2 optionnel ou string.Template stdlib) + PDF via un seul moteur
tests/
  cpp/          (doctest ou Catch2 via FetchContent, ou ctest + asserts : à décider)
  python/       pytest : oracle Clapeyron, cas analytiques, régression, convergence, parité API
benchmarks/     modèles S/M/L + script de mesure
```

Le moteur (`model`, `solver`, `influence`, `loads`, `envelopes`, `results`)
n'inclut **ni** `nlohmann/json` **ni** `<filesystem>` **ni** `<iostream>`.

### API Python cible (indicative, à valider)

```python
import Tsaraloha.LIPoutreContinue as tsa
beam = tsa.Beam()
beam.add_span(length=10.0, E=210e9, I=8e-4)
beam.add_span(length=12.0, E=210e9, I=1.2e-3)
beam.add_support(0.0, "pin"); beam.add_support(10.0, "roller"); beam.add_support(22.0, "fixed")
truck = tsa.PointLoadTrain(axles_kN=[60, 120, 120], spacing_m=[1.5, 4.5], reversible=True)
lane  = tsa.DistributedLoad(q_kN_m=9.0, length_m=None)      # None = zones défavorables uniquement
res = beam.analyze(step=0.25, moving_loads=[truck, lane])
res.envelope("M").max   # tableau numpy sur res.x, en N·m (SI) ; res.envelope("M").max_at → (x, valeur, position charge)
res.influence_line("M", x=5.0)       # n'importe quelle abscisse, pas seulement aux nœuds
res.plot("M"); res.report("note.html")
```

## 3. Plan priorisé

Chaque étape correspond à un ou plusieurs commits relus, avec compilation,
tests, comparaison des résultats et des performances. **Aucun commit ni push
sans votre autorisation explicite.**

| Phase | Contenu | Risque API | Dépend de |
|---|---|---|---|
| **P0 — Socle de tests** | `tests/` pytest : oracle Clapeyron (de `reproduce/`), cas analytiques, les 26 contrôles actuels (les 7 en échec marqués `xfail(strict=True)` avec le n° de défaut) ; test C++ minimal ; job CI Linux + Windows | aucun | — |
| **P1 — Build** | Correction de l'emplacement du `.pyd` (05 §4.1) ; avertissement explicite si repli sur le backend Python ; flags `PRIVATE` | aucun | — |
| **P2 — Corrections numériques** | N8 maillage → N6 intégration → N7 tranchant → N4 convention `length` → N3 entrée complète → N2 enveloppes max⁺/min⁻ → N5 combinaison → N9 interpolation → N1 unités | **valeurs modifiées** (justifiées dans 03) | P0, décisions 2 et 3 |
| **P3 — Mémoire et performance** | Stockage SupportMoment O(S·N) + L.I. à la demande ; tampons contigus ; numpy sans copie ; algorithme d'enveloppes O(N²·n) ; décision sur `std::async` d'après benchmark | aucun si façade | P2 |
| **P4 — Résultats** | `Results` : abscisses physiques, max et min, positions critiques, charge responsable, réactions, diagrammes pour un chargement fixe | ajout | P2 |
| **P5 — Moteur unique** | Exécution de la décision 1 sur le backend Python ; extension des wheels (Linux, macOS, Python 3.14 et 3.15) | selon décision | P0, P2 |
| **P6 — API haut niveau** | `Beam`, `PointLoadTrain`, `DistributedLoad`, `analyze()` ; ancienne API conservée en façade avec dépréciation | ajout | P4 |
| **P7 — Note de calcul** | Gabarit, 20 sections demandées, uniquement des valeurs calculées, vérifications réellement exécutées (équilibre, Maxwell-Betti, convergence), traçabilité (version, pas, méthode) ; retrait des mentions RDM6 | modifié | P4 |
| **P8 — Nettoyage** | Suppressions listées dans 02 (avec CHANGELOG), dépréciation d'`UpdatePositions` et des parseurs texte | dépréciation | P6 |
| **P9 — Documentation** | README, DOCUMENTATION, architecture, guide de validation scientifique, CONTRIBUTING, CHANGELOG, exemples C++ et Python mis à jour | — | toutes |

## 4. Changements incompatibles prévisibles (à consigner dans le CHANGELOG)

- Les flèches et rotations des enveloppes sont multipliées par 1000 si l'on adopte un SI cohérent (N1).
- Les valeurs d'enveloppes changent (N2 à N7, N9). Elles sont généralement **plus défavorables**, donc plus sûres, parce que les positions manquées sont désormais explorées.
- Le nombre de nœuds change pour les pas qui ne divisent pas exactement la portée (N8), ce qui modifie les dimensions des tenseurs.
- Si la décision 1 aboutit à une suppression, les plateformes sans wheel C++ ne peuvent plus installer la librairie sans compilateur.
