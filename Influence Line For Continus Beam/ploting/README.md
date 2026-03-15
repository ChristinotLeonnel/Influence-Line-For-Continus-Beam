# Influence Line — C++ Port

## Architecture

```
influence_line_cpp/
├── CMakeLists.txt
├── include/
│   ├── data_paths.hpp        ← résolution du répertoire racine
│   ├── json_loader.hpp       ← cache JSON thread-safe (nlohmann)
│   ├── plot_context.hpp      ← contexte géométrique singleton
│   ├── plot_config.hpp       ← configuration visuelle typée
│   ├── plot_results.hpp      ← tracés statiques (PNG)
│   ├── animate_results.hpp   ← animations GIF + MP4
│   ├── thread_pool.hpp       ← pool de threads C++17
│   └── matplotlibcpp_final.h ← votre header corrigé Python 3.14
├── third_party/
│   └── nlohmann/
│       └── json.hpp          ← télécharger depuis https://github.com/nlohmann/json
└── src/
    └── main.cpp              ← pipeline principal
```

## Correspondance Python → C++

| Python                        | C++                                |
|-------------------------------|------------------------------------|
| `open_json()`                 | `io::open_json()` + `JsonCache`    |
| `load_plot_context()`         | `plots::load_plot_context()`       |
| `load_plot_config()`          | `plots::load_plot_config()`        |
| `plot_analysis_results()`     | `plots::plot_analysis_results()`   |
| `build_and_save_animation()`  | `animations::build_and_save_animation()` |
| `animate_curvature()`         | `animations::animate_curvature()`  |
| `ThreadPoolExecutor`          | `influence_line::ThreadPool`       |
| `convert_gif_to_mp4()`        | `animations::convert_gif_to_mp4()` |

## Nouvelle architecture des dossiers de données

```
Influence Line/
├── 01_Input/
│   └── structural_model.json
├── 02_Influence_Lines/
│   ├── bending_moment.json
│   ├── shear_force.json
│   ├── deflection.json
│   ├── rotation.json
│   ├── support_moment.json
│   ├── abscissa.json
│   ├── shear_abscissa.json
│   └── node_lengths.json
├── 03_Critical_Values/
│   ├── bending_moment.json   ← { "span": 2, "section": 16, "value": 123.4 }
│   ├── shear_force.json
│   ├── deflection.json
│   ├── rotation.json
│   └── support_moment.json
└── 05_Output/                ← généré automatiquement
    ├── Plots/
    │   ├── Maximum/
    │   └── All/
    └── Animation/
        ├── Results/
        │   ├── GIF/
        │   └── MP4/
        └── Curvature/
            ├── GIF/
            └── MP4/
```

## Dépendances

| Dépendance        | Version | Où se procurer                                      |
|-------------------|---------|-----------------------------------------------------|
| Python            | 3.14+   | https://python.org                                  |
| numpy             | latest  | `pip install numpy`                                 |
| matplotlib        | latest  | `pip install matplotlib`                            |
| nlohmann/json     | 3.11+   | https://github.com/nlohmann/json/releases           |
| ffmpeg            | any     | https://ffmpeg.org/download.html (ajouter au PATH)  |
| CMake             | 3.20+   | https://cmake.org                                   |

## Build (Windows MSVC)

```bat
:: 1. Placer nlohmann/json.hpp dans third_party/nlohmann/json.hpp
:: 2. Copier matplotlibcpp_final.h dans include/

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Release\influence_line.exe
```

## Build (Linux / GCC)

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/influence_line
```

## Optimisations clés vs Python

1. **JsonCache** : chaque fichier JSON n'est lu qu'une seule fois.
   En Python, `open_json()` rouvre le fichier à chaque appel.

2. **ThreadPool C++17** : pas de GIL sur les calculs C++.
   Les threads Python sont limités par le GIL.

3. **PlotConfig struct typée** : accès par champ compilé,
   vs. `config["style"]["line_color"]` résolu à l'exécution.

4. **enum CurveType** : dispatch O(1) vs. comparaisons de strings.

5. **std::filesystem** : gestion des chemins cross-platform sans
   dépendance à `pathlib`.
