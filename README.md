# Pebble

Pebble is a small native language for building programs and games. The
compiler translates a Pebble program into C++20 or C11 and builds it with the
platform compiler, so a Pebble program becomes a native executable without a
virtual machine.

The project ships two compiler frontends:

| Frontend | Sources | Generated code |
| --- | --- | --- |
| `pebble` | `include/pebble/`, `src/` | C++20 |
| `pebble_native` | `include/pebble_c/`, `src/c/` | C11 |

Both frontends implement the same language, the same package system and the
same command line. The C11 frontend is migrated incrementally alongside the
C++ frontend and the test suite compares the two on every change.

The platform is planned in four layers: the language and its standard
library, the engine for games and 3D programs, the studio that edits and
runs them, and the native platform they rest on. The
[architecture document](docs/architecture/OVERVIEW.md) draws the line between
them and says what stands behind each line today.

## Building

```sh
cmake -S . -B build
cmake --build build
```

The build needs a C++20 compiler, a C11 compiler and Python 3 for the test
suite. The generated programs are compiled with `c++` (or `$CXX`) and `cc` (or
`$CC`) at run time.

## Testing

```sh
cd build
ctest --output-on-failure
```

The suite contains unit tests for every stage of the pipeline, end to end
tests that compile and run programs, and parity tests that compare the C++
and the C frontend on the same input.

## Command line

```
pebble lex <file.peb>        print the tokens of a program
pebble parse <file.peb>      print the syntax tree of a program
pebble check [<file.peb>]    type check a file or the project in the working directory
pebble build [<file.peb>]    build a native executable
pebble run [<file.peb>]      build and run a program
pebble init [<name>]         create a new package
pebble add <path>            add a path dependency to the manifest
pebble remove <package>      remove a dependency from the manifest
pebble install               resolve the dependencies and write the lockfile
pebble update                resolve the dependencies again and refresh the lockfile
pebble tree                  print the dependency tree
pebble package               stage a distributable copy of the package
```

`pebble_native` understands the same commands.

## A first program

```pebble
import package math

fn main() {
    let radius: Float = 2.0
    let area = math.pi() * radius * radius

    print("The area is", area)
}
```

```sh
pebble run main.peb
```

A statement is ended by a line break. The semicolon is an optional separator
between two statements and never a mandatory terminator, so a program never
needs one. A program that is written with semicolons keeps working.

An expression may be written over several lines. It continues when the line
ends with an operator, when it stands inside a parenthesis that is not closed
yet or when the next line starts with the `.` or the `:` of a member. The
rules are described in [docs/language.md](docs/language.md#line-breaks).

## The standard library

Five packages are built into the compiler and are available in every program.

```pebble
import package Log
import package filesystem
import package math
import package string
import package time

fn main() {
    print(string.upper("pebble"), math.round(2.5), time.delta())
    print(filesystem.exists("README.md"))
    Log:Print("The program ends")
}
```

* `math` holds the random source and the usual number functions.
* `string` holds the functions that work on a string, such as `length`,
  `substring`, `upper` and `replace`.
* `time` holds the clock of the program: `now`, `ticks`, `delta`, `sleep`
  and `unix`.
* `filesystem` holds the files of the program: `read`, `write`, `exists`,
  `list` and `mkdir`.
* `Log` holds the messages of the program: `Print` writes to the standard
  output, and `Warning` and `Error` write to the standard error output.

The full list is in [docs/packages.md](docs/packages.md).

## Documentation

* [docs/language.md](docs/language.md) describes the syntax, the types and
  the rules of the language.
* [docs/packages.md](docs/packages.md) describes the package system, the
  `pebble.toml` manifest, the standard library and the error behaviour.
* [docs/architecture/OVERVIEW.md](docs/architecture/OVERVIEW.md) describes the
  four layers of the platform: the language, the engine, the studio and the
  native platform, and what stands behind each line today.
* [docs/architecture/ENGINE_VS_STUDIO.md](docs/architecture/ENGINE_VS_STUDIO.md)
  describes the boundary between the engine and the studio.
* [docs/architecture/PACKAGE_PARITY.md](docs/architecture/PACKAGE_PARITY.md)
  describes which package is in which frontend, and the backlog that closes
  the gap.
* [examples/README.md](examples/README.md) lists the examples that exist and
  the examples that are planned.
* [docs/studio/branding.md](docs/studio/branding.md) describes the icon and
  the wallpaper of Pebble Studio, the installation and the launcher of
  VS Codium.
