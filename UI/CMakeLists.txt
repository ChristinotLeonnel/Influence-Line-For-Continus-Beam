cmake_minimum_required(VERSION 3.16)
project(MatrixOneViewer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 preferred, Qt5 fallback
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Charts)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Charts)

set(SOURCES
    main.cpp
    MainWindow.cpp
    ProjectBrowser.cpp
    InfluenceLineViewer.cpp
    CriticalValuesPanel.cpp
    LoadEnvelopePanel.cpp
    ImageGallery.cpp
    ModelInfoPanel.cpp
    BeamViewer.cpp
    BeamPanel.cpp
)
set(HEADERS
    MainWindow.h
    ProjectBrowser.h
    InfluenceLineViewer.h
    CriticalValuesPanel.h
    LoadEnvelopePanel.h
    ImageGallery.h
    ModelInfoPanel.h
    BeamViewer.h
    BeamPanel.h
    JsonUtils.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# nlohmann/json: header-only.
# Place json.hpp in third_party/nlohmann/ alongside this CMakeLists.txt
# OR install it system-wide and remove the line below.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
)

# High-DPI support (Qt5 compat shim; Qt6 enables it automatically)
if(QT_VERSION_MAJOR EQUAL 5)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        QT_DEPRECATED_WARNINGS
    )
endif()
