# CHANGELOG

Corrections de bugs, ameliorations de l'affichage et nettoyage de l'arborescence.

---

## 1. Bugs corriges

### Critiques

| # | Fichier | Probleme | Correction |
|---|---|---|---|
| 1 | `Ploting/include/data_paths.hpp` | `influence_line_dir()` ignorait totalement la variable d'environnement `MATRIX_ONE_INFLUENCE_LINE_DIR` que `Ploting.cpp` definissait avant chaque appel. Le mecanisme de propagation du `configPath` etait donc casse. | Lecture de l'env var en priorite 1 (cf. la docstring qui le promettait). |
| 2 | `Ploting/include/animate_results.hpp` | `io::influence_line_dir(base_dir)` etait appele avec un repertoire de donnees la ou la fonction attend un chemin de fichier `path.json`. Si `base_dir` etait non vide, OpenCV ouvrait un dossier comme un fichier et echouait silencieusement. | Ajout d'un helper `io::resolve_base_dir(base_dir)` utilise partout, et appel correct dans `build_and_save_animation` et `animate_curvature`. |
| 3 | `Ploting/include/animate_results.hpp` | `cv::CAP_MSMF` (Microsoft Media Foundation) etait code en dur, donc les MP4 etaient impossible a generer hors Windows. | `#ifdef _WIN32` -> `CAP_MSMF`, sinon `CAP_FFMPEG`, avec retombee sur `CAP_ANY` si echec. |
| 4 | `Influence_Line/headers/Utils.h` | `_dupenv_s` (specifique MSVC) etait appele meme dans la branche `#else` POSIX -> ne compilait pas sur Linux/macOS. Path Windows contenait un `New Folder` accidentel. | `getEnvSafe` reecrit en fonction libre, branche `#else` utilisant `std::getenv`. Path Windows nettoye (`Documents/Matrix One/Influence Line`). Verifie : compile et tourne sur Linux. |
| 5 | `Influence_Line/src/Input.cpp` | BOM UTF-8 en debut de fichier ; en cas d'echec d'ouverture/parsing de `path.json`, le code continuait avec un `data` invalide et lisait `data["configPath"]` -> UB. | Reecriture sans BOM, exceptions levees aux points d'erreur, message clair, absence de tab/space mixte. |
| 6 | `Influence_Line/headers/JsonStreamWriter.h` | Le commentaire promettait "17 chiffres significatifs = round-trip garanti" mais le flux n'etait jamais configure -> precision 6 par defaut, perte de precision sur les valeurs d'ingenierie. | `out_ << std::setprecision(17)` defini une fois a la construction. |
| 7 | `Ploting/include/plot_context.hpp` | `std::min_element` / `std::max_element` utilises sans inclure `<algorithm>`. | `#include <algorithm>` ajoute explicitement. |

### Mineurs

| # | Fichier | Probleme | Correction |
|---|---|---|---|
| 8 | `Influence_Line/src/Output.cpp` | Mix tab/space ligne 92, `<iostream>` non inclus directement. | Indentation normalisee, include explicite. |
| 9 | `Ploting/include/envelope_plots.hpp` | `<iostream>`, `<sstream>`, `<iomanip>` non inclus directement (compilait par transitivite seulement). | Includes explicites. |

---

## 2. Arborescence (avant -> apres)

L'ancienne arborescence avait une **collision de prefixe** : `05_Load_Positioning/` (cree par
`UpdatePositions`) et `05_Output/` (cree par `Ploting`) cohabitaient au meme niveau, ce qui rendait
l'organisation difficile a lire. Les deux modules generaient sous le meme prefixe sans relation
entre eux.

```
AVANT                                APRES
<root>/                              <root>/
  01_Input/                            01_Input/
  02_Influence_Lines/                  02_Influence_Lines/
  03_Critical_Values/                  03_Critical_Values/
  04_Load_Envelopes/                   04_Load_Envelopes/
    Global/                              Global/
    Critical_Section/                    Critical_Section/
  05_Load_Positioning/    <-- conflit  05_Load_Positioning/
    Global/                              Global/
    Critical_Section/                    Critical_Section/
  05_Output/              <-- conflit  06_Plots/             <-- nouveau
    Plots/                                 All/
      All/                                 Maximum/
      Maximum/                             Envelopes/
      Envelopes/                             Point_Load/
        Point_Load/                          Distributed_Load/
        Distributed_Load/                    Combined_Load/
        Combined_Load/                   07_Animations/        <-- nouveau
    Animation/                             Results/
      Results/                               GIF/
        GIF/                                 MP4/
        MP4/                               Curvature/
      Curvature/                             GIF/
        GIF/                                 MP4/
        MP4/
```

