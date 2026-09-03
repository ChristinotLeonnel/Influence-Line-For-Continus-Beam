# =============================================================================
#  LIPoutreContinue/examples/CMakeLists.txt
#
#  Exemples C++ standalone — linkent directement contre LIPoutreContinue_core.
#  Activés via l'option CMake parente :
#      cmake -B build -DTSARALOHA_BUILD_EXAMPLES=ON
# =============================================================================

cmake_minimum_required(VERSION 3.18)

foreach(ex
    ex01_isostatique
    ex02_hyperstatique
    ex03_output_enveloppes
)
    add_executable(${ex} ${ex}.cpp)
    target_link_libraries(${ex} PRIVATE LIPoutreContinue_core)

    # Copie le binaire dans build/examples/ pour faciliter l'exécution
    set_target_properties(${ex} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples"
    )
endforeach()
