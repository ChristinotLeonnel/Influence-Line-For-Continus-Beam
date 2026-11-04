# =============================================================================
#  CMakeLists.txt — Tsaraloha
#
#  Tsaraloha est une librairie chapeau destinée à regrouper plusieurs
#  sous-librairies de calcul. La première sous-librairie intégrée est
#  LIPoutreContinue (analyse structurelle de poutres continues).
#
#  Construit, pour la sous-librairie LIPoutreContinue :
#    1) la librairie coeur C++  "LIPoutreContinue_core"   (calcul pur, sans Python)
#    2) le module Python        "Tsaraloha.LIPoutreContinue._LIPoutreContinue"
#       (bindings pybind11, extension privée enveloppée par le paquet Python
#       python/Tsaraloha/LIPoutreContinue/, sur le modèle de numpy : le code
#       compilé de numpy est privé, et `import numpy` expose une API Python
#       propre par-dessus)
#
#  D'autres sous-librairies pourront être ajoutées ultérieurement de la même
#  façon : un dossier <NomDeLaSousLib>/ à la racine (include/src/bindings)
#  + un sous-paquet python/Tsaraloha/<NomDeLaSousLib>/, greffés ici par la
#  même mécanique que la section LIPoutreContinue ci-dessous.
#
#  Le module Python est généré AUTOMATIQUEMENT :
#    - pybind11 est détecté s'il est déjà installé (pip install pybind11),
#      sinon téléchargé et compilé automatiquement via FetchContent —
#      aucune installation manuelle requise, "cmake -B build" suffit.
#    - nlohmann_json n'est PAS une dépendance externe : il est déjà
#      embarqué dans include/nlohmann/ (partagé entre sous-librairies),
#      donc rien à installer pour lui non plus.
#
#  Détection du compilateur / IDE — AUCUNE action requise de ta part :
#    - Sous Windows, si tu ne passes pas -G, CMake choisit automatiquement
#      la version de Visual Studio la plus récente installée sur la machine
#      (VS 2022, VS 2026, ...) — c'est le comportement par défaut de CMake,
#      pas une option à activer. Rien dans ce fichier ne force une version.
#    - VS 2026 correspond au générateur CMake "Visual Studio 18 2026", need
#      CMake >= 4.2 pour être reconnu ; VS 2022 = "Visual Studio 17 2022"
#      (CMake >= 3.21). Si `cmake --version` est trop ancien, CMake ne verra
#      pas ta VS la plus récente et retombera sur une version antérieure —
#      mets à jour CMake dans ce cas (voir la démarche d'installation).
#    - Pour forcer une version précise malgré tout :
#          cmake -B build -G "Visual Studio 17 2022" -A x64
#
#  Usage direct (CMake seul) :
#      cmake -B build
#      cmake --build build --config Release -j
#      # -> build/python/Tsaraloha/LIPoutreContinue/_LIPoutreContinue<...>.pyd (Windows) ou .so (Linux/macOS)
#      # Windows (PowerShell) :
#      $env:PYTHONPATH = "build/python"; python -c "import Tsaraloha.LIPoutreContinue"
#      # Linux/macOS :
#      PYTHONPATH=build/python python3 -c "import Tsaraloha.LIPoutreContinue"
#
#  Usage "lib Python installable" (recommandé, comme numpy) :
#      pip install .
#      # nécessite le pyproject.toml (scikit-build-core) livré à côté de ce
#      # fichier — c'est lui qui invoque ce CMakeLists.txt pour toi.
#      # Ensuite, dans n'importe quel script :  import Tsaraloha.LIPoutreContinue
# =============================================================================

cmake_minimum_required(VERSION 3.18)
project(Tsaraloha LANGUAGES CXX)

message(STATUS "Tsaraloha: générateur sélectionné par CMake -> ${CMAKE_GENERATOR}")
if(MSVC)
    message(STATUS "Tsaraloha: compilateur MSVC détecté, version ${MSVC_VERSION} (${CMAKE_CXX_COMPILER})")
endif()

