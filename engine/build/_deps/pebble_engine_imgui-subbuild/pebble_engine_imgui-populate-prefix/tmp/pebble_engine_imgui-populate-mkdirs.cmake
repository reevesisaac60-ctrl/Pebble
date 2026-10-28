# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/astro/Projects/Pebble/engine/build/_deps/pebble_engine_imgui-src")
  file(MAKE_DIRECTORY "/home/astro/Projects/Pebble/engine/build/_deps/pebble_engine_imgui-src")
endif()
file(MAKE_DIRECTORY
  "/home/astro/Projects/Pebble/engine/build/_deps/pebble_engine_imgui-build"
  "/home/astro/Projects/Pebble/engine/build/_deps/pebble_engine_imgui-subbuild/pebble_engine_imgui-populate-prefix"
  "/home/astro/Projects/Pebble/engine/build/_deps/pebble_engine_imgui-subbuild/pebble_engine_imgui-populate-prefix/tmp"
  "/home/astro/Projects/Pebble/engine/build/_deps/pebble_engine_imgui-subbuild/pebble_engine_imgui-populate-prefix/src/pebble_engine_imgui-populate-stamp"
  "/home/astro/Projects/Pebble/engine/build/_deps/pebble_engine_imgui-subbuild/pebble_engine_imgui-populate-prefix/src"
  "/home/astro/Projects/Pebble/engine/build/_deps/pebble_engine_imgui-subbuild/pebble_engine_imgui-populate-prefix/src/pebble_engine_imgui-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/astro/Projects/Pebble/engine/build/_deps/pebble_engine_imgui-subbuild/pebble_engine_imgui-populate-prefix/src/pebble_engine_imgui-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/astro/Projects/Pebble/engine/build/_deps/pebble_engine_imgui-subbuild/pebble_engine_imgui-populate-prefix/src/pebble_engine_imgui-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
