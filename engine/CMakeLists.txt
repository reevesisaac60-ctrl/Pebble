cmake_minimum_required(VERSION 3.25)

project(PebbleEngine
    VERSION 0.1.0
    DESCRIPTION "Pebble engine: native SDL3 + OpenGL 3.3 foundation with a custom PebbleUI framework in development"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PEBBLE_ENGINE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(PEBBLE_ENGINE_WITH_IMGUI "Build the Dear ImGui editor shell" ON)

function(pebble_engine_configure_target target)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )

    if(PEBBLE_ENGINE_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Dependencies: SDL3 (window, input, events, GL context), libepoxy (OpenGL
# function loading), OpenGL. Discovered via pkg-config modules.
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL3 REQUIRED sdl3)
pkg_check_modules(EPOXY REQUIRED epoxy)
find_package(OpenGL REQUIRED)

# ---------------------------------------------------------------------------
# Dear ImGui: official "docking" branch, fetched with CMake FetchContent.
# A system package was not present and its SDL3/OpenGL3 backends were not
# available, so the upstream source is used. This requires network access at
# configure time; see engine/README.md.
# ---------------------------------------------------------------------------
set(PEBBLE_ENGINE_HAS_IMGUI FALSE)

if(PEBBLE_ENGINE_WITH_IMGUI)
    include(FetchContent)

    FetchContent_Declare(
        pebble_engine_imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(pebble_engine_imgui)

    set(IMGUI_DIR "${pebble_engine_imgui_SOURCE_DIR}")

    file(GLOB IMGUI_CORE_SOURCES "${IMGUI_DIR}/imgui*.cpp")

    add_library(pebble_imgui STATIC
        ${IMGUI_CORE_SOURCES}
        "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    )

    target_include_directories(pebble_imgui PUBLIC
        "${IMGUI_DIR}"
        "${IMGUI_DIR}/backends"
    )

    # libepoxy resolves every OpenGL entry point, so the OpenGL3 backend must
    # not use its own loader. epoxy/gl.h is force-included below so the
    # backend sees GL types and entry points.
    target_compile_definitions(pebble_imgui PUBLIC
        IMGUI_IMPL_OPENGL_LOADER_CUSTOM=epoxy/gl.h
    )
    target_compile_options(pebble_imgui PRIVATE -include epoxy/gl.h)

    target_link_libraries(pebble_imgui PUBLIC
        ${SDL3_LIBRARIES}
        ${EPOXY_LIBRARIES}
        ${OPENGL_LIBRARIES}
    )

    set(PEBBLE_ENGINE_HAS_IMGUI TRUE)
    message(STATUS "PebbleEngine: Dear ImGui docking branch enabled (${IMGUI_DIR})")
endif()

# ---------------------------------------------------------------------------
# Engine library
# ---------------------------------------------------------------------------
add_library(pebble_engine_lib STATIC
    src/application.cpp
    src/ui/layout.cpp
    src/ui/ui_context.cpp
    src/ui/opengl_renderer.cpp
    src/editor_camera.cpp
    src/editor/asset_browser.cpp
    src/renderer/renderer.cpp
    src/renderer/sky.cpp
    src/renderer/shader.cpp
    src/renderer/mesh.cpp
    src/renderer/material.cpp
    src/renderer/model_importer.cpp
    src/scene/node.cpp
    src/scene/node3d.cpp
    src/scene/part.cpp
    src/scene/camera3d.cpp
    src/scene/directional_light.cpp
    src/scene/point_light.cpp
    src/scene/world_environment.cpp
    src/scene/scene.cpp
)

target_include_directories(pebble_engine_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(PEBBLE_ENGINE_HAS_IMGUI)
    target_link_libraries(pebble_engine_lib PUBLIC pebble_imgui)
    target_compile_definitions(pebble_engine_lib PUBLIC PEBBLE_ENGINE_HAS_IMGUI)
endif()

target_link_libraries(pebble_engine_lib PUBLIC
    ${SDL3_LIBRARIES}
    ${EPOXY_LIBRARIES}
    ${OPENGL_LIBRARIES}
)

pebble_engine_configure_target(pebble_engine_lib)

# ---------------------------------------------------------------------------
# Engine application
# ---------------------------------------------------------------------------
add_executable(pebble_engine
    src/main.cpp
)

target_link_libraries(pebble_engine PRIVATE pebble_engine_lib)

# Standalone custom UI demo. This lets PebbleUI be tested independently from
# the existing editor shell before any production UI migration is attempted.
option(PEBBLE_ENGINE_BUILD_UI_DEMO "Build the standalone PebbleUI demo" ON)
if(PEBBLE_ENGINE_BUILD_UI_DEMO)
    add_executable(pebble_ui_demo
        src/ui_demo.cpp
        src/ui/layout.cpp
        src/ui/ui_context.cpp
        src/ui/opengl_renderer.cpp
    )
    target_include_directories(pebble_ui_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(pebble_ui_demo PRIVATE ${SDL3_LIBRARIES} ${EPOXY_LIBRARIES} ${OPENGL_LIBRARIES})
    pebble_engine_configure_target(pebble_ui_demo)
endif()

# ---------------------------------------------------------------------------
# Engine assets: copy the whole assets/ tree into the build directory so the
# binary works regardless of the working directory. Copied at build time so
# asset changes are picked up on subsequent builds.
# ---------------------------------------------------------------------------
add_custom_target(pebble_engine_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "${CMAKE_CURRENT_BINARY_DIR}/assets"
    COMMENT "Copying engine assets into build directory"
)
add_dependencies(pebble_engine pebble_engine_assets)

# ---------------------------------------------------------------------------
# SDL diagnostic executable: tests SDL initialization independently of the
# renderer, window, GL context, and ImGui. Run with `./engine/build/sdl_diag`.
# ---------------------------------------------------------------------------
add_executable(sdl_diag
    src/main.cpp
)

target_link_libraries(sdl_diag PRIVATE pebble_engine_lib)
set_target_properties(sdl_diag PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# ---------------------------------------------------------------------------
# Scene graph tests (compiled against the engine sources directly).
# ---------------------------------------------------------------------------
add_executable(pebble_engine_tests
    tests/test_scene_graph.cpp
)

target_link_libraries(pebble_engine_tests PRIVATE pebble_engine_lib)
target_include_directories(pebble_engine_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
pebble_engine_configure_target(pebble_engine_tests)

enable_testing()
add_test(NAME pebble_engine_tests COMMAND pebble_engine_tests)


# ---------------------------------------------------------------------------
# Installation rules
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS pebble_engine sdl_diag
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(PEBBLE_ENGINE_BUILD_UI_DEMO)
    install(TARGETS pebble_ui_demo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(TARGETS pebble_engine_lib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY assets/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pebble/assets
    PATTERN "*.cmake" EXCLUDE
)

install(DIRECTORY ../scenes/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pebble/scenes
    OPTIONAL
)

install(DIRECTORY scenes/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pebble/scenes
    OPTIONAL
)


# PebbleUI layout tests are intentionally standalone: they exercise the pure
# responsive layout engine without requiring SDL, OpenGL, a window, or ImGui.
add_executable(pebble_ui_layout_tests
    tests/test_ui_layout.cpp
    src/ui/layout.cpp
)
target_include_directories(pebble_ui_layout_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
pebble_engine_configure_target(pebble_ui_layout_tests)
add_test(NAME pebble_ui_layout_tests COMMAND pebble_ui_layout_tests)

# PebbleUI interaction tests use a mock renderer and do not need a GPU.
add_executable(pebble_ui_context_tests
    tests/test_ui_context.cpp
    src/ui/layout.cpp
    src/ui/ui_context.cpp
)
target_include_directories(pebble_ui_context_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
pebble_engine_configure_target(pebble_ui_context_tests)
add_test(NAME pebble_ui_context_tests COMMAND pebble_ui_context_tests)
