# The Architecture of Pebble

Pebble is meant to be a complete platform for programs and games: a language,
a standard library, an engine for games and 3D applications, and a studio
that edits, builds and runs them. This document draws the line between those
four parts and says what stands behind the line today.

The document describes the **intended** architecture. Where a part does not
exist yet, the document says so. Nothing in here claims a feature that the
source of the repository does not have.

---

## The four layers

```
        ┌───────────────────────────────────────────────────────────┐
        │ 4. The native platform                                    │
        │    the operating system, the window system, the graphics  │
        │    API, the file system, the platform compiler            │
        └───────────────▲───────────────────────────▲──────────────┘
                        │                           │
        ┌───────────────┴───────────┐   ┌───────────┴──────────────┐
        │ 3. Pebble Studio          │   │ 2. Pebble Engine         │
        │    the editor, the build  │   │    the runtime of games   │
        │    and the run, the scene │   │    and of 3D programs     │
        │    editor, the viewport   │   │    rendering, resources,  │
        └───────────────▲───────────┘   │    scenes, entities       │
                        │               └───────────▲──────────────┘
        ┌───────────────┴───────────────────────────┴──────────────┐
        │ 1. Pebble Language and Standard Library                  │
        │    the language, the two frontends, the runtime of a     │
        │    program, the module system, the general packages      │
        └──────────────────────────────────────────────────────────┘
```

The arrows point upwards: a layer may use the layer below it, never the
other way round. The engine is built on the language and its runtime, the
studio is built on the compiler and on the engine, and the four layers rest
on the operating system.

| Layer | Source | Depends on |
| --- | --- | --- |
| 1. Language and standard library | `src/`, `src/c/`, `include/pebble/`, `include/pebble_c/`, `src/manifest/` | 4 |
| 2. Engine | *does not exist yet* | 1, 4 |
| 3. Studio | `src/studio/` | 1, 2, 4 |
| 4. Native platform | the system libraries | — |

---

## 1. The language and the standard library

This layer holds the language itself and the packages that every program may
use. It knows nothing about games, editors or graphics.

**What is in it**

* The language: the lexer, the parser, the type checker and the code
  generator, in `src/` for the C++20 frontend (`pebble`) and in `src/c/` for
  the C11 frontend (`pebble_native`). The two frontends implement the same
  language, the same package system and the same command line.
* The runtime of a program: the code that the frontends generate for a
  program, including the runtime of the packages.
* The module system: the local packages (`import package tools`), the
  projects and the manifest (`pebble.toml`, `pebble.lock`). The manifest
  layer in `src/manifest/` is plain C, so that both frontends use it without
  a C++ dependency.
* The general packages: `math`, `string`, `time`, `filesystem` and `Log`.
  They are in both frontends, they have the same behaviour in both, and they
  are documented in [packages.md](../packages.md).

**What is not in it**

* No window, no drawing, no scene. The `Window`, `Input` and `Math`
  packages of the C++ frontend are engine packages and are described in
  layer 2, even though their code sits in the code generator today.
* No editor, no project view, no inspector. That is layer 3.

**The rules**

* A package of this layer may not open a window, may not draw and may not
  read the settings of the studio. A package that needs a window belongs to
  the engine.
* The language and its runtime do not depend on the studio. The studio is
  built on the compiler, never the other way round.

---

## 2. The engine

The engine is the runtime that a game or a 3D program is built on. It is
meant to be a library, not a program: a Pebble program uses it through its
packages, and the studio uses the same library to draw what the program
draws.

**What it is meant to hold**

* Rendering: a device, a frame, a camera, a draw call, a shader program.
* Resources: shaders, textures, materials, meshes, models that are imported
  from a file, and the loading of all of them.
* A scene: entities, their components, the way they follow each other, and
  the way the scene is written and read.
* Simulation: lights, shadows, physics, audio and the scripting of a
  gameplay.

**What exists today**

Nothing that can be called an engine. The pieces that are close to it are:

* The `Window` and the `Input` packages of the C++ frontend. They are
  twenty lines of generated code that open a GLFW window and read the keys
  and the mouse. There is no renderer behind them: a program can open a
  window and poll the input, and nothing else.
* The `Math` package of the C++ frontend. It is the small part of a game
  runtime that is already useful. The `Log` package was here before and has
  moved to layer 1, because it needs no window.
* The resource index and the resource loader in `src/manifest/`. They scan a
  project, hash its files and load a text or a binary resource by its name.
  They are used by the compiler and the studio today, and they are written
  in plain C, so the engine can use them without a rewrite.

**What does not exist yet**

