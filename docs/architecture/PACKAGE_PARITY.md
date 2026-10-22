# The Parity of the Packages

Pebble has two frontends. The C++ frontend (`pebble`) and the C frontend
(`pebble_native`) implement the same language, the same package system and
the same command line, and the test suite compares the two on every change.
This document records which packages each frontend holds, what the two agree
on, and what is still open.

The audit behind this document was made on 2026-10-21 against the sources of
the repository. The registries are `src/package.cpp` for the C++ frontend and
`src/c/package.c` for the C frontend. The behaviour was compared with the
parity suite, `tests/test_package_runtime_parity.py`, which builds a program
with both frontends and compares the output of the two.

---

## What each frontend holds

| Package | C++ frontend | C frontend | Kind |
| --- | --- | --- | --- |
| `math` | 11 functions | 11 functions | standard library |
| `string` | 15 functions | 15 functions | standard library |
| `time` | 5 functions | 5 functions | standard library |
| `filesystem` | 5 functions | 5 functions | standard library |
| `Log` | 3 functions | 3 functions | standard library |
| `Math` | 12 functions | — | engine |
| `Window` | 5 functions | — | engine |
| `Input` | 4 functions | — | engine |

The five packages of the standard library are in both frontends. The three
packages of the engine are in the C++ frontend only.

## What the two agree on

For the five packages of the standard library the audit found:

* **The same names.** Every function of `math`, `string`, `time`,
  `filesystem` and `Log` is in both frontends, and no frontends has a
  function that the other does not have.
* **The same signatures.** The parameter types and the return types of every
  function are the same. The comparison of the two registries found no
  difference.
* **The same error messages.** The wording of the runtime errors is the
  same, down to the name of the function and the value that is reported:
  `math.sqrt is undefined for negative values`,
  `string.substring is out of range (start=5, length=3)`,
  `time.sleep needs a non negative duration (duration=-1)` and
  `filesystem.read cannot read 'missing.txt'`. The `Log` package writes
  `Pebble warning: ` before a warning and `Pebble error: ` before an error
  in both frontends.
* **The same behaviour.** The parity suite runs 82 programs through both
  frontends and compares the output and the diagnostics of the two. Every
  case passes, including the whole of the `filesystem` package and the
  whole of the `Log` package.

## Where the two differ on purpose

Two packages of the standard library cannot print the same numbers, and
this is by design:

* **The random source.** Both frontends use the same generator, but the seed
  comes from the clock and from the address of the state, so two runs
  produce different numbers. A program that calls `math.randomseed` before
  it draws numbers prints the same sequence in both frontends and on every
  run.
* **The clock.** The readings of `time.now()` and `time.ticks()` are the
  time of the machine and not the same in two programs. The parity suite
  compares the bounds of the readings and not their values.

## The limits of the platform

| Limit | C++ frontend | C frontend |
| --- | --- | --- |
| The window and the input | GLFW, OpenGL and X11 are linked into the program, so a window needs a display | the program links `lm` only, so it cannot open a window |
| The clock and the files | the clock and the file system of the system through `<chrono>`, `<thread>` and `<filesystem>` | the clock and the file system of the system through POSIX: `clock_gettime`, `nanosleep`, `opendir` and `mkdir` |
| The memory of a string | a `std::string` is released with the value that holds it | a `char *` of the runtime is never released, because the language has no way to release it |

The C frontend needs a POSIX system for the `time` and the `filesystem`
package. The generated program asks for it with
`_POSIX_C_SOURCE 200809L` before its first header.

---

## The rule

A package that belongs to the **language and the standard library** is
portable: it is registered in both frontends, it has the same names, the
same signatures, the same behaviour and the same error messages, and it is
covered by the parity suite.

A package that belongs to the **engine** needs a window, a device or a
scene. It is in the C++ frontend, which links the window system and the
graphics API. The C frontend is the frontend for the programs that do not
need a window.

A package of the standard library may not depend on the window, on the
graphics API or on the studio. A package of the engine may not depend on
the studio either. See [overview.md](overview.md) for the layers and
[engine_vs_studio.md](engine_vs_studio.md) for the boundary.

---

## The backlog

The items are ordered by how much they hold up the rest of the work.

### P1 — The C++ frontend cannot join a string that was bound to a literal

*Found on 2026-10-21.*

The type checker accepts the program and [the language](../language.md)
promises that `+` joins two strings, but the C++ code generator gives a
string literal the type `const char *`, so the `+` of the generated code is
not a C++ `operator+` and the program does not compile:

```pebble
fn main() {
    let text = "a"
    print(text + "b")   // error in the C++ frontend
}
```

The C frontend is right: it emits `pebble_string_concat` and prints `ab`.
A string that comes from a package is a `std::string` in the C++ frontend
and joins with a literal without a problem, so the defect only shows where
a literal is bound to a variable.

*Depends on:* nothing.
*Acceptance criteria:*
1. `print("a" + "b")` and `let text = "a"; print(text + "b")` compile and
   run in both frontends and print `ab`.
2. Both programs are cases of the parity suite, so the two frontends have to
   print the same text.
