# Pebble Engine

Native SDL3 + OpenGL 3.3 foundation of the Pebble engine, with a Dear ImGui
docking editor shell. It opens a resizable window, renders entities from an
active scene (with per-vertex colored mesh) inside a dedicated ImGui viewport
panel, and runs independently of the Pebble compiler, runtime, tests, and
Qt Studio. The editor includes Hierarchy, Inspector, Console, and Project
panels with shared selection and basic scene save/load.

## What it does

- Initializes SDL3 and creates a resizable window with an OpenGL 3.3 Core
  context (via `SDL_GL_CreateContext`; function entry points are loaded by
  libepoxy, no manual OpenGL loading).
- Reports the actual OpenGL version and fails with a clear message when it is
  below 3.3.
- Runs an event/render loop: SDL3 events (`SDL_EVENT_QUIT`,
  `SDL_EVENT_WINDOW_CLOSE_REQUESTED`, `SDL_EVENT_KEY_DOWN`,
  `SDL_EVENT_WINDOW_RESIZED`), Escape to quit, frame-time-based rotation.
- Renders an actual 3D cube: vertex positions and per-vertex colors, VAO +
  VBO, indexed geometry with an element buffer, external vertex/fragment
  shaders, model + view + projection transforms, depth testing, dark
  background, drawable-size-aware viewport.
- Renders the cube into a small offscreen framebuffer (color texture + depth
  renderbuffer) and displays it in a Dear ImGui "Viewport" panel, keeping
  scene rendering and editor rendering separate.
- Dear ImGui docking shell (official `docking` branch, SDL3 + OpenGL3
  backends): Viewport, Hierarchy, Inspector, Console, Project panels.
- Shader source files live in `engine/assets/shaders/` and are copied into the
  build tree by CMake; they are also resolved at runtime relative to the
  executable (`PEBBLE_ENGINE_ASSETS` overrides).

## Required packages (Arch Linux)

```sh
sudo pacman -S sdl3 libepoxy glew glm  # glew/glm only if already used elsewhere
```

The engine itself needs:

```sh
sudo pacman -S sdl3 libepoxy
```

plus the base development tools already installed here:

```sh
sudo pacman -S cmake ninja gcc
```

Dear ImGui is fetched from GitHub at configure time (`GIT_TAG docking`),
so network access to github.com is required for the first configure.

## Build

```sh
cmake -S engine -B engine/build -G Ninja
cmake --build engine/build
```

Build outputs live in `engine/build/` and are ignored by Git. The root
project's `build/` directory is untouched.

## Test

```sh
cd engine/build
ctest --output-on-failure
```

The test suite (`tests/test_scene_graph.cpp`) covers scene graph hierarchy,
transforms, serialization round-trip, materials, editor camera behavior,
lighting, environment fog, reparenting cycle prevention, and asset browser
path resolution/filtering/missing-asset handling. See `tests/README.md` for
the full list.


## Run

```sh
./engine/build/pebble_engine
```

The working directory does not matter: shaders are resolved relative to the
executable, or via `PEBBLE_ENGINE_ASSETS=/path/to/assets`.

### SDL diagnostic mode

If the window does not open, run the SDL-only diagnostic to test SDL
initialization independently of the renderer, window, GL context, and ImGui:

```sh
./engine/build/sdl_diag --sdl-diag
```

It prints whether `SDL_Init(SDL_INIT_VIDEO)` succeeds, the SDL version, the
available video drivers, and the SDL error string immediately on failure.
You can also test individual video drivers:

```sh
SDL_VIDEODRIVER=wayland ./engine/build/sdl_diag --sdl-diag
SDL_VIDEODRIVER=x11   ./engine/build/sdl_diag --sdl-diag
SDL_VIDEODRIVER=offscreen ./engine/build/sdl_diag --sdl-diag
```

Neither driver is forced as the permanent default.

## Controls

| Input | Action |
| --- | --- |
| Escape | Close the application |
| Close button (×) | Close the application |
| Resize window | Viewport and framebuffer resize automatically |

The cube rotates continuously in three dimensions (independent X and Y
rotation, scaled by elapsed seconds, not frame count).

## Architecture overview

```
engine/
  CMakeLists.txt              # Independent CMake project (Ninja)
  src/main.cpp                # Entry point
  src/application.hpp/.cpp    # SDL3 window/GL context, event loop, ImGui shell
  src/renderer/renderer.hpp/.cpp   # Cube geometry, framebuffer, draw calls
  src/renderer/shader.hpp/.cpp     # Shader file load / compile / link with diagnostics
  src/math/vec3.hpp           # Minimal 3-vector math
  src/math/mat4.hpp           # Column-major 4x4 math (perspective, lookAt, rotate, translate)
  src/scene/node.hpp/.cpp     # Scene-graph node base: id, name, type, parent/child
  src/scene/node3d.hpp/.cpp   # Transform-bearing node (local + global transform)
  src/scene/part.hpp/.cpp     # Renderable primitive node (cube)
  src/scene/camera3d.hpp/.cpp # Camera node (operational view/projection)
  src/scene/directional_light.hpp/.cpp   # Directional light node (operational)
  src/scene/point_light.hpp/.cpp         # Point light node (operational)
  src/scene/world_environment.hpp/.cpp   # World environment / fog node (operational)
  src/scene/scene.hpp/.cpp   # Scene owner: node graph, hierarchy, serialization
  src/scene/selection.hpp    # Single-node selection
  src/editor_camera.hpp/.cpp # Editor-only viewport camera (not serialized)
  src/renderer/renderer.hpp/.cpp   # Cube geometry, framebuffer, lighting, fog
  src/renderer/shader.hpp/.cpp     # Shader file load / compile / link with diagnostics
  assets/shaders/cube.vert    # Vertex shader (OpenGL 3.3 core)
  assets/shaders/cube.frag    # Fragment shader (directional + point light, fog)
  README.md
```