# ── Options ───────────────────────────────────────────────────────────────
option(TSARALOHA_BUILD_PYTHON   "Construire le(s) module(s) Python pybind11 des sous-librairies" ON)
option(TSARALOHA_BUILD_EXAMPLES "Construire les exécutables d'examples/ (si présents)" OFF)
option(TSARALOHA_BUILD_TESTS    "Construire les tests natifs C++ (tests/cpp, exécutés par ctest)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)   # requis pour lier les libs coeur dans un .so/.pyd Python

# Visual Studio (comme Xcode) est un générateur "multi-config" : Debug/Release
# coexistent dans le même dossier de build, et CMAKE_BUILD_TYPE est ignoré —
# la configuration se choisit au moment du build/install (--config Release).
# On ne force donc CMAKE_BUILD_TYPE que pour les générateurs mono-config
# (Makefiles, Ninja), utilisés sous Linux/macOS.
get_property(TSARALOHA_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT TSARALOHA_IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

# =============================================================================
#  Sous-librairie : LIPoutreContinue
#     Calcul pur (Isostatique/Hyperstatique/Output/Loading/UpdatePositions),
#     aucune dépendance à Python. Réutilisable telle quelle par un programme
#     C++ (voir examples/), le module Python n'est qu'un consommateur de plus.
# =============================================================================

# ── 1) Librairie coeur — "LIPoutreContinue_core" ────────────────────────────
add_library(LIPoutreContinue_core STATIC
    LIPoutreContinue/src/Output.cpp
    LIPoutreContinue/src/core/Hyperstatique.cpp
    LIPoutreContinue/src/core/Isostatique.cpp
    LIPoutreContinue/src/io/UpdatePositions.cpp
    LIPoutreContinue/src/loading/Input.cpp
    LIPoutreContinue/src/loading/Loading.cpp
    LIPoutreContinue/src/loading/MovingLoadSearch.cpp
)

target_include_directories(LIPoutreContinue_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/LIPoutreContinue/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# BendingMoments()/ShearForce()/Deflection()/Rotation() sont lancés en
# parallèle via std::async -> nécessite de lier pthread sous Linux/macOS
# (no-op sous Windows, Threads::Threads reste portable).
target_link_libraries(LIPoutreContinue_core PUBLIC Threads::Threads)

# PUBLIC : options nécessaires à tout code qui inclut les en-têtes du cœur.
# PRIVATE : niveau d'avertissement et parallélisme de build, propres à ce
# projet — ils ne doivent pas être imposés aux programmes consommateurs.
# L'optimisation est laissée à CMAKE_BUILD_TYPE (Release = -O3 / /O2).
if(MSVC)
    target_compile_options(LIPoutreContinue_core PUBLIC
        /EHsc          # exceptions C++ standard (requis par pybind11)
        /permissive-   # conformité stricte au standard C++ (comme GCC/Clang)
        /Zc:__cplusplus # __cplusplus reporte la vraie version C++ (sinon MSVC ment)
    )
    target_compile_options(LIPoutreContinue_core PRIVATE
        /W4            # warnings niveau 4 (équivalent -Wall -Wextra)
        /MP            # compilation multi-fichiers en parallèle -> build plus rapide
    )
else()
    target_compile_options(LIPoutreContinue_core PRIVATE -Wall -Wextra)
endif()

# ── 2) Exemples — optionnels, désactivés par défaut (TSARALOHA_BUILD_EXAMPLES=ON
#      pour les activer). Ignorés silencieusement si examples/ n'existe pas.
if(TSARALOHA_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LIPoutreContinue/examples/CMakeLists.txt")
    add_subdirectory(LIPoutreContinue/examples)
endif()

# ── 2bis) Tests natifs C++ — optionnels (TSARALOHA_BUILD_TESTS=ON), lancés
#      par `ctest -C Release`. Aucun framework externe.
if(TSARALOHA_BUILD_TESTS)
    enable_testing()
    add_executable(test_core tests/cpp/test_core.cpp)
    target_link_libraries(test_core PRIVATE LIPoutreContinue_core)
    add_test(NAME core_closed_forms COMMAND test_core)
endif()

# ── 3) Module Python — "Tsaraloha.LIPoutreContinue._LIPoutreContinue"
#      (extension privée, bindings pybind11), enveloppé par le paquet
#      pur-Python python/Tsaraloha/LIPoutreContinue/.
#      Modèle numpy : le code compilé est un détail d'implémentation privé
#      (préfixe underscore), l'utilisateur ne fait jamais que
#      `import Tsaraloha.LIPoutreContinue`.
if(TSARALOHA_BUILD_PYTHON)
    # PYBIND11_FINDPYTHON ON = pybind11 utilise le module CMake FindPython
    # moderne (plutôt que l'ancien FindPythonLibs) — nécessaire pour cibler
    # correctement le Python actif (venv, pyenv, etc.), y compris quand
    # pybind11 est récupéré via FetchContent ci-dessous.
    set(PYBIND11_FINDPYTHON ON)
    find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)

    # 1) On cherche d'abord un pybind11 déjà installé (pip install pybind11 —
    #    c'est le cas le plus courant et le plus rapide à configurer).
    find_package(pybind11 CONFIG QUIET)

    # 2) Sinon, on le télécharge et le compile automatiquement : aucune
    #    installation manuelle requise, "cmake -B build" suffit tel quel.
    if(NOT pybind11_FOUND)
        message(STATUS "Tsaraloha: pybind11 non trouvé localement -> téléchargement via FetchContent")
        include(FetchContent)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG        v2.13.6
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(pybind11)
    endif()

    pybind11_add_module(_LIPoutreContinue MODULE LIPoutreContinue/bindings/bindings.cpp)
    target_link_libraries(_LIPoutreContinue PRIVATE LIPoutreContinue_core)

    # /bigobj : les templates pybind11 génèrent beaucoup de symboles par
    # fichier objet -> nécessaire sous MSVC pour éviter l'erreur
    # "too many sections" sur bindings.cpp.
    if(MSVC)
        target_compile_options(_LIPoutreContinue PRIVATE /bigobj)
    endif()

    # Place le .so compilé dans build/python/Tsaraloha/LIPoutreContinue/, et
    # copie tout le paquet pur-Python (python/Tsaraloha/LIPoutreContinue/*.py,
    # *.pyi) à côté, pour que
    # `PYTHONPATH=build/python python3 -c "import Tsaraloha.LIPoutreContinue"`
    # fonctionne directement après le build, sans étape d'installation.
    # Le `$<1:...>` (expression génératrice triviale) empêche les générateurs
    # multi-configuration (Visual Studio, Xcode) d'ajouter un sous-dossier
    # `Release/`/`Debug/` : sans lui, le .pyd atterrissait dans
    # .../LIPoutreContinue/Release/ tandis que les .py étaient copiés un niveau
    # au-dessus, et `import` retombait silencieusement sur un autre backend.
    set_target_properties(_LIPoutreContinue PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/python/Tsaraloha/LIPoutreContinue>"
        RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/python/Tsaraloha/LIPoutreContinue>"
    )

    # ── Copie du paquet Python (racine Tsaraloha/ + sous-paquet) ───────────
    # Cible dédiée, exécutée à CHAQUE build : une modification d'un .py seul
    # est donc propagée même si l'extension C++ n'a pas besoin d'être
    # recompilée (avec une copie POST_BUILD, elle ne l'était pas).
    # CONFIGURE_DEPENDS : la liste des fichiers est réévaluée à chaque build
    # (fichier ajouté ou supprimé pris en compte sans reconfiguration).
    set(TSARALOHA_PY_ROOT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/python/Tsaraloha")
    set(TSARALOHA_PY_ROOT_OUT "${CMAKE_BINARY_DIR}/python/Tsaraloha")
    set(TSARALOHA_PY_PKG_SRC "${CMAKE_CURRENT_SOURCE_DIR}/python/Tsaraloha/LIPoutreContinue")
    set(TSARALOHA_PY_PKG_OUT "${CMAKE_BINARY_DIR}/python/Tsaraloha/LIPoutreContinue")
    file(GLOB TSARALOHA_PY_ROOT_FILES CONFIGURE_DEPENDS
         "${TSARALOHA_PY_ROOT_SRC}/*.py" "${TSARALOHA_PY_ROOT_SRC}/py.typed")
    file(GLOB TSARALOHA_PY_FILES CONFIGURE_DEPENDS
         "${TSARALOHA_PY_PKG_SRC}/*.py" "${TSARALOHA_PY_PKG_SRC}/*.pyi" "${TSARALOHA_PY_PKG_SRC}/py.typed")
    add_custom_target(tsaraloha_python_sources ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${TSARALOHA_PY_PKG_OUT}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TSARALOHA_PY_ROOT_FILES} "${TSARALOHA_PY_ROOT_OUT}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TSARALOHA_PY_FILES} "${TSARALOHA_PY_PKG_OUT}/"
        COMMENT "Synchronisation du paquet Python dans ${CMAKE_BINARY_DIR}/python"
        VERBATIM
    )
    add_dependencies(_LIPoutreContinue tsaraloha_python_sources)

    # Installation ("cmake --install", voir démarche fournie à côté de ce
    # fichier). Sous Windows, .pyd est un DLL -> RUNTIME DESTINATION (et non
    # LIBRARY, ignoré par les DLL) ; sous Linux/macOS c'est LIBRARY qui
    # s'applique. On garde les deux pour couvrir toutes les plateformes.
    install(TARGETS _LIPoutreContinue
        LIBRARY DESTINATION Tsaraloha/LIPoutreContinue
        RUNTIME DESTINATION Tsaraloha/LIPoutreContinue
    )
    if(EXISTS "${TSARALOHA_PY_ROOT_SRC}")
        install(FILES "${TSARALOHA_PY_ROOT_SRC}/__init__.py" "${TSARALOHA_PY_ROOT_SRC}/py.typed"
                DESTINATION Tsaraloha)
    endif()
    if(EXISTS "${TSARALOHA_PY_PKG_SRC}")
        install(DIRECTORY "${TSARALOHA_PY_PKG_SRC}/"
                DESTINATION Tsaraloha/LIPoutreContinue
                FILES_MATCHING PATTERN "*.py" PATTERN "*.pyi" PATTERN "py.typed")
    endif()
endif()