The renderer, the shaders, the textures, the materials, the lights, the
shadows, the cameras, the meshes, the model importer, the scene, the
entities, the physics, the audio and the scripting.

**The rules**

* The engine never reimplements the language. It is a library that the
  runtime of a program links, and its own interfaces are reached through
  packages.
* The engine never reads the state of the studio. The studio is a client of
  the engine, not the other way round. See
  [engine_vs_studio.md](engine_vs_studio.md).
* The engine never depends on Qt. The studio depends on the engine, so the
  engine may not depend on the studio.

---

## 3. Pebble Studio

The studio is the development environment: a Qt 6 Widgets application that
opens the files of a project, edits them, builds them and runs them.

**What is in it today**

* The window, the project tree, the tabs, the editor with its gutter, its
  syntax colours and its find and replace bar, the output pane, the
  diagnostics list and the status bar. The code is in `src/studio/`.
* The build and the run. The studio does not compile anything itself: it
  starts the `pebble` command as a process of its own. The command is
  looked up next to the studio binary, then in `$PEBBLE`, then in `PATH`.
* The diagnostics: the compiler prints `path:line:column: severity: message`
  and the studio turns that into the entries of the diagnostics list.

**What is not in it yet**

There is no scene editor, no viewport, no asset browser, no inspector and no
debugger. The studio is a code editor that can build and run a project.

**The rules**

* The studio never draws a 3D scene with a renderer of its own. The scene
  editor and the viewport of the future use the engine, so that what the
  editor shows and what the program shows are the same picture.
* The studio never loads a mesh, a texture or a model with a loader of its
  own. It uses the loader of the engine.
* The studio never changes the language. The syntax comes from
  [language.md](../language.md) and from the lexer of the compiler, and the
  highlighter of the editor follows the lexer.
* The studio may not be needed to build a program. A project is built and
  run with the `pebble` command alone, and the studio only drives that
  command.

---

## 4. The native platform

The layer that the three layers above rest on.

* **The platform compiler.** A Pebble program is translated into C++20 or
  C11 and compiled with the platform compiler. The C++ frontend uses
  `c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror` and links `glfw`, `GL`,
  `X11`, `pthread` and `dl`. The C frontend uses `cc -std=c11 -Wall -Wextra
  -Wpedantic -Wconversion -Wshadow -Werror` and links `lm`.
* **The window system and the graphics API.** GLFW and OpenGL are the
  window and the graphics of the C++ programs today. The engine will hold
  the rendering, and the windowing stays in the platform layer.
* **The file system.** The `filesystem` package of the language reads and
  writes the files of the system, and the manifest layer of the compiler
  scans a project.
* **The clock.** The `time` package reads the monotonic clock of the system.
* **Qt 6 Widgets.** The studio is built on it. The build skips the studio
  when Qt is not found, so the compiler and its tests build without it.

---

## The state of the layers today

| Layer | Exists | Missing |
| --- | --- | --- |
| 1. Language and standard library | the language, two frontends, the local packages, the manifest, the packages `math`, `string`, `time`, `filesystem` and `Log` | the packages `collections`, `random`, `json`, `regex`; a `read` builtin; a string type that can be joined in the C++ frontend (see [PACKAGE_PARITY.md](PACKAGE_PARITY.md)) |
| 2. Engine | the `Window`, `Input` and `Math` packages of the C++ frontend, the resource index of the compiler | the renderer, the shaders, the textures, the materials, the lights, the shadows, the cameras, the meshes, the model importer, the scene, the entities, the physics, the audio, the scripting |
| 3. Studio | the editor, the project tree, the build, the run, the diagnostics, the output, the theme | the scene editor, the viewport, the asset browser, the inspector, the debugger |
| 4. Native platform | the compiler, GLFW, OpenGL, X11, the file system, the clock, Qt | nothing is missing for what exists; the engine will need more of it |

## The documents of the repository

| Document | What it holds |
| --- | --- |
| [language.md](../language.md) | The syntax, the types and the rules of the language. |
| [packages.md](../packages.md) | The package system, the manifest, the standard library, the error behaviour. |
| [overview.md](overview.md) | This document: the four layers. |
| [engine_vs_studio.md](engine_vs_studio.md) | The boundary between the engine and the studio. |
| [PACKAGE_PARITY.md](PACKAGE_PARITY.md) | Which packages are in which frontend, and the backlog that closes the gap. |
| [project-assets.md](project-assets.md) | The proposal for the asset pipeline. Design only. |
| [../studio/claude-handoff.md](../studio/claude-handoff.md) | The state of the studio, its limits and its manual checklist. |
| [../studio/parity-backlog.md](../studio/parity-backlog.md) | The roadmap of the studio. |