- **SDL3**: window creation, input, events, OpenGL context creation, swap
  and drawable size.
- **libepoxy**: OpenGL entry points (headers `<epoxy/gl.h>`); no GLAD, no
  manual function loading.
- **OpenGL 3.3 Core**: the cube renderer. VAO/VBO/EBO, drawable-size
  viewport, depth test, framebuffer for the ImGui viewport.
- **Dear ImGui (docking branch)**: immediate-mode editor shell with the
  official `imgui_impl_sdl3` and `imgui_impl_opengl3` backends, dockspace,
  dark style. The scene renders into an FBO; ImGui samples its color texture
  in the Viewport panel via `ImGui::Image`.
- **Scene graph**: a hierarchy of `Node` objects owned by `Scene`. `Node3D`
  carries local position/rotation/scale and computes a global transform by
  composing with its `Node3D` ancestors. Concrete types include `Part`
  (renderable cube), `Camera3D`, `DirectionalLight`, `PointLight`, and
  `WorldEnvironment`. The Hierarchy and Inspector panels operate on this real
  scene graph; the renderer draws `Part` nodes using their global transforms.
- **Editor camera**: a separate, non-serialized camera used to navigate the
  viewport. Right-mouse drag looks around, WASD moves, the mouse wheel zooms,
  and `F` focuses the selected node. It never mutates a scene camera node.
- **Scene cameras**: `Camera3D` nodes are operational. The first `Camera3D`
  in the scene is the active game camera and drives the viewport's view and
  projection matrices. Invalid projection values are validated; scenes with no
  camera fall back to the editor camera.
- **Directional lighting**: one directional light (direction, colour,
  intensity) plus an ambient term, with diffuse and specular shading. The
  first `DirectionalLight` node in the scene drives the renderer.
- **Point lighting**: up to 4 point lights (position, colour, intensity,
  range) with quadratic distance attenuation. Additional point lights beyond
  the limit are ignored safely.
- **Environment fog**: `WorldEnvironment` drives fog colour, start and end
  distance using a linear camera-to-fragment distance metric. Degenerate
  ranges (end <= start) disable fog.

## Known limitations and next milestone

- In this environment, `SDL_Init(SDL_INIT_VIDEO)` fails at the OS level with
  `errno=11 (EAGAIN)` and no SDL error string, even though SDL2 works fine.
  The engine therefore cannot open a window here. Run `./engine/build/sdl_diag
  --sdl-diag` to see the exact diagnostics. The engine code itself is correct;
  the failure is session/environment-specific.
- The engine is a standalone application; it is not yet wired into the Pebble
  compiler or Qt Studio.
- Point lights are capped at 4 per frame (`kMaxPointLights`); additional point
  lights are ignored.
- Only directional lighting is implemented; point-light contributions are
  computed in the fragment shader but no shadowing or advanced materials exist.
- The Viewport panel renders at the panel size; there is no camera controller
  beyond the editor camera.
- ImGui multi-viewport (floating OS windows) is not enabled.

## Scene format

Scenes are serialized in a versioned binary format (`.pebscene`):

1. Magic: `PEBBLE_SCENE\0` (ASCII, null terminated)
2. Version (uint32, little-endian): `2`
3. Scene name: length (uint32) followed by UTF-8 bytes
4. Node count: uint32
5. For each node:
   - Node ID (uint64, little-endian)
   - Node type: length (uint32) followed by UTF-8 bytes
     (`Node3D`, `Part`, `Camera3D`, `DirectionalLight`, `PointLight`,
     `WorldEnvironment`)
   - Node name: length (uint32) followed by UTF-8 bytes
   - Parent ID (uint64, little-endian; `0` for no parent / scene root)
   - Type-specific properties:
     - `Node3D` / `Part` / `Camera3D` / `DirectionalLight` / `PointLight`:
       local transform — position, rotation, scale (3 float32 each)
     - `Part`: additionally primitive type (uint32)
     - `Camera3D`: field of view, near plane, far plane (float32 each)
     - `DirectionalLight`: direction, color (3 float32 each), intensity
     - `PointLight`: color (3 float32), intensity, range (float32 each)
     - `WorldEnvironment`: fog color (3 float32), fog start, fog end

Version 1 (legacy flat entities: id, name, transform, no type or hierarchy)
is still readable and migrated to a hierarchy of `Part` nodes on load.
Invalid headers, unsupported versions, duplicate ids, invalid parent
references, unsupported types and unreasonable collection sizes are rejected
gracefully without corrupting the active scene.

Starter scenes are provided in `scenes/` and `engine/scenes/`. The engine loads a starter scene on initialization. Use File → New/Save/Load in the editor to manage scenes.

## Editor interaction notes

- Viewport transform gizmos are drawn in the viewport window's draw list and clipped to the scene image, so they do not paint over other docked ImGui panels.
- Move, rotate, and scale have separate visual handles. `W`, `E`, and `R` switch modes when keyboard input is not being captured by an ImGui control. Transform snapping is disabled by default and can be enabled from the viewport controls.
- The Project panel is a navigable asset directory browser; the asset picker is rendered inside a single modal and supports case-insensitive extension filters.
- Dear ImGui style dimensions and fonts scale with the SDL display scale, and docked panels/list regions adapt to available window space.

