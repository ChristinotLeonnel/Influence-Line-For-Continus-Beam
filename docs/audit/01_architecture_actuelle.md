# 01 — Architecture actuelle

## 1. Arborescence

```
CMakeLists.txt, pyproject.toml (scikit-build-core), setup_pure.py (wheel py3-none-any)
.github/workflows/wheels.yml           CI : wheels Windows x64 + wheel pure + sdist + publication
include/nlohmann/                      json.hpp embarqué (dépendance du *cœur* C++)
LIPoutreContinue/
  include/LIPoutreContinue/
    StructuralAnalysis.h               en-tête parapluie
    Utils.h                            structs + recherche de max + intégration + export JSON + parsing
    core/Isostatique.h, Hyperstatique.h
    loading/Input.h (Configuration), Loading.h
    io/ProjectPaths.h, UpdatePositions.h
    Output.h
  src/ (…cpp correspondants)
  bindings/bindings.cpp (844 l.), DictConverter.h
  examples/ ex01..ex03 (.cpp)
python/Tsaraloha/LIPoutreContinue/
  __init__.py      façade + validation (sous-classes des types C++)
  __init__.pyi     stubs
  _core_python.py  2e moteur complet en pur Python (repli)
  _validation.py   contrôles d'entrée
  plot.py          matplotlib (1150 l.)
  report.py        note de calcul HTML/PDF (904 l.)
exemple/           10 scripts Python (dont 4 brouillons : teste.py, exemple1..3.py)
resultats_ex03/    40 fichiers JSON de sortie *versionnés*
google_veo_script.md  prompts vidéo marketing (hors périmètre technique)
```

## 2. Graphe de dépendances (C++)

```
Output ──hérite──▶ Hyperstatique ──utilise──▶ Isostatique
   │                    └─▶ Utils.h (removeByIndices, prod_list, flatten) ──▶ nlohmann/json
   ├─▶ Loading ──hérite──▶ Configuration
   ├─▶ ProjectPaths (std::filesystem)
   └─▶ Utils.h (delivery → écriture disque)
UpdatePositions ──▶ ProjectPaths, lit les JSON écrits par Output
```

Le cœur de calcul (`Hyperstatique`) inclut `Utils.h`, qui inclut
`nlohmann/json.hpp`, `<fstream>` et `<filesystem>`. **Le moteur dépend donc
transitivement de la couche de sérialisation**, ce qui contredit l'objectif
« le moteur ne dépend pas du JSON ».

## 3. Fiches par classe

