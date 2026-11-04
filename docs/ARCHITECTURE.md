# Architecture — LIPoutreContinue 2.0

## Principe

**Un seul moteur numérique, en C++.** Python valide les entrées, orchestre le calcul, présente les résultats
(NumPy), trace les graphiques et produit les notes de calcul. Aucun algorithme de calcul n'est dupliqué. Les
seuls calculs écrits en Python sont des **contrôles** indépendants : les vérifications de la note de calcul et
l'oracle de Clapeyron des tests.

```
 Python (python/Tsaraloha/LIPoutreContinue/)                       C++ (LIPoutreContinue/)
 ┌──────────────────────────────┐                                  ┌─────────────────────────────────────────┐
 │ api.py   Beam, PointLoadTrain │  traduit le modèle               │ core/Isostatique   L.I. travée simple   │
 │          DistributedLoad      │ ───────────────────────────────▶ │ core/Hyperstatique méthode des foyers   │
 │          Results, Envelope    │                                  │ Output   orchestration + résultats      │
 │ __init__.py  façade 1.x +     │ ◀── bindings pybind11 ─────────  │          réactions, enveloppes, export   │
 │              validation       │     (bindings/bindings.cpp,      │ loading/MovingLoadSearch  recherche      │
 │ _validation.py messages       │      DictConverter.h)            │          exacte sur une ligne quelconque │
 │ plot.py      matplotlib       │                                  │ loading/Loading  enveloppes par section  │
 │ report.py    note de calcul + │                                  │ io/ProjectPaths  arborescence d'export   │
 │              contrôles        │                                  │ Utils.h  structures, toJson, delivery    │
 └──────────────────────────────┘                                  └─────────────────────────────────────────┘
```

## Modules C++ et responsabilités

| Module | Responsabilité | Dépend de |
|---|---|---|
| `core/Isostatique` | Lignes d'influence exactes (M, V, w, θ) d'une travée simplement appuyée ; maillage déterministe ; termes de charge ω′, ω″ | — |
| `core/Hyperstatique` | Poutre continue : rapports de foyers φ, φ′, moments sur appuis, tenseurs `[travée][section][charge]` | Isostatique |
| `loading/MovingLoadSearch` | `InfluenceLine` (interpolant, limites à gauche et à droite, primitive) ; `pointLoadExtreme` et `distributedLoadExtreme` (recherche exacte) | `struct load` |
| `loading/Loading` | Enveloppes par section et globales (E⁺ = Σmax, E⁻ = Σmin) pour un tenseur de lignes d'influence ; détail par charge | MovingLoadSearch |
| `Output` | Point d'entrée : calcul, réactions d'appui, extrema, enveloppes des 4 grandeurs et des réactions, export JSON explicite | Hyperstatique, Loading |
| `io/ProjectPaths` | Arborescence `01_Input/ … 05_Load_Positioning/` | `<filesystem>` |
| `io/UpdatePositions` | **Déprécié** (flux texte 1.x) | ProjectPaths |

Le calcul se fait entièrement en mémoire. Rien n'est écrit sur disque sans appel explicite à `export_*()`.

## Flux de données

1. `Beam.analyze()` transforme le modèle : l'encastrement devient une travée de longueur nulle et les charges deviennent des `Load` avec la convention `length = [d0, …]`.
2. `Output.compute()` :
   - calcule les tenseurs BM/SF/Def/Rot `[S][N_s][N]` et `SupportMoment[S+1][N]` ;
   - en déduit les réactions `SupportReaction[S+1][N]` ;
   - calcule les extrema.
3. `Output.computeLoadEnvelopes()` lance une instance de `Loading` par grandeur (en parallèle, sans copie du tenseur), puis les enveloppes de réactions. Facteur ×10³ pour w et θ (charges en kN, lignes d'influence par N).
4. `Results` expose ces données en NumPy, avec unités et abscisses physiques.
5. `report.py` produit la note de calcul et exécute ses contrôles.

## Choix et limites connus

- Les tenseurs exposés sont en O(N²) (≈ 39 Mo chacun pour N = 2205). Ils sont convertis en listes Python à chaque accès. Évolution prévue : stockage contigu, vues NumPy et évaluation exacte à la demande à partir des coefficients de ω′ et ω″. Voir [audit/03 N6b](audit/03_numerique.md) et [audit/benchmarks](audit/benchmarks.md).
- `std::async` est conservé tel quel : `compute()` ne coûte que quelques dizaines de millisecondes à N = 1000, aucun gain mesuré ne justifie OpenMP.
- La version est écrite à trois endroits (`pyproject.toml` et les deux `__init__.py`).