Concretement les **plots statiques** vont desormais dans `06_Plots/` et les **animations**
(MP4 + GIF) dans `07_Animations/`. L'ordre numerique reflete le pipeline d'execution :
1 (input) -> 2 (lignes d'influence) -> 3 (maxima) -> 4 (enveloppes) -> 5 (positions txt)
-> 6 (plots) -> 7 (animations).

`ProjectPaths.h` a ete etendu pour exposer ces nouveaux chemins (`plots_*`, `anim_*`) et
`createAll()` cree desormais aussi ces dossiers.

---

## 3. Affichage console (avant -> apres)

Trois modules ecrivaient sur `stdout` avec trois formats incoherents :

```
AVANT
Output generated successfully in: /path/to/root          (Output.cpp)

[Critical_Section] Traitement de 4 courbes...            (UpdatePositions.cpp)
  [OK] Point_Load        - Bending Moment
  [ERR] Distributed_Load  - Shear Force : ...
[Critical_Section] Done.

=== Phase 1 : Structural Animations ===                  (Ploting.cpp)
    OK   shear_force
    ERR  bending_moment : ...
Phase 1 : Structural Animations completed.
```

```
APRES (format unique partout)
=====================================================
  INFLUENCE LINE - Continuous Beam Analysis
=====================================================

----- Phase A : Structural Analysis -----------------
  [ OK  ]  Analysis time : 1234 ms

----- Phase B : Update Load Positions ---------------

----- Update Positions [Global]----------------------
  [ OK  ]  Point_Load         | Bending Moment
  [ OK  ]  Distributed_Load   | Bending Moment
  [ OK  ]  Combined_Load      | Bending Moment
  ...

=====================================================
  PLOTTING PIPELINE
  root: /path/to/root
=====================================================

----- Phase 1 : Structural Animations ---------------
  [ OK  ]  shear_force
  [ OK  ]  bending_moment
  [ OK  ]  deflection
  [ OK  ]  rotation
----- done ------------------------------------------

----- Phase 2 : Static Plots ------------------------
  ...
----- done ------------------------------------------
```

Regles unifiees :
- Bandeau `=====` pour les sections d'application.
- En-tete `----- Phase X : titre -----` pour les phases.
- Pied `----- done -----` pour les phases.
- Statut `[ OK  ]` / `[ ERR ]` (largeur fixe, justifie a gauche).
- Item courant aligne avec `setw(18)` quand il y a deux colonnes.
- `::` separe l'item de la cause d'erreur.

Les fichiers concernes par l'unification : `Aplication.cpp`, `Output.cpp`, `UpdatePositions.cpp`,
`Ploting.cpp`, `envelope_plots.hpp`.

---

## 4. Nettoyage

- **Suppression** de `Ploting/include/gnuplot_init.hpp` : code mort, le projet n'utilise plus
  matplot++/gnuplot (uniquement OpenCV).
- **Suppression** dans `data_paths.hpp` du gros bloc `#undef` des macros de Python.h : le
  projet n'embarque plus de Python.

---

## 5. Verification

- Test de compilation Linux des branches modifiees de `Utils.h` (`getEnvSafe`, `getConfigPath`)
  reussi avec `g++ -std=c++20`.
- Aucune reference residuelle a `05_Output/`, `_dupenv_s` non guarde, `CAP_MSMF` non guarde,
  ni `gnuplot_init` ne subsiste dans le code (verifie par `grep`).
- Les chemins `06_Plots/` et `07_Animations/` sont coherents entre `Ploting.cpp`, les headers
  `plot_results.hpp` / `animate_results.hpp`, et `ProjectPaths.h`.

---

## 6. Compatibilite

Aucune modification d'API publique :
- `plotting::run(const std::string&)` inchange.
- Constructeur `Output` inchange.
- `Configuration::loadFromFile()` inchange.
- `UpdatePositions` inchange.

Les anciens dossiers `05_Output/` deja generes par d'anciens runs ne sont **pas** migres
automatiquement -- ils peuvent etre supprimes a la main si besoin.
