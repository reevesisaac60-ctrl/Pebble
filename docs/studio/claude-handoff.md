# Pebble — Handoff Package

Status: the Studio is usable, the language and the compiler are in place, and
the optional parity items are deferred on purpose. This document is written
for a developer who takes the project over without the history of this chat.

Verified on 2026-10-22 on Linux (Qt 6.11.2, GCC 16.2, CMake 4.4.4, Ninja
1.13, Python 3.14) with `build` as the build directory.

---

## 1. What Pebble is

Pebble is a small native language for programs and games, with a compiler and
an editor.

* A Pebble program is translated into C++20 or C11 and compiled with the
  platform compiler, so a program becomes a native executable without a
  virtual machine.
* Two frontends implement the same language, the same package system and the
  same command line: `pebble` (C++20) and `pebble_native` (C11). The C11
  frontend is migrated alongside the C++ frontend and the test suite compares
  the two on every change.
* **Pebble Studio** (`src/studio/`) is the editor: a Qt 6 Widgets application
  with a project tree, tabs, a code editor with a gutter and syntax
  highlighting, find and replace, a build and a run, an output pane with
  levels, a diagnostics list, a status bar and a theme of its own.

The direction of the project is a complete game-creation platform, editor and
runtime. The editor and the language are the parts that exist today; the
engine, the assets pipeline and the multiplayer are not implemented. The
design note for the asset pipeline is `docs/architecture/project-assets.md`
and is explicitly marked *design only*.

The Python editor `pebble_studio_qt.py` in the root of the repository is a
**read-only design reference**. It is not runnable here (it imports
`pebble_studio_common`, and it expects `pebble_wallpaper.svg` and
`assets/icons/pebble.png`, none of which exist), and it describes an older
dialect of the language. Do not copy its keywords into the editor: write
against `docs/language.md` and against the lexer of the compiler.

The Studio itself carries the artwork of the user: `assets/icon.png` is the
icon of the program and `assets/Windows XP.jpg` is the background of the
window. The icon, the wallpaper, the installation and the launcher of
VS Codium are described in `docs/studio/branding.md`.

## 2. Architecture

```
pebble source (.peb)
   │
   ├─ src/lexer.cpp        tokens          (C++ frontend, include/pebble/)
   ├─ src/parser.cpp       syntax tree
   ├─ src/type_checker.cpp types
   ├─ src/codegen.cpp      C++20 source
   ├─ src/package.cpp      packages        (src/manifest/ is shared)
   └─ src/module.cpp       imports
        │
        └─ the platform compiler (c++ / $CXX)  →  a native executable
```

The same pipeline exists a second time in C: `src/c/*.c` with the headers in
`include/pebble_c/`. The C frontend has the same stages in the same order and
the same command line, and the parity tests run both frontends on the same
input and compare the result.

`src/manifest/` and `include/pebble_manifest/` hold the layer that both
frontends share: the `pebble.toml` manifest, the project lookup, the
resource index and the loader. It is plain C, so that the C frontend can use
it without a C++ dependency.

The Studio is a separate library, `pebble_studio_lib`, so that the test
binary can drive the real window instead of a copy of it.

| Path | What it holds |
| --- | --- |
| `include/pebble/`, `src/` | The C++20 frontend: lexer, parser, type checker, code generator, packages, modules, CLI. |
| `include/pebble_c/`, `src/c/` | The C11 frontend, the same pipeline. |
| `include/pebble_manifest/`, `src/manifest/` | The manifest, the project, the resource index and the loader; shared by both frontends. |
| `src/studio/` | The Studio: `studio.hpp` / `studio.cpp` (the window, the editor, the panels) and `theme.hpp` / `theme.cpp` (the visual system). |
| `tests/` | The unit tests of the compiler, the parity tests and the Studio tests. |
| `docs/` | `language.md`, `packages.md`, `architecture/project-assets.md` and `studio/`. |
| `pebble_studio_qt.py` | The read-only reference of the old editor. Do not modify. |

## 3. The Studio in detail

