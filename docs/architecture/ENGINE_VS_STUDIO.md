# The Engine and the Studio

The studio and the engine will both draw a 3D scene: the studio to show the
scene while it is edited, the program to show the scene while it runs. This
document says which of the two owns what, so that the two pictures cannot
drift apart.

The short version: **the engine owns the scene, the studio owns the editing
of it.**

---

## The engine

The engine is a library. It is not a program, it has no window of its own
and it does not know that a studio exists.

**What the engine owns**

* The graphics device: the context, the frame, the camera, the draw calls.
* The resources: the shaders, the textures, the materials, the meshes, the
  models that are imported from a file, and the loading of all of them.
* The scene: the entities, their components, the way they follow each
  other, and the way a scene is written to a file and read from one.
* The simulation: the lights, the shadows, the physics, the audio and the
  scripting of a gameplay.

**How a program reaches the engine**

Through packages. A program that wants to draw calls a package of the
engine, and the package holds the bridge between the language and the
library. The engine itself never sees the source of a program, and a
package of the engine never re-implements a part of the engine.

**What the engine may not do**

* It may not depend on the studio, and it may not depend on Qt. The studio
  is a client of the engine, so the dependency may only point that way.
* It may not read the state of the studio: the open document, the selection,
  the settings of the editor. The engine is given a scene and draws it.
* It may not depend on the language. The engine is C++ (or C) code that
  stands beside the runtime of a program.

---

## The studio

The studio is a program. It is the place where a person edits a project.

**What the studio owns**

* The documents: the files of the project, the tabs, the editor, the
  diagnostics and the output.
* The project: the manifest, the dependencies, the build and the run. The
  studio drives the `pebble` command as a process; it does not compile
  anything itself.
* The editing of a scene: the panels, the tools, the undo, the selection,
  the inspector of the selected entity.

**What the studio may not do**

* It may not draw a scene with a renderer of its own.
* It may not load a mesh, a texture or a model with a loader of its own.
* It may not keep a second copy of the scene. The scene that is edited is
  the scene of the engine.
* It may not change the language. The syntax of the editor follows the
  lexer of the compiler.

---

## The rules for the scene editor and the viewport

The scene editor and the viewport are the parts of the studio that will
touch the engine. They are the reason this document exists, so the rules
are written down before the code is written.

1. **One engine, one scene.** The viewport of the studio and the program
   that the studio runs use the same engine library and the same scene
   object. What the editor shows is what the program shows.
2. **One loader.** A mesh, a texture and a model are loaded by the loader of
   the engine. The studio may ask for a resource, it may not read the file
   itself and it may not keep a second copy of it.
3. **One set of units.** The camera, the lights and the transforms of the
   editor are the camera, the lights and the transforms of the engine. The
   editor may not scale a model by a factor of its own.
4. **The editor drives, it does not patch.** The editor sends the changes
   of the user to the engine as commands, and the engine reports the state
   of the scene back. The editor does not write into the fields of a scene
   behind the back of the engine.
5. **The preview is the engine.** The preview of the studio is the engine
   running in a window of the studio. It is not a second, simplified
   renderer that shows an approximation of what the program draws.
6. **The engine runs without the studio.** A program that is built and run
   with the `pebble` command alone behaves the same as the same program
   that is run from the studio. The studio is never in the middle.

---

## The boundary in the code

| Concern | Lives in | Never lives in |
| --- | --- | --- |
| The syntax, the types, the packages | `src/`, `src/c/` | `src/studio/` |
| The rendering, the resources, the scene | the engine library (planned) | `src/studio/` |
| The editing of a project, the panels | `src/studio/` | the engine |
| The build of a program | the `pebble` command | `src/studio/` |
| The manifest, the project, the resource index | `src/manifest/` | `src/studio/` |

The studio starts the `pebble` command as a process of its own and reads its
output. It does not link the compiler as a library, and it will not link the
engine into the editor either: the editor drives the engine through the
same interface that a program uses.

---

## What is in place today

| Part | State |
| --- | --- |
| The studio edits the files of a project | in place |
| The studio builds and runs a project through the `pebble` command | in place |
| The studio shows the diagnostics of the compiler | in place |
| An engine library that a program and the studio can both use | missing |
| A scene that can be edited | missing |
| A viewport that draws the scene of the engine | missing |

So there is no boundary to keep between the engine and the studio today,
because the engine does not exist yet. The document describes the boundary
that the first engine has to respect, so that the studio is not written
against a second copy of the engine later.
