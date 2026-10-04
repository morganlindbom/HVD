# CMakeLists.txt
cmake_minimum_required(VERSION 3.21)
project(HVDComponentManager VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets Concurrent)
include(CTest)

add_library(hvd_components_core
    src/core/ComponentDefinition.cpp src/core/ComponentDefinition.h
    src/core/Catalogue.cpp src/core/Catalogue.h
    src/storage/ComponentPackage.cpp src/storage/ComponentPackage.h)
target_include_directories(hvd_components_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(hvd_components_core PUBLIC Qt6::Core)

set(HVD_KICAD_DEVELOPMENT_ROOT "" CACHE PATH "Explicit source-tree KiCad root for development only")
add_library(hvd_kicad src/storage/KiCadLibrary.h src/storage/KiCadLibrary.cpp src/storage/VrmlGeometry.cpp)
target_include_directories(hvd_kicad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(hvd_kicad PUBLIC Qt6::Core Qt6::Gui)
add_library(hvd_components_ui src/ui/MainWindow.cpp src/ui/MainWindow.h
    src/ui/ComponentEditor.cpp src/ui/ComponentEditor.h src/ui/KiCadBrowser.cpp src/ui/KiCadBrowser.h
    src/ui/ModelPreview.cpp src/ui/ModelPreview.h
    src/ui/DemoBoardGeometry.cpp src/ui/DemoBoardGeometry.h)
target_link_libraries(hvd_components_ui PUBLIC hvd_components_core hvd_kicad Qt6::Concurrent Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)
if(HVD_KICAD_DEVELOPMENT_ROOT)
    target_compile_definitions(hvd_components_ui PRIVATE HVD_KICAD_DEVELOPMENT_ROOT="${HVD_KICAD_DEVELOPMENT_ROOT}")
endif()
add_executable(hvd_component_manager WIN32 src/app/main.cpp)
target_link_libraries(hvd_component_manager PRIVATE hvd_components_ui)
add_custom_command(TARGET hvd_component_manager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/components
    $<TARGET_FILE_DIR:hvd_component_manager>/components)

foreach(target hvd_kicad hvd_components_core hvd_components_ui hvd_component_manager)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(kicad_tests tests/KiCadTests.cpp)
    target_link_libraries(kicad_tests PRIVATE hvd_components_ui Qt6::Test)
    target_compile_definitions(kicad_tests PRIVATE KICAD_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/kicad")
    add_test(NAME kicad-tests COMMAND kicad_tests)
    add_executable(core_tests tests/CoreTests.cpp)
    target_link_libraries(core_tests PRIVATE hvd_components_core Qt6::Test)
    target_compile_definitions(core_tests PRIVATE BUNDLED_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/components")
    add_test(NAME core-tests COMMAND core_tests)
    add_executable(ui_tests tests/UiTests.cpp)
    target_link_libraries(ui_tests PRIVATE hvd_components_ui Qt6::Test)
    target_compile_definitions(ui_tests PRIVATE BUNDLED_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/components")
    add_test(NAME ui-smoke COMMAND ui_tests)
    add_executable(model_preview_tests tests/ModelPreviewTests.cpp)
    target_link_libraries(model_preview_tests PRIVATE hvd_components_ui Qt6::Test)
    target_compile_definitions(model_preview_tests PRIVATE BUNDLED_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/components")
    add_test(NAME model-preview-tests COMMAND model_preview_tests)
    if(WIN32)
        set_tests_properties(kicad-tests ui-smoke model-preview-tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=windows")
    endif()
endif()