3. The C++ frontend does not copy a string of the `string` package to join
   it with a literal.

### P2 — The `Log` package is only in the C++ frontend — **closed**

*Found on 2026-10-21, closed on 2026-10-21.*

`Log` prints to the standard output and to the standard error output. It
needs no window, so it belongs to the standard library and it has to be in
both frontends.

*Depends on:* nothing.
*Acceptance criteria:*
1. The package is registered in both frontends with the names `Print`,
   `Warning` and `Error` and with the same signatures.
2. `Print` writes to the standard output and `Warning` and `Error` write to
   the standard error output in both frontends, with the same prefixes.
3. The three functions are cases of the parity suite.

*What was done:*

1. The C registry (`src/c/package.c`) holds the package `Log` with the three
   functions, each with one `String` parameter and a `Void` return. The
   names keep their case, so `Log:print` and `import package log` are
   refused.
2. The runtime of the C frontend writes the message of `Print` to the
   standard output and the message of `Warning` and `Error` to the standard
   error output, with the same marks as the C++ frontend. The standard
   output is flushed before a message of the standard error output is
   written, so that the two streams keep their order when they are written
   into the same file. A message is never read as a format string, and a
   message that is not there stands in for the text `null`.
3. The three functions are cases of the parity suite, and
   `tests/test_log_runtime.py` builds and runs programs of the C frontend
   and looks at the two streams.

### P3 — There are two math packages, and one of them is only in the C++ frontend

The C++ frontend holds `math` with 11 functions and `Math` with 12. The
extra functions are `Radians`, `Sin`, `Cos` and `Tan`, and the examples of
the engine use them. The C frontend holds only `math` with 11 functions.

*Depends on:* nothing.
*Acceptance criteria:*
1. There is one math package in both frontends. The decision is written in
   [packages.md](../packages.md) and the examples are updated with it.
2. Every function of the package is a case of the parity suite, with the
   same result in both frontends.

### P4 — The engine packages are only in the C++ frontend, and this is not written down

`Window` and `Input` need GLFW, a display and X11. The C frontend links
`lm` only, so it cannot hold them. That is the right split, but the two
frontends are supposed to implement the same language, and the C frontend
does not say that a package belongs to the other frontend.

*Depends on:* P5.
*Acceptance criteria:*
1. The rule is written in this document and in [packages.md](../packages.md):
   a package of the engine is in the C++ frontend.
2. The C frontend reports a package of the engine with a message that names
   the frontend that holds it, and the message is a case of the parity
   suite.

### P5 — The C frontend looks for an engine package on the disk

The C frontend answers `import package Window` with
`Cannot find package 'Window'. Looked for 'Window.peb' and
'packages/Window.peb'.` The message points at a local package, which is not
what went wrong: the package is a package of the engine, and it is in the
other frontend.

*Depends on:* nothing.
*Acceptance criteria:*
1. The C frontend answers a package of the engine with a message that names
   the frontend that holds the package, and the message is a case of the
   parity suite.

### P6 — The C frontend never releases the strings of the runtime

The runtime of the C frontend hands a `char *` to the program, and the
language has no way to release it. A program that reads a file in a loop
grows without bound. The C++ frontend returns a `std::string`, which is
released with the value that holds it.

*Depends on:* nothing.
*Acceptance criteria:*
1. The behaviour is written down in [packages.md](../packages.md).
2. Either the language grows a way to release memory, or the runtime of the
   C frontend holds the strings it hands out and releases them when the
   program ends. A program that reads the same file many times does not
   grow without bound, and there is a test that shows it.

### P7 — The collections, the random and the json packages do not exist

The platform needs a package for lists and maps, one for random values with
a seed of its own, and one for json. They belong to the standard library,
so they have to be in both frontends.

*Depends on:* nothing, but the type system has to grow first: the language
has no list and no map today.
*Acceptance criteria:*
1. Each package is registered in both frontends, documented in
   [packages.md](../packages.md) and covered by the parity suite.
2. The type system holds the types that the packages need, and the two
   frontends agree on them.

### P8 — The engine does not exist

The renderer, the shaders, the textures, the materials, the lights, the
shadows, the cameras, the meshes, the model importer, the scene, the
entities, the physics, the audio and the scripting are all missing. The
plan is in [overview.md](overview.md) and in
[project-assets.md](project-assets.md).

*Depends on:* the boundary of [engine_vs_studio.md](engine_vs_studio.md).
*Acceptance criteria:*
1. The engine is a library that a program and the studio both use.
2. It is reached through packages, and it does not depend on the studio.

---

## How the audit was made

```sh
# the registries of the two frontends
src/package.cpp
src/c/package.c

# the behaviour of the packages, compared between the two frontends
python3 tests/test_package_runtime_parity.py \
    build/pebble build/pebble_native

# the behaviour of the Log package of the C frontend, on its own
python3 tests/test_log_runtime.py build/pebble_native
```

The suite builds a program with both frontends, runs it and compares the
output and the diagnostics of the two. The cases that cannot be equal, the
numbers of the random source and the readings of the clock, are compared by
their bounds.