* `src/studio/studio.hpp` — the public surface of the window. Everything the
  tests drive is public here: opening and closing documents, the menus, the
  status bar, the panels, the diagnostics, the recent files.
* `src/studio/studio.cpp` — the implementation. The main classes are
  `MainWindow`, `CodeEditor` (a `QPlainTextEdit` with a line number area and
  the key handling of the reference), `DocumentEditor` (a `CodeEditor` that
  knows its path and its dirty state), `ProjectTree`, `FindReplaceBar`,
  `PebbleHighlighter` and `OutputHighlighter`.
* `src/studio/theme.hpp`, `src/studio/theme.cpp` — the visual system: the
  colours of the surfaces, the `QPalette`, the style sheet, the fonts and the
  drawn icons. The application is put into the Fusion style and the theme is
  applied before the window is built (`applyStudioTheme`).

### The editor

* Syntax highlighting with the eight categories of the reference (keyword,
  string, number, comment, builtin, boolean, call, operator), following the
  rules of the lexer of the compiler, so that the code of the *implemented*
  dialect is the code that is coloured.
* A line number gutter whose width follows the number of lines, and a current
  line highlight.
* The keys of the reference: `Tab` and `Shift+Tab` indent and outdent, `Enter`
  keeps the indentation, `Ctrl+/` toggles the `//` comment of the selected
  lines.
* A find and replace panel above the editor: `Ctrl+F`, `Ctrl+H`, `F3`,
  `Shift+F3`, `Esc`.

### The workspace

* An activity bar of 44 px on the left that switches the Explorer and the
  output; the switch of the panel that is on the screen is marked. The state
  of the workspace survives a restart.
* A status bar that names the document that is active, the line and the
  column of the cursor and a state word, and a window title that follows the
  document and marks a changed one.
* An output pane with the levels `normal`, `info`, `warning` and `error`, a
  bound of 5 000 lines and a *Clear* button, and a diagnostics list whose
  head carries the number of the entries.

### The processes

* A build and a run are **two processes of their own** (`buildProcess` and
  `runProcess`). They never share their state, their output or their exit
  code, and stopping one of them does not touch the other.
* `Build > Run File` (`F5`) runs the document that is active, after saving it.
  `Build > Run` runs the project.
* The compiler is looked up next to the Studio binary, then in `$PEBBLE`,
  then in `PATH`; the resolution is cached.
* The Studio stops a running program when it is closed, and a program that
  ignores the request is killed after 1500 ms.

## 4. Building and testing

The commands below are the ones that were run for this handoff. They are the
verified commands; nothing else is guaranteed to work.

```sh
cd ~/Projects/Pebble

cmake -S . -B build -G Ninja -DPEBBLE_WARNINGS_AS_ERRORS=ON
cmake --build build

QT_QPA_PLATFORM=offscreen ./build/pebble_studio_tests
ctest --test-dir build --output-on-failure
```

* The build needs a C++20 compiler, a C11 compiler, CMake 3.16 or newer,
  Ninja (or Make) and Python 3 for the test suite.
* The Studio needs Qt 6 Widgets. The CMake file looks for `Qt6` and skips the
  Studio when it is not found, so the compiler and its tests still build
  without Qt.
* `PEBBLE_WARNINGS_AS_ERRORS` is `ON` by default. The flags are
  `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`. Do not weaken
  them.
* The Studio tests need a Qt platform plugin. Use `QT_QPA_PLATFORM=offscreen`
  on a machine without a display; CTest already sets that for the
  `pebble_studio` test.

### What the suite holds

`ctest` runs 22 tests. They are the unit tests of every stage of the
pipeline, the tests of the C frontend, the end-to-end tests that compile and
run programs, the parity tests that compare the two frontends and the test
of the Studio.

The Studio test binary holds **186 cases** in `tests/test_studio.cpp`. They
cover the document lifecycle, the project tree, the processes, the
diagnostics, the resolver of the compiler, the find and replace panel, the
keyboard editing, the *Edit* menu, the recent files, the run of the current
file, the highlighting, the gutter, the activity bar, the output pane, the
status bar and the visual system. The tests drive the real widgets; they do
not look at pictures, so they stay meaningful on a machine without a display.

