cmake_minimum_required(VERSION 3.25)

project(Pebble
    VERSION 0.3.0
    DESCRIPTION "Native Pebble language and game creation platform"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PEBBLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

function(pebble_configure_target target)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )

    if(PEBBLE_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()

add_library(pebble_frontend
   src/lexer.cpp
   src/parser.cpp
   src/package.cpp
   src/module.cpp
   src/type_checker.cpp
   src/codegen.cpp
)

target_include_directories(pebble_frontend PUBLIC include)
pebble_configure_target(pebble_frontend)

add_library(pebble_manifest STATIC
    src/manifest/manifest.c
    src/manifest/project.c
    src/manifest/resources.c
    src/manifest/build_graph.c
    src/manifest/resource_loader.c
)
target_include_directories(pebble_manifest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(pebble_manifest PRIVATE c_std_11)
target_compile_options(pebble_manifest PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
)

add_executable(pebble
    src/main.cpp
    src/cli.cpp
)
target_link_libraries(pebble PRIVATE pebble_frontend pebble_manifest)
pebble_configure_target(pebble)

include(CTest)
include(GNUInstallDirs)

target_compile_definitions(pebble PRIVATE
    PEBBLE_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/pebble"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/examples/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pebble/examples
    FILES_MATCHING
        PATTERN "*.peb"
        PATTERN "*.csv"
        PATTERN "*.vert"
        PATTERN "*.frag"
        PATTERN "README.md"
    PATTERN "build" EXCLUDE
)

set(PEBBLE_STUDIO_ICON ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png)

set(PEBBLE_STUDIO_DEFAULT_WALLPAPER_NAME "Windows XP.jpg" CACHE STRING
    "The file of the assets directory that the Studio shows by default"
)

file(GLOB PEBBLE_STUDIO_WALLPAPERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.jpg
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.jpeg
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.png
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.svg
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.webp
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.bmp
)

set(PEBBLE_STUDIO_WALLPAPER
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/${PEBBLE_STUDIO_DEFAULT_WALLPAPER_NAME}
)
if(NOT EXISTS ${PEBBLE_STUDIO_WALLPAPER})
    set(PEBBLE_STUDIO_MISSING_WALLPAPER ${PEBBLE_STUDIO_DEFAULT_WALLPAPER_NAME})
    list(SORT PEBBLE_STUDIO_WALLPAPERS)
    if(PEBBLE_STUDIO_WALLPAPERS)
        list(GET PEBBLE_STUDIO_WALLPAPERS 0 PEBBLE_STUDIO_WALLPAPER)
        get_filename_component(
            PEBBLE_STUDIO_DEFAULT_WALLPAPER_NAME
            ${PEBBLE_STUDIO_WALLPAPER}
            NAME
        )
        message(WARNING
            "The wallpaper '${PEBBLE_STUDIO_MISSING_WALLPAPER}' is not there. "
            "The wallpaper of the Studio is '${PEBBLE_STUDIO_DEFAULT_WALLPAPER_NAME}'."
        )
    else()
        message(FATAL_ERROR
            "The assets directory carries no image that the Studio could use "
            "as its wallpaper."
        )
    endif()
endif()

list(REMOVE_ITEM PEBBLE_STUDIO_WALLPAPERS
    ${PEBBLE_STUDIO_ICON}
    ${PEBBLE_STUDIO_WALLPAPER}
)

if(BUILD_TESTING)
    add_executable(pebble_tests tests/test_lexer.cpp)
    target_link_libraries(pebble_tests PRIVATE pebble_frontend)
    pebble_configure_target(pebble_tests)
    add_test(NAME pebble_lexer COMMAND pebble_tests)

    add_executable(pebble_parser_tests tests/test_parser.cpp)
    target_link_libraries(pebble_parser_tests PRIVATE pebble_frontend)
    pebble_configure_target(pebble_parser_tests)
    add_test(NAME pebble_parser COMMAND pebble_parser_tests)

    add_executable(pebble_type_tests tests/test_type_checker.cpp)
    target_link_libraries(pebble_type_tests PRIVATE pebble_frontend)
    pebble_configure_target(pebble_type_tests)
    add_test(NAME pebble_types COMMAND pebble_type_tests)

   add_executable(pebble_codegen_tests tests/test_codegen.cpp)
   target_link_libraries(pebble_codegen_tests PRIVATE pebble_frontend)
   pebble_configure_target(pebble_codegen_tests)
   add_test(NAME pebble_codegen COMMAND pebble_codegen_tests)

    add_executable(pebble_package_tests tests/test_packages.cpp)
    target_link_libraries(pebble_package_tests PRIVATE pebble_frontend)
    pebble_configure_target(pebble_package_tests)
    add_test(NAME pebble_packages COMMAND pebble_package_tests)

    add_executable(pebble_local_package_tests tests/test_local_packages.cpp)
    target_link_libraries(pebble_local_package_tests PRIVATE pebble_frontend)
    pebble_configure_target(pebble_local_package_tests)
    add_test(NAME pebble_local_packages COMMAND pebble_local_package_tests)
endif()


enable_language(C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(pebble_c_frontend
    src/c/token.c
    src/c/lexer.c
    src/c/ast.c
    src/c/parser.c
    src/c/package.c
    src/c/module.c
    src/c/type_checker.c
    src/c/codegen.c
)
target_include_directories(pebble_c_frontend PUBLIC include)
target_compile_options(pebble_c_frontend PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
)

add_executable(pebble_native
    src/c/main.c
    src/c/cli.c
)
target_link_libraries(pebble_native PRIVATE pebble_c_frontend pebble_manifest)
target_compile_options(pebble_native PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
)

install(TARGETS pebble pebble_native RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(test_lexer_c tests/test_lexer_c.c)
target_link_libraries(test_lexer_c PRIVATE pebble_c_frontend)
target_compile_options(test_lexer_c PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
)
add_test(NAME pebble_c_lexer COMMAND test_lexer_c)

add_executable(test_parser_c tests/test_parser_c.c)
target_link_libraries(test_parser_c PRIVATE pebble_c_frontend)
target_compile_options(test_parser_c PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
)
add_test(NAME pebble_c_parser COMMAND test_parser_c)

add_executable(test_type_checker_c tests/test_type_checker_c.c)
target_link_libraries(test_type_checker_c PRIVATE pebble_c_frontend)
target_compile_options(test_type_checker_c PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
)
add_test(NAME pebble_c_types COMMAND test_type_checker_c)


add_executable(test_codegen_c tests/test_codegen_c.c)
target_link_libraries(test_codegen_c PRIVATE pebble_c_frontend)
target_compile_options(test_codegen_c PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
)
add_test(NAME pebble_c_codegen COMMAND test_codegen_c)

add_executable(test_packages_c tests/test_packages_c.c)
target_link_libraries(test_packages_c PRIVATE pebble_c_frontend)
target_compile_options(test_packages_c PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
)
add_test(NAME pebble_c_packages COMMAND test_packages_c)

add_executable(test_local_packages_c tests/test_local_packages_c.c)
target_link_libraries(test_local_packages_c PRIVATE pebble_c_frontend)
target_compile_options(test_local_packages_c PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
)
add_test(NAME pebble_c_local_packages COMMAND test_local_packages_c)

add_executable(test_project_c tests/test_project.c)
target_link_libraries(test_project_c PRIVATE pebble_manifest)
target_compile_options(test_project_c PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
)
add_test(NAME pebble_project COMMAND test_project_c)

add_executable(test_resources_c tests/test_resources_c.c)
target_link_libraries(test_resources_c PRIVATE pebble_manifest)
target_compile_options(test_resources_c PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
)
add_test(NAME pebble_resources COMMAND test_resources_c)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_test(
    NAME pebble_native_cli
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_native_cli.py
        $<TARGET_FILE:pebble_native>
)

add_test(
    NAME pebble_parser_parity
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_parser_parity.py
        $<TARGET_FILE:pebble>
        $<TARGET_FILE:pebble_native>
)

add_test(
    NAME pebble_type_checker_parity
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_type_checker_parity.py
        $<TARGET_FILE:pebble>
        $<TARGET_FILE:pebble_native>
)

add_test(
    NAME pebble_runtime_parity
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_runtime_parity.py
        $<TARGET_FILE:pebble>
        $<TARGET_FILE:pebble_native>
)

add_test(
    NAME pebble_package_runtime_parity
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_package_runtime_parity.py
        $<TARGET_FILE:pebble>
        $<TARGET_FILE:pebble_native>
)

add_test(
    NAME pebble_log_runtime
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_log_runtime.py
        $<TARGET_FILE:pebble_native>
)

add_test(
    NAME pebble_cli_parity
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli_parity.py
        $<TARGET_FILE:pebble>
        $<TARGET_FILE:pebble_native>
)

add_test(
    NAME pebble_local_package_parity
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_local_package_parity.py
        $<TARGET_FILE:pebble>
        $<TARGET_FILE:pebble_native>
)

add_test(
    NAME pebble_documentation_examples
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_documentation_examples.py
        $<TARGET_FILE:pebble>
)

add_test(
    NAME pebble_examples
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_examples.py
        $<TARGET_FILE:pebble>
)

add_test(
    NAME pebble_resource_paths
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_resource_paths.py
        $<TARGET_FILE:pebble>
)
set_tests_properties(pebble_resource_paths PROPERTIES TIMEOUT 600)

add_test(
    NAME pebble_build_locations
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_build_locations.py
        $<TARGET_FILE:pebble>
        $<TARGET_FILE:pebble_native>
)

find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
    add_library(pebble_studio_assets OBJECT)
    target_link_libraries(pebble_studio_assets PUBLIC Qt6::Core)
    set_source_files_properties(
        ${PEBBLE_STUDIO_ICON}
        PROPERTIES QT_RESOURCE_ALIAS "assets/icon.png"
    )
    set_source_files_properties(
        ${PEBBLE_STUDIO_WALLPAPER}
        PROPERTIES QT_RESOURCE_ALIAS "assets/${PEBBLE_STUDIO_DEFAULT_WALLPAPER_NAME}"
    )
    qt_add_resources(pebble_studio_assets "studio_assets"
        PREFIX "/"
        FILES ${PEBBLE_STUDIO_ICON} ${PEBBLE_STUDIO_WALLPAPER}
    )

    add_library(pebble_studio_lib STATIC
        src/studio/studio.hpp
        src/studio/studio.cpp
        src/studio/intellisense.hpp
        src/studio/intellisense.cpp
        src/studio/glass_surface.hpp
        src/studio/glass_surface.cpp
        src/studio/theme.hpp
        src/studio/theme.cpp
        src/studio/assets.hpp
        src/studio/assets.cpp
    )
    set_target_properties(pebble_studio_lib PROPERTIES AUTOMOC ON)
    target_include_directories(pebble_studio_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/studio
    )
    target_link_libraries(pebble_studio_lib PUBLIC
        Qt6::Widgets pebble_frontend pebble_manifest
    )
    target_compile_definitions(pebble_studio_lib PRIVATE
        PEBBLE_STUDIO_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/pebble"
        PEBBLE_STUDIO_DEFAULT_WALLPAPER="${PEBBLE_STUDIO_DEFAULT_WALLPAPER_NAME}"
    )
    pebble_configure_target(pebble_studio_lib)

    add_executable(pebble_studio src/studio/main.cpp)
    target_link_libraries(pebble_studio PRIVATE pebble_studio_lib pebble_studio_assets)
    pebble_configure_target(pebble_studio)

    find_package(Qt6 COMPONENTS Test QUIET)
    if(Qt6Test_FOUND)
        add_executable(pebble_studio_tests tests/test_studio.cpp)
        set_target_properties(pebble_studio_tests PROPERTIES AUTOMOC ON)
        target_link_libraries(pebble_studio_tests PRIVATE
            pebble_studio_lib
            pebble_studio_assets
            Qt6::Test
        )
        pebble_configure_target(pebble_studio_tests)
        add_test(NAME pebble_studio COMMAND pebble_studio_tests)
        set_tests_properties(pebble_studio PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )

        add_executable(pebble_studio_theme_tests tests/test_studio_theme.cpp)
        set_target_properties(pebble_studio_theme_tests PROPERTIES AUTOMOC ON)
        target_link_libraries(pebble_studio_theme_tests PRIVATE
            pebble_studio_lib
            Qt6::Test
        )
        pebble_configure_target(pebble_studio_theme_tests)
        add_test(NAME pebble_studio_theme COMMAND pebble_studio_theme_tests)
        set_tests_properties(pebble_studio_theme PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()

    install(TARGETS pebble_studio RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${PEBBLE_STUDIO_ICON} ${PEBBLE_STUDIO_WALLPAPER}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/pebble/assets
    )
    if(PEBBLE_STUDIO_WALLPAPERS)
        install(FILES ${PEBBLE_STUDIO_WALLPAPERS}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/pebble/assets
        )
    endif()

    foreach(pebble_icon_size 16 24 32 48 64 128 256 512)
        install(
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/pebble-${pebble_icon_size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${pebble_icon_size}x${pebble_icon_size}/apps
            RENAME pebble-studio.png
        )
    endforeach()

    set(PEBBLE_STUDIO_BINDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/packaging/pebble-studio.desktop.in
        ${CMAKE_CURRENT_BINARY_DIR}/pebble-studio.desktop @ONLY
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pebble-studio.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()

# ---------------------------------------------------------------------------
# Pebble install rules (compiler/CLI and shared resources)
# ---------------------------------------------------------------------------
if(NOT TARGET pebble_install_rules_added)
    set(TARGET pebble_install_rules_added TRUE)
    include(GNUInstallDirs)
    
    install(TARGETS pebble pebble_native
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        OPTIONAL
    )
    
    install(DIRECTORY examples/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/pebble/examples
        OPTIONAL
    )
    
    install(FILES README.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/pebble/docs
        OPTIONAL
    )
    
    install(DIRECTORY docs/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/pebble/docs
        OPTIONAL
        PATTERN "*.cmake" EXCLUDE
    )
endif()
