# Pebble OpenGL examples

These examples use the built-in `OpenGL` package and the shared GLFW window
provided by `Window`. `OpenGL:Initialize()` loads OpenGL 3.3 entry points from
the current context. Programs, vertex arrays, and buffers are opaque integer
handles owned by the generated native runtime; pass them back to the package
and release them before closing the window.

The package provides viewport and clear/depth state, shader compilation and
program linking, scalar/vector/matrix uniforms, vertex arrays and buffers,
RGBA textures with filter/wrap configuration, attribute layouts, and triangle drawing. Vertex and index data are currently
comma-separated numeric strings uploaded with `UploadArrayBuffer` and
`UploadElementBuffer`; texture pixels use comma-separated 0–255 RGBA byte
values with `UploadTextureRgba`. Image-file decoding, non-triangle topologies,
and automatic window-resize callbacks are not included in this first package
version.

The spinning cube demonstrates GLSL, model/view/projection matrices, and a
rainbow material whose bands flow across every face with time. The binary cube
draws itself as text: the screen is divided into character cells, and each cell
of the cube shows one of the glyphs `.` `~` `+` `#` `$`, chosen by the light on
that cell, so every visible face has its own density. It needs a GLSL
3.30-capable driver.

## Requirements

- GLFW 3 development/runtime library.
- An OpenGL 3.3 Core-capable GPU and driver.
- A graphical desktop session for running the windows.
- A C++20 compiler used by `pebble build` to compile generated native code.

On Debian or Ubuntu, the usual dependencies are `libglfw3-dev` and
`libgl1-mesa-dev`.

## Running the examples

Each example reads its assets (`cube_vertices.csv`, `cube_indices.csv` and its
two shaders) by name, relative to the program itself. The working directory does
not matter, so the examples run the same way from a source checkout, from
Pebble Studio and from an installation:

```sh
pebble run examples/opengl/spinning_cube.peb
pebble run examples/opengl/binary_shader_cube.peb
pebble run /any/where/examples/opengl/binary_shader_cube.peb   # from any directory
```

When a program calls `filesystem.read` with a relative path, Pebble looks for it
in this order:

1. relative to the working directory, exactly as before, so programs that
   depend on ordinary relative paths read what they always read;
2. in the directories listed in the `PEBBLE_RESOURCE_PATH` environment
   variable (separated by `:`, or `;` on Windows);
3. in the directory of the program;
4. in the root of the program's project, the nearest directory above it that
   holds a `pebble.toml`;
5. in the resource directory of the Pebble installation,
   `${CMAKE_INSTALL_PREFIX}/share/pebble`, which is found next to the running
   `pebble` executable first, so a moved installation keeps working.

Only paths that do not exist relative to the working directory use steps 2 to
5. An absolute path is never searched for elsewhere. The roots of steps 3 to 5
are fixed when `pebble build` or `pebble run` generates the program.

Pebble Studio adds the directories of the examples it knows to step 2 when it
runs a program, so that a program that was copied out of the examples still
finds the files it reads, such as `cube_vertices.csv`. The examples of an
installation live in `${CMAKE_INSTALL_PREFIX}/share/pebble/examples`, beside
the assets of the Studio.

The directories of step 2 are a list, and the list is separated by the
separator of the platform, `:` on Unix and `;` on Windows. The Studio writes
the list into `PEBBLE_RESOURCE_PATH` and joins it with that same separator,
and it keeps the rest of the environment of the Studio, so the compiler that
`pebble` starts finds its own tools.

The Studio starts the `pebble` executable that stands next to it, so the
Studio and the CLI belong to the same build. A CLI that is older than the
Studio cannot know what the Studio asks of it: the programs it builds look
for the files they read in their working directory only, and a program that
lives outside of the examples does not find its files.

The install places the CLI under the configured binary directory and the
example programs together with their assets under
`${CMAKE_INSTALL_PREFIX}/share/pebble/examples`. A program that is copied
somewhere else needs its assets copied next to it.
