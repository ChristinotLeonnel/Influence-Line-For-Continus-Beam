cmake_minimum_required(VERSION 3.20)

project(InfluenceLineContinuousBeam
    VERSION     1.0.0
    DESCRIPTION "Hyperstatique — Continuous Beam Influence Line Analysis"
    LANGUAGES   CXX
)

# =============================================================================
#  C++ Standard
# =============================================================================
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# =============================================================================
#  Sources  (src/)
# =============================================================================
set(SOURCES
    src/Aplication.cpp
    src/Hyperstatique.cpp
    src/Input.cpp
    src/Isostatique.cpp
    src/Loading.cpp
    src/Output.cpp
    src/UpdatePositions.cpp
)

# =============================================================================
#  Headers  (headers/  +  headers/nlohmann/)
# =============================================================================
set(HEADERS
    headers/Hyperstatique.h
    headers/Input.h
    headers/Isostatique.h
    headers/JsonStreamWriter.h
    headers/Loading.h
    headers/Output.h
    headers/ProjectPaths.h
    headers/SpanResult.h
    headers/UpdatePositions.h
    headers/Utils.h
    headers/nlohmann/json.hpp
    headers/nlohmann/json_fwd.hpp
)

# =============================================================================
#  Executable
# =============================================================================
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# =============================================================================
#  Include Directories
#    - headers/          → #include "Hyperstatique.h" etc.
#    - headers/nlohmann/ → #include <nlohmann/json.hpp>  (via headers/ parent)
# =============================================================================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/headers
)

# =============================================================================
#  Compiler Options
# =============================================================================
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4         # High warning level
        /O2         # Full optimisation
        /EHsc       # Standard C++ exception handling
        /MP         # Multi-processor compilation
        /utf-8      # UTF-8 source and execution charset
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O2
    )
endif()

# =============================================================================
#  Linker — Threads (required by std::async on Linux / macOS)
# =============================================================================
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# =============================================================================
#  Compatibility — GCC < 9 needs explicit -lstdc++fs for <filesystem>
# =============================================================================
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif()

# =============================================================================
#  Install
# =============================================================================
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# =============================================================================
#  Summary
# =============================================================================
message(STATUS "------------------------------------------------------------")
message(STATUS "  Project  : ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  Compiler : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Standard : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build    : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Sources  : ${CMAKE_CURRENT_SOURCE_DIR}/src")
message(STATUS "  Headers  : ${CMAKE_CURRENT_SOURCE_DIR}/headers")
message(STATUS "------------------------------------------------------------")