## 5. Known limitations and unverified behaviour

Read this part before you promise anything to a user.

* **The window was not looked at by eye.** The visual pass was checked by
  reading the rendered pixels, by measuring the contrast of the colours and
  by running the window at 900x600, 1280x820 and 1600x1000. The agent that
  wrote this pass cannot see images and the machine has no display, so the
  layout was never inspected by a human. The checklist in section 9 is
  still open.
* **`pebble` must be on the machine to build or run.** The Studio shells out
  to the `pebble` command. It is looked up next to the Studio binary, in
  `$PEBBLE` and in `PATH`, but the Studio does not ship a compiler.
* **The output pane is bounded at 5 000 lines.** A run that writes more is
  truncated at the top. The bound can be changed
  (`MainWindow::setOutputMaximumBlockCount`), but it is not a setting of the
  Studio yet.
* **The dirty marker is text.** The `*` sits in the text of the tab, so the
  state of a document is still read back from its title. The polish that
  moves it into the close button is deferred (R11).
* **The tabs are neither movable nor elided.** A long name is cut off.
  Deferred (R11).
* **A program cannot read from the standard input.** The language has no
  `read` builtin yet, so there is nothing to feed. Deferred (R14).
* **The syntax colours cannot be changed.** They live in the theme of the
  Studio. Deferred (R20).
* **There is no welcome page, no help dialog and no settings dialog.**
  Deferred (R12, R21, R24).
* **The encoding is not shown in the status bar.** The reference shows
  `UTF-8`.
* **The asset pipeline, the engine and the multiplayer do not exist.**
  `docs/architecture/project-assets.md` is a proposal.
* **The reference editor cannot be run**, so no behaviour of it was observed
  directly. Every statement about it in `docs/studio/python-cpp-parity.md`
  comes from reading its source.

## 6. Fragile areas

* **The line number area.** The area is placed in the margin that the editor
  keeps on the left (`CodeEditor::updateLineNumberAreaGeometry`). It was
  placed at a negative coordinate before, so the numbers were never on the
  screen even though the gutter existed. Two tests read the rendered pixels
  to keep it that way; do not change the geometry without running them.
* **The comment syntax.** The editor toggles `//`, the syntax of the lexer
  (`src/lexer.cpp:286`). A `#` is not a comment in this language.
* **The style sheet and the palette.** They are two halves of one system: the
  palette gives the colours to the parts that Qt draws itself (the menus, the
  scrollbars, the tooltips), the style sheet gives them to the parts that
  name themselves (`#activitybar`, `#output`, `#state`). A new widget that is
  not named will not follow the theme.
* **The application style sheet.** `applyStudioTheme` sets the style sheet on
  the application *and* the window sets it on itself. The window copy reaches
  the widgets of the window, the application copy reaches the popups (the
  menus, the tooltips). Both hold the same rules.
* **The process handling.** A build and a run are two `QProcess` objects, and
  the state of the Studio follows them. A run that is started while a build
  runs is refused, and the Studio stops a running program when it is closed.
  The tests of the process handling use a stub program and are timing
  sensitive; do not weaken them to make them faster.
* **The settings.** The Studio stores `size`, `projectDir`, `openFiles`,
  `recentFiles`, `explorerShown`, `outputShown` and the state of the
  splitters under `QSettings("Pebble", "Pebble Studio")`. The tests use
  `QSettings("Pebble", "Pebble Studio Test")`, so they never touch the
  settings of a real installation.

## 7. The deferred items and why

These are in `docs/studio/parity-backlog.md` and are **not** a list that has
to be worked through.

