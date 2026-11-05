cmake_minimum_required(VERSION 3.25)

project(Pebble
    VERSION 0.3.3
    DESCRIPTION "Native Pebble language and game creation platform"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PEBBLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

function(pebble_configure_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(PEBBLE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        )
        if(PEBBLE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
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
if(MSVC)
    target_compile_options(pebble_manifest PRIVATE /W4 /WX)
else()
    target_compile_options(pebble_manifest PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
    )
endif()
if(MSVC)
    target_compile_definitions(pebble_manifest PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(pebble
    src/main.cpp
    src/cli.cpp
)
target_link_libraries(pebble PRIVATE pebble_frontend pebble_manifest)
pebble_configure_target(pebble)
if(MSVC)
    target_compile_definitions(pebble PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_compile_definitions(pebble PRIVATE PEBBLE_VERSION_STRING="${PROJECT_VERSION}")

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
if(MSVC)
    target_compile_options(pebble_c_frontend PRIVATE /W4 /WX)
    target_compile_definitions(pebble_c_frontend PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(pebble_c_frontend PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
    )
endif()

add_executable(pebble_native
    src/c/main.c
    src/c/cli.c
)
target_link_libraries(pebble_native PRIVATE pebble_c_frontend pebble_manifest)
if(MSVC)
    target_compile_definitions(pebble_native PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_compile_definitions(pebble_native PRIVATE PEBBLE_VERSION_STRING="${PROJECT_VERSION}")
if(MSVC)
    target_compile_options(pebble_native PRIVATE /W4 /WX)
else()
    target_compile_options(pebble_native PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
    )
endif()

install(TARGETS pebble pebble_native RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

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
