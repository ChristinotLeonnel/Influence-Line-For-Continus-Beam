# Matrix One — Influence Line Viewer

Interface graphique Qt pour visualiser les resultats produits par
**Influence_Line_For_Continus_Beam** et **Ploting**.

---

## Arborescence du projet

```
MatrixOneViewer/
 ├── CMakeLists.txt          Fichier de build CMake (Qt6 ou Qt5)
 ├── main.cpp                Point d'entree, palette sombre, DPI
 ├── MainWindow.h/cpp        Fenetre principale (toolbar + dock + stack)
 ├── ProjectBrowser.h/cpp    Arborescence du projet (QTreeWidget)
 ├── InfluenceLineViewer.h/cpp  Graphe interactif des lignes d'influence
 ├── CriticalValuesPanel.h/cpp  Tableau des valeurs critiques
 ├── LoadEnvelopePanel.h/cpp    Tableau + graphe en barres des enveloppes
 ├── ImageGallery.h/cpp      Grille de miniatures PNG avec zoom
 ├── ModelInfoPanel.h/cpp    Resume du modele structurel
 └── JsonUtils.h             Utilitaires JSON header-only
```

---

## Prerequis

| Composant  | Version minimale |
|------------|-----------------|
| Qt         | 5.15 ou 6.x     |
| CMake      | 3.16+           |
| Compilateur| C++20           |
| nlohmann/json | 3.x (header-only) |

### Installation de nlohmann/json

Telecharger `json.hpp` depuis https://github.com/nlohmann/json/releases
et le placer dans :

```
MatrixOneViewer/third_party/nlohmann/json.hpp
```

Ou installer la lib systemiquement (ex. `apt install nlohmann-json3-dev`)
et adapter `CMakeLists.txt`.

---

## Compilation

```bash
# Creer le dossier de build
cd MatrixOneViewer
mkdir build && cd build

# Configurer (Qt6 en priorite, Qt5 en fallback)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Compiler
cmake --build . --config Release -j$(nproc)

# Lancer
./MatrixOneViewer
# ou sur Windows :
MatrixOneViewer.exe
```

Pour specifier Qt explicitement :

```bash
cmake .. -DQt6_DIR=/path/to/qt6/lib/cmake/Qt6
# ou
cmake .. -DQt5_DIR=/path/to/qt5/lib/cmake/Qt5
```

---

## Utilisation

### Ouvrir un projet

1. Cliquer **📂 Open Project** dans la barre d'outils
2. Choisir le dossier qui contient `path.json`
   (ex. `~/Documents/Matrix One/Influence Line/`)
3. **Ou** glisser-deposer le dossier directement sur la fenetre

Le viewer accepte aussi un chemin en argument :

```bash
./MatrixOneViewer "/home/user/Documents/Matrix One/Influence Line"
```

### Navigation

Dans l'arborescence a gauche :

| Noeud | Panneau affiche |
|-------|----------------|
| Structural Model | Parametres du modele (portees, E, I, pas) |
| Influence Lines > BM / SF / Def / Rot | Graphe interactif avec selecteurs de travee et section |
| Critical Values > All curves | Tableau des maxima absolus |
| Load Envelopes > Global / Critical_Section | Tableau + histogramme par type de charge |
| Plots (06_Plots) > All / Maximum / Envelopes | Galerie de miniatures PNG avec zoom |

### Graphe de ligne d'influence

- **Selecteur de courbe** : BM / SF / Deflection / Rotation
- **Spinner Span** : numero de la travee (1-based)
- **Spinner Section** : numero de la section (1-based)
- La valeur critique est marquee en rouge ⚡ si la section correspond a la section critique
- Hover sur la courbe : coordonnees affichees dans Qt Charts

### Galerie d'images

- Cliquer une miniature pour ouvrir l'image en plein ecran
- Champ de filtre pour chercher par nom de fichier
- Raccourci clavier **Echap** ou **Entree** pour fermer le zoom

---

## Palette de couleurs

Le theme suit la palette **Catppuccin Mocha** :

| Role | Couleur |
|------|---------|
| Fond principal | `#1e2230` |
| Fond secondaire | `#181825` |
| Texte | `#cdd6f4` |
| Accent violet | `#cba6f7` |
| Succes (vert) | `#a6e3a1` |
| Erreur (rouge) | `#f38ba8` |
| Info (bleu) | `#89b4fa` |

---

## Formats JSON attendus

### `01_Input/structural_model.json`
```json
{
  "spans": [6.0, 8.0, 6.0],
  "young_modulus": [2e11, 2e11, 2e11],
  "inertia": [1e-4, 1e-4, 1e-4],
  "step": 0.5,
  "node_lengths": [0, 6, 14, 20],
  "n_spans": 3,
  "n_total_nodes": 121
}
```

### `02_Influence_Lines/bending_moment.json`
Tenseur `[span][section][alpha]` de doubles.

### `03_Critical_Values/bending_moment.json`
```json
{ "span": 1, "section": 42, "alpha": 33, "value": 12.456 }
```

### `04_Load_Envelopes/Global/Point_Load/bending_moment.json`
```json
{
  "maximum": 48.3,
  "span": 1,
  "section": 42,
  "position": 3.5,
  "load": {
    "P1": { "alpha": 12, "value": 10.0, "Position": 3.5 }
  }
}
```