| Item | What it is | Why it waits |
| --- | --- | --- |
| R11 | The polish of the tabs: movable, elided, a dirty close button. | The state of a document is readable in the tab text, and the tab bar of the Studio is already consistent with the rest of the interface. |
| R12 | The welcome page. | The *File* menu and the *Recent Files* submenu already lead into the work. |
| R14 | The input of a running program. | The language has no `read` builtin, so there is nothing to answer. |
| R20 | The dialog of the syntax colours. | The colours belong to the theme; a dialog needs a theme of its own. |
| R21 | The help and the about box. | The text of the reference describes the old dialect and would have to be written from `docs/language.md`. |
| R23 | The entry of the example project. | The repository holds no example project yet. |
| R24 | The general settings dialog. | The reference does not have one either. |

## 8. The next steps, in order

1. **Walk through the checklist of section 9** on a machine with a display
   and fix what looks wrong. This is the cheapest and the most valuable
   next step.
2. **Look at the run workflow of a real project.** The run of the current
   file, the build and the diagnostics were checked with a stub program and
   with the real compiler; they were not checked inside a real project
   directory with the manifest, the packages and the standard library.
3. **Decide about the tab polish (R11).** It is the smallest deferred item
   and the one that touches the editor the most.
4. **Give the language a `read` builtin** and then the input of a program
   (R14) becomes possible.
5. **Write the help dialog (R21)** from `docs/language.md` when the language
   has settled; it is a good first task for a new contributor.
6. **The asset pipeline** (`docs/architecture/project-assets.md`) is the next
   large piece of work, and it is a proposal, not a plan.

## 9. The manual verification checklist

Walk through this on a machine with a display. Every step is a thing a user
does; none of them is covered by a test.

* [ ] The window opens with the menu bar, the activity bar on the left, the
      tree of the project, the tabs, the editor and the output below.
* [ ] The window can be resized; the tree, the editor and the output keep
      their proportions and nothing is cut off at 900x600 and at 1600x1000.
* [ ] The two switches of the activity bar show and hide the tree and the
      output; the switch of the panel that is shown is marked.
* [ ] The panel of a hidden panel comes back the way it was left after a
      restart.
* [ ] A new document shows `Untitled.peb`; a file that is opened shows its
      name; a changed file shows a `*` in its tab and in the title of the
      window.
* [ ] The numbers of the lines are on the left of the code and the number of
      the line of the cursor is brighter.
* [ ] The keywords, the strings, the numbers, the comments, the literals and
      the calls are in different colours, and they follow an edit.
* [ ] `Tab`, `Shift+Tab`, `Enter` and `Ctrl+/` behave as described; the
      comment is `//`.
* [ ] `Ctrl+F` opens the find panel above the editor, `F3` and `Shift+F3`
      walk the matches, `Ctrl+H` adds the replacement row, `Esc` closes the
      panel and gives the focus back to the editor.
* [ ] `F5` runs the file that is being edited, the output of the program
      appears in the output pane, an error of the program is red and a
      warning is yellow, and the exit code is reported.
* [ ] A build that fails lists its errors in the diagnostics, the number of
      the entries is in the head of the list, and a double click on an entry
      opens the file on the reported line.
* [ ] The status bar shows the name of the document, the line and the column
      of the cursor and a state word, and the word is coloured when the
      document is changed.
* [ ] The buttons of the panels, the menu, the scrollbars and the tooltips
      are dark and readable; the text of the code is comfortable to read for
      a long session.
* [ ] The window keeps the size, the project and the open documents over a
      restart.

## 10. The documents

| Document | What it holds |
| --- | --- |
| `docs/language.md` | The syntax, the types and the rules of the language. The truth for every editor feature that needs language syntax. |
| `docs/packages.md` | The package system, the `pebble.toml` manifest, the standard library, the error behaviour. |
| `docs/architecture/project-assets.md` | The proposal for the asset pipeline. Design only. |
| `docs/studio/python-cpp-parity.md` | The audit of the reference against the native Studio, finding by finding, with the state of every one of them. |
| `docs/studio/parity-backlog.md` | The roadmap: what is done, what is deferred and why, and the dependencies between the items. |
| `README.md` | The build, the test and the command line of the compiler. |
