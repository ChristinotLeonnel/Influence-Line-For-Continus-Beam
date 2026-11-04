# 05 — Résultats, notes de calcul, documentation, build et CI

## 1. Système de résultats

| Problème | Détail |
|---|---|
| Extrema = indices | `Position3D{i,j,k,val}` : travée, indice de section, indice de charge. Pas d'abscisse, pas d'unité, pas de min séparé du max |
| Champs réutilisés | `support_moment.json` exporte `Position2D.j` sous le nom `alpha` ; `load_delivery.position` vaut 0,0 pour les charges ponctuelles et réparties (non renseigné), mais est rempli pour la combinée |
| « Position » ambiguë | Ponctuelle : `POSITION[max_position]` (origine du convoi). Combinée : `first_wall + Length[0]` (1ᵉʳ essieu). Les deux grandeurs portent le même nom |
| Clés textuelles | Le détail des contributions est indexé par `"<int(intensité)> <rang>"` ; l'intensité est tronquée à l'entier |
| Pas de diagrammes de sollicitation | On ne peut pas obtenir M(x), V(x), w(x) pour un chargement donné |
| Pas de réactions | Voir 03 §3 |
| Format lourd | `02_Influence_Lines/*.json` : tenseurs N² en JSON indenté (`dump(4)`). Pour N = 1000, plusieurs dizaines de Mo par fichier |

## 2. Note de calcul — [report.py](../../python/Tsaraloha/LIPoutreContinue/report.py)

| Problème | Détail |
|---|---|
| **Attribution à un logiciel tiers** | « conforme au standard RDM6 / RDM Le Mans » (l. 210) ; « Logiciel : RDM6 / Tsaraloha… » (l. 841) ; références par défaut `RDM6-TSARALOHA-01`. RDM6 est un logiciel de l'IUT du Mans. La note ne doit pas laisser croire qu'il a produit les résultats |
| Unités erronées | Les extrema de *lignes d'influence* (par charge unité) sont affichés en kN·m et kN (l. 810, 815). Les flèches héritent de N1 |
| Résultats affichés faux | Les tableaux d'enveloppes reprennent directement N2 à N6 |
| Vérifications absentes | Aucun contrôle d'équilibre, de convergence ou de cohérence n'est exécuté, ni donc rapporté. C'est correct (rien n'est inventé), mais incomplet par rapport à l'objectif |
| Traçabilité | La méthode (foyers / Clapeyron), le pas, le nombre de nœuds et la version du code ne figurent pas tous dans la note |
| Moteurs PDF multiples | weasyprint, puis Chromium headless (recherché sur le disque), puis pdfkit. Le comportement dépend de la machine |
| Maintenabilité | HTML et CSS inline dans des f-strings (≈ 650 lignes) ; pas de gabarit, pas de tests |

## 3. Documentation

| Document | Problème |
|---|---|
| README / DOCUMENTATION | « La dernière valeur de `length` n'est pas utilisée » : **faux** (voir 03-N4) |
| README / DOCUMENTATION | Les unités annoncées (kN avec Pa) conduisent à N1 |
| DOCUMENTATION | Annonce « Enveloppes critiques (à la section du maximum global) » ; en réalité la recherche se fait sur la **travée** du maximum |
| `__init__.pyi` | Déclare des classes `Position3D` avec attributs ; à l'exécution, ce sont des `dict` (commentaire « IMPORTANT » dans `__init__.py`) |
| PUBLISHING.md | Intitulé « Windows uniquement » alors que README et pyproject annoncent Linux et macOS |
| Absents | Guide de contribution, CHANGELOG, guide de validation scientifique, description des conventions de signe |

## 4. Build, distribution, CI

### 4.1 Import cassé après `cmake --build` sous Visual Studio
`LIBRARY_OUTPUT_DIRECTORY` sans suffixe de configuration
([CMakeLists.txt:179-182](../../CMakeLists.txt#L179-L182)) : les générateurs
multi-configurations ajoutent `Release/`. Les `.py` sont copiés dans
`build/python/Tsaraloha/LIPoutreContinue/`, le `.pyd` dans
`…/LIPoutreContinue/Release/`. Avec `PYTHONPATH=build/python`, on obtient
`_BACKEND == "python"`, **silencieusement**. (Vérifié : après copie manuelle
du `.pyd`, `_BACKEND == "cpp"`.)
**Correction :** `LIBRARY_OUTPUT_DIRECTORY_<CONFIG>` via une expression
génératrice `$<1:…>`, ou une boucle sur `CMAKE_CONFIGURATION_TYPES`.

### 4.2 Plateformes
- `cibuildwheel` : `build = "cp3{9..13}-win_amd64"` seulement. Linux et macOS reçoivent la wheel pur-Python (backend lent, encastrements impossibles), malgré les classifieurs « POSIX :: Linux » et « MacOS ».
- Python 3.14 et 3.15 ne sont pas construits, alors que l'environnement de l'utilisateur est en 3.15.
- `cibuildwheel@v2.21` est figé ; les versions récentes (3.x) savent construire Linux (manylinux, musllinux), macOS (x86_64, arm64), et selon les versions iOS et Android. Ce dernier point est à vérifier au moment de l'implémentation : il conditionne la décision 1.
- Deux systèmes de build concurrents : `pyproject.toml` (scikit-build-core) et `setup_pure.py` (setuptools), avec des métadonnées dupliquées (version, dépendances, classifieurs).
- Version écrite en dur à 3 endroits (`pyproject.toml`, `setup_pure.py`, `__init__.py` ×2).

### 4.3 Qualité C++
- `/W4` et `-Wall -Wextra` sont déclarés `PUBLIC` : ils se propagent à tout consommateur de la librairie, ce qu'il vaut mieux éviter (`PRIVATE`).
- Pas de cible `install`/`export` CMake pour le cœur C++ : une application tierce ne peut pas faire `find_package(LIPoutreContinue)`.
- Pas de sanitizers ni d'analyse statique en CI. Pas de job Linux en CI.

### 4.4 CI
- `publish_pypi` a `continue-on-error: true` : un échec de publication passe en vert.
- Le seul test en CI est un test fumée d'import.