### Isostatique — [Isostatique.h](../../LIPoutreContinue/include/LIPoutreContinue/core/Isostatique.h)
- **Rôle réel :** lignes d'influence exactes d'une travée simplement appuyée (M, V, w, θ) et termes de charge de Clapeyron (`Omega_Prime`, `Omega_Second`), coefficients de souplesse `a, b, c`.
- **Données :** E, I, L, steps, `nodePositions`, matrices pleines N×N produites à la demande.
- **Défauts :**
  - Maillage par accumulation flottante ([Isostatique.cpp:12-17](../../LIPoutreContinue/src/core/Isostatique.cpp#L12-L17)), voir 03-N8.
  - Pas de validation de `steps > 0` ni de `L ≥ 0` en C++ (boucle infinie si `steps <= 0`).
  - `Eq_*` renvoient des `std::vector` alloués à chaque appel.
  - Membres tous publics et mutables.

### Hyperstatique — [Hyperstatique.h](../../LIPoutreContinue/include/LIPoutreContinue/core/Hyperstatique.h)
- **Rôle réel :** résolution de la poutre continue par la méthode des foyers (rapports φ, φ′), assemblage des tenseurs `[S][N_s][N_total]` de M, V, w, θ et de la matrice `SupportMoment[S+1][N_total]`.
- **Défauts de conception :**
  - **Stocke des références** vers les `E, I, L, steps` de l'appelant ([Hyperstatique.h:30,59-60](../../LIPoutreContinue/include/LIPoutreContinue/core/Hyperstatique.h#L30)) → durée de vie à la charge de l'appelant. Les bindings contournent le problème par des copies (`PyOutput::E_`).
  - **Modifie les vecteurs de l'appelant** : les travées de longueur nulle sont retirées de `L`, `E`, `I` ([Hyperstatique.cpp:84-91](../../LIPoutreContinue/src/core/Hyperstatique.cpp#L84-L91)). Après construction, `L_spans` ne décrit plus la structure déclarée, et `node_lengths` perd la position des encastrements.
  - `SpanLeftSupportMoments`, `SpanRightSupportMoments`, `SpanLeftRight`, `BendingMomentStatic`, etc. : 5 tenseurs statiques N×N gardés en mémoire en permanence, alors que seuls `SupportMoment` (S+1 × N) et les formules fermées sont nécessaires.
  - `std::pow(-1.0, k)` dans une boucle interne ([Hyperstatique.cpp:222,240](../../LIPoutreContinue/src/core/Hyperstatique.cpp#L222)).
  - `std::async` lancé par travée *et* par grandeur (jusqu'à 4×S threads), sans mesure de bénéfice.

### Output — [Output.h](../../LIPoutreContinue/include/LIPoutreContinue/Output.h)
- **Rôle réel :** quatre responsabilités dans une seule classe : (1) déclencher le calcul, (2) stocker les résultats, (3) piloter `Loading`, (4) exporter en JSON.
- **Défauts :**
  - Hérite de `Hyperstatique` : un *résultat* « est un » solveur.
  - Ne fournit que des extrema |max| sous forme d'indices `(i,j,k)`. Il n'y a pas d'abscisse physique ni de min/max séparés.
  - « Enveloppe critique » ([Output.cpp:179-183](../../LIPoutreContinue/src/Output.cpp#L179-L183)) : n'utilise que `pos.i` (la **travée**) et refait une recherche sur toute cette travée. Le nom « section critique » est trompeur.
  - Le tenseur SF est passé à `Loading` avec la grille `X`, qui n'a pas la même longueur ([Output.cpp:163](../../LIPoutreContinue/src/Output.cpp#L163)), voir 03-N7.
  - `exportAll` écrit sur `std::cout` ([Output.cpp:246](../../LIPoutreContinue/src/Output.cpp#L246)).

### Loading / Configuration — [Loading.h](../../LIPoutreContinue/include/LIPoutreContinue/loading/Loading.h), [Input.h](../../LIPoutreContinue/include/LIPoutreContinue/loading/Input.h)
- **Rôle réel :** recherche de la position la plus défavorable des charges mobiles sur une ligne d'influence.
- **Défauts :**
  - **Tout le calcul est fait dans le constructeur** ([Loading.cpp:9-148](../../LIPoutreContinue/src/loading/Loading.cpp#L9-L148)) : construire un objet coûte O(N³).
  - Hérite de `Configuration` uniquement pour stocker `spans` et les charges. `YoungModule`, `Inertie` et `steps` y sont inutilisés.
  - Trois boucles quasi identiques (ponctuelle, répartie, combinée), recopiées une 4ᵉ fois dans `computeCriticalSection`.
  - Calcul des positions d'essieux recopié trois fois (lignes 205-211, 268-274, 440-446) et **divergent** dans `CombinedLoad` (ligne 420), voir 03-N4.
  - Résultats sous forme de `map<string, map<string,double>>` avec des clés construites à partir de l'intensité tronquée en entier (`to_string((int)I)`, ligne 425). Deux essieux de 12,4 et 12,6 kN produisent donc des clés `"12 …"` et `"13 …"`. Il est impossible d'exploiter ces données sans parser les chaînes.
  - `std::cout` dans `OnePointLoad` (ligne 185) : un effet de bord console dans le moteur.
  - `Indice_of` retrouve un indice par égalité flottante stricte après coup, au lieu de mémoriser l'argmax.

### ProjectPaths — [ProjectPaths.h](../../LIPoutreContinue/include/LIPoutreContinue/io/ProjectPaths.h)
- **Rôle :** arborescence d'export. Correct et isolé. `createAll()` crée aussi `05_Load_Positioning/*` même si `UpdatePositions` n'est jamais utilisé.

### UpdatePositions — [UpdatePositions.cpp](../../LIPoutreContinue/src/io/UpdatePositions.cpp)
- **Rôle réel :** relire les JSON d'enveloppes *écrits sur disque*, puis réécrire des lignes d'un ancien format texte (`… /Point/ x … :: nom`) en y injectant la position critique.
- **Constat :** vestige de l'ancien flux fichier (« Conversion C++ de update_positions_from_bm.py »). Il dépend du disque alors que les mêmes informations sont déjà en mémoire dans `Output`. Aucun exemple n'utilise ce format texte, et aucun parseur n'en est exposé (`LoadParser` n'est utilisé que par les bindings). Il est candidat à la dépréciation (voir 02).

### Position1D / Position2D / Position3D / CombineLoadPosition / load_delivery — [Utils.h:37-74](../../LIPoutreContinue/include/LIPoutreContinue/Utils.h#L37-L74)
- Ce sont des structures d'*indices* (`i, j, k`) sans unités ni abscisses. Les champs `i/j/k/val` ont une signification différente selon le contexte (pour `Position2D` dans `support_moment.json`, `j` est exporté sous le nom `alpha`).

### Load — [Utils.h:37-41](../../LIPoutreContinue/include/LIPoutreContinue/Utils.h#L37-L41)
- Le même type sert à la fois aux convois ponctuels et aux charges réparties. La sémantique de `Length` change selon la liste dans laquelle la charge est placée. Rien n'empêche de passer une charge répartie dans `point_loads`.

### Bindings — [bindings.cpp](../../LIPoutreContinue/bindings/bindings.cpp)
- Les propriétés `BM`, `SF`, `Def`, `Rot` **renvoient une copie complète en listes Python imbriquées à chaque accès** ([bindings.cpp:739-746](../../LIPoutreContinue/bindings/bindings.cpp#L739-L746)). `out.BM[0][3][7]` recopie le tenseur entier.
- Les structures sont renvoyées tantôt en `dict`, tantôt en objets (`Position3D`), alors que les stubs `.pyi` annoncent des classes ([__init__.py, commentaire « IMPORTANT »](../../python/Tsaraloha/LIPoutreContinue/__init__.py)).

### plot.py / report.py
- Ce sont des consommateurs des résultats, ce qui est correct dans le principe. `report.py` génère le HTML par f-strings géantes (≈ 650 lignes). Pour le PDF, il essaie successivement weasyprint, Chromium et pdfkit (voir 05).

## 4. Défauts transverses

1. Pas d'espace de noms C++ : `load`, `Output`, `Loading`, `json` (alias global dans un en-tête public) polluent l'espace global.
2. Gardes `#ifndef __HYP__` : identifiants réservés par le standard (double underscore). `#pragma once` suffit.
3. Aucune validation des entrées côté C++ (seul Python valide), alors que l'API C++ est présentée comme publique.
4. Conventions de nommage mélangées : français et anglais, `Rectangulare_LOAD`, `phy`, `PluralPointLoad`.
5. Aucune unité ni convention de signe portée par les données.
