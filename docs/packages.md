# Packages

Pebble programs are built from packages. A package is a named collection of
functions that other programs can import.

```pebble
import package math

fn main() {
    print(math.abs(-3));
}
```

## The standard library

The `math`, the `string`, the `time`, the `filesystem` and the `Log` package
are built into the compiler and are available in every program. They are the
same packages in the C++ frontend and in the C frontend.

### math

| Function | Signature | Description |
| --- | --- | --- |
| `math.randint(minimum, maximum)` | `(Int, Int) -> Int` | A random integer between the two bounds, both included. |
| `math.random()` | `() -> Float` | A random number between `0.0` and `1.0`. |
| `math.randomseed(seed)` | `(Int) -> Void` | Makes the random sequence reproducible. |
| `math.abs(value)` | `(Int) -> Int` | The absolute value. |
| `math.min(left, right)` | `(Int, Int) -> Int` | The smaller of the two numbers. |
| `math.max(left, right)` | `(Int, Int) -> Int` | The larger of the two numbers. |
| `math.sqrt(value)` | `(Float) -> Float` | The square root. |
| `math.floor(value)` | `(Float) -> Float` | The largest integer that is not greater than the value. |
| `math.ceil(value)` | `(Float) -> Float` | The smallest integer that is not smaller than the value. |
| `math.round(value)` | `(Float) -> Float` | The value rounded to the nearest integer, halves away from zero. |
| `math.pi()` | `() -> Float` | The constant pi. |

The random generator is seeded from the environment, so two runs of the same
program produce different numbers. A program that calls `math.randomseed`
before it draws numbers produces the same sequence on every run and in every
frontend.

```pebble
import package math

fn main() {
    math.randomseed(12345);
    print(math.randint(1, 100));
}
```

Invalid mathematical operations stop the program with a message on the
standard error output:

| Operation | Message |
| --- | --- |
| `math.randint(10, 1)` | `Pebble runtime error: math.randint requires min <= max (min=10, max=1)` |
| `math.abs(-9223372036854775808)` | `Pebble runtime error: math.abs is undefined for -9223372036854775808` |
| `math.sqrt(-1.0)` | `Pebble runtime error: math.sqrt is undefined for negative values` |

### string

A string is a sequence of bytes. The functions of the package work on the
bytes of a string, so the length of a string is the number of bytes in it.

| Function | Signature | Description |
| --- | --- | --- |
| `string.length(text)` | `(String) -> Int` | The number of bytes in the string. |
| `string.substring(text, start, count)` | `(String, Int, Int) -> String` | `count` bytes from `start`. The result is cut off at the end of the string. |
| `string.left(text, count)` | `(String, Int) -> String` | The first `count` bytes. |
| `string.right(text, count)` | `(String, Int) -> String` | The last `count` bytes. |
| `string.upper(text)` | `(String) -> String` | The string in upper case. |
| `string.lower(text)` | `(String) -> String` | The string in lower case. |
| `string.trim(text)` | `(String) -> String` | The string without the space at both ends. |
| `string.contains(text, needle)` | `(String, String) -> Bool` | Whether the string holds the other string. |
| `string.startswith(text, prefix)` | `(String, String) -> Bool` | Whether the string starts with the other string. |
| `string.endswith(text, suffix)` | `(String, String) -> Bool` | Whether the string ends with the other string. |
| `string.indexof(text, needle)` | `(String, String) -> Int` | The place of the first occurrence, or `-1`. |
| `string.replace(text, needle, replacement)` | `(String, String, String) -> String` | The string with every occurrence replaced. |
| `string.repeat(text, count)` | `(String, Int) -> String` | The string repeated `count` times. |
| `string.toint(text)` | `(String) -> Int` | The number in the string. |
| `string.tofloat(text)` | `(String) -> Float` | The number in the string. |

```pebble
import package string

fn main() {
    let name = string.trim("  Pebble  ");
    print(string.upper(name), string.length(name));
}
```

The functions of the package report a problem and stop the program when they
cannot do what they are asked:

| Operation | Message |
| --- | --- |
| `string.substring("abc", 5, 1)` | `Pebble runtime error: string.substring is out of range (start=5, length=3)` |
| `string.substring("abc", 0, -1)` | `Pebble runtime error: string.substring needs a non negative count (count=-1)` |
| `string.repeat("a", -1)` | `Pebble runtime error: string.repeat needs a non negative count (count=-1)` |
| `string.toint("abc")` | `Pebble runtime error: string.toint cannot parse 'abc'` |
| `string.tofloat("abc")` | `Pebble runtime error: string.tofloat cannot parse 'abc'` |

### time

The package reads the clock of the program. The clock is monotonic, so it
never goes backwards, and it starts when the program starts.

| Function | Signature | Description |
| --- | --- | --- |
| `time.now()` | `() -> Float` | The seconds the program has been running. |
| `time.ticks()` | `() -> Int` | The nanoseconds the program has been running. |
| `time.delta()` | `() -> Float` | The seconds since the last call of the function. |
| `time.sleep(seconds)` | `(Float) -> Void` | Waits for the given number of seconds. The program waits at least that long. |
| `time.unix()` | `() -> Int` | The seconds since the Unix epoch, the first of January 1970. |

```pebble
import package time

fn main() {
    let start = time.now()
    time.sleep(0.5)
    print(time.now() - start)
}
```

The delta of a program is the length of a frame in a loop. The first call of
`time.delta()` returns zero, because it has no frame before it. Every later
call returns the time since the call before it.

```pebble
import package time

fn main() {
    var frame = 0
    while frame < 3 {
        time.sleep(0.01)
        print(time.delta())
        frame = frame + 1
    }
}
```

A sleep that is given a negative duration reports a problem and stops the
program:

| Operation | Message |
| --- | --- |
| `time.sleep(-1.0)` | `Pebble runtime error: time.sleep needs a non negative duration (duration=-1)` |

### filesystem

The package reads and writes the files of the program. It is the same package
in both frontends, and the behaviour of a call is the same in both of them.

| Function | Signature | Description |
| --- | --- | --- |
| `filesystem.read(path)` | `(String) -> String` | The whole content of a file, as bytes. |
| `filesystem.write(path, contents)` | `(String, String) -> Void` | Writes the bytes into the file. The file is created when it is not there, and its content is replaced when it is. |
| `filesystem.exists(path)` | `(String) -> Bool` | Whether a file or a directory is at the path. |
| `filesystem.list(path)` | `(String) -> String` | The names of the entries of a directory. |
| `filesystem.mkdir(path)` | `(String) -> Void` | Creates a directory with the directories above it. |

```pebble
import package filesystem

fn main() {
    filesystem.mkdir("notes")
    filesystem.write("notes/note.txt", "A note from Pebble.\n")
    print(filesystem.exists("notes/note.txt"))
    print(filesystem.read("notes/note.txt"))
    print(filesystem.list("notes"))
}
```

The rules of the package are the same in both frontends:

* A path is a sequence of bytes in the encoding of the operating system, which
  is UTF-8 on Linux and on macOS. A relative path is resolved against the
  directory the program runs in. A path may name a file or a directory.
* A file is a sequence of bytes. The package does not translate the encoding
  of a file, so the bytes that are written are the bytes that are read.
* `filesystem.write` does not create the directories of the path. The
  directory of the file has to exist.
* `filesystem.list` returns the names of the entries, one name per line, and
  every line ends with a newline character. The names come in the order of
  the alphabet, so two programs that list the same directory print the same
  lines. The names of the directory itself are not part of the list.
* `filesystem.mkdir` creates the directory and every directory above it that
  is not there yet. A path that is there already is reported as a problem.
* `filesystem.exists` never fails. It reports whether something is at the
  path.

A path that cannot be used stops the program with a message on the standard
error output:

| Operation | Message |
| --- | --- |
| `filesystem.read("missing.txt")` | `Pebble runtime error: filesystem.read cannot read 'missing.txt'` |
| `filesystem.read("notes")` (a directory) | `Pebble runtime error: filesystem.read cannot read 'notes'` |
| `filesystem.write("missing/note.txt", "x")` | `Pebble runtime error: filesystem.write cannot write 'missing/note.txt'` |
| `filesystem.list("missing")` | `Pebble runtime error: filesystem.list cannot list 'missing'` |
| `filesystem.mkdir("notes")` (it is there) | `Pebble runtime error: filesystem.mkdir cannot create 'notes'` |
| `filesystem.exists("")` | `Pebble runtime error: filesystem.exists needs a path` |

An empty path is not a path, so every function of the package reports it the
same way.

### Log

The package writes the messages of the program. It needs no window, so it
belongs to the standard library, and it is the same package in both
frontends.

| Function | Signature | Description |
| --- | --- | --- |
| `Log:Print(message)` | `(String) -> Void` | Writes the message on the standard output. |
| `Log:Warning(message)` | `(String) -> Void` | Writes the message on the standard error output, marked as a warning. |
| `Log:Error(message)` | `(String) -> Void` | Writes the message on the standard error output, marked as an error. |

```pebble
import package Log

fn main() {
    Log:Print("The program starts")
    Log:Warning("The path is unusual")
    Log:Error("The file cannot be opened")
}
```

The rules of the package are the same in both frontends:

* The name of the package and the names of its functions keep their case.
  `Log:print` is not `Log:Print`, and the name of the package is `Log` and
  not `log`.
* `Log:Print` writes to the standard output. `Log:Warning` and `Log:Error`
  write to the standard error output, so that a message of the error output
  cannot be mistaken for the output of the program.
* A warning is marked with `Pebble warning: ` and an error with
  `Pebble error: `. The mark and the message are on one line.
* Every message ends its line. A message that is empty writes an empty line.
* A message is written as it is. It is never read as a format string, so a
  message that holds a `%` is printed as it was written.
* The message has to be a string. A number or a boolean is refused by the
  type checker.

## Local packages

A package that is used during development is a file that sits next to the
file that imports it, or in a `packages` directory next to it.

```
project/
  main.peb          import package tools
  tools.peb         the package
```

```
project/
  main.peb          import package tools
  packages/
    tools.peb       the package
```

A package is imported with its name and its functions are called with that
name in front of them.

```pebble
// tools.peb
export fn double(value: Int) -> Int {
    return value * 2;
}
```

```pebble
// main.peb
import package tools

fn main() {
    print(tools.double(21));
}
```

A package may import other packages. A dependency cycle is reported and the
program is rejected:

```
error: Circular dependency detected: alpha -> beta -> alpha.
```

### Visibility

Only functions that are marked with `export` can be used from another file.
A function without `export` is private to the package that declares it, but
it is still compiled and can be used by the other functions of that package.

```pebble
// tools.peb
fn helper(value: Int) -> Int {
    return value + 1;
}

export fn double(value: Int) -> Int {
    return helper(value) * 2;
}
```

```pebble
// main.peb
import package tools

fn main() {
    print(tools.helper(1));   // error: Package 'tools' has no function 'helper'.
}
```

Inside a package an unqualified call resolves to the function of the package
before it resolves to a function of the program:

```pebble
// tools.peb
fn helper(value: Int) -> Int {
    return value + 1;
}

export fn compute(value: Int) -> Int {
    return helper(value);   // calls the helper of this package
}
```

## Projects and the manifest

A directory with a `pebble.toml` manifest is a project. The manifest
describes the package and the packages it depends on.

```toml
[package]
name = "math_tools"
version = "0.1.0"
description = "Reusable helpers"
license = "MIT"
authors = ["Ada Lovelace"]
pebble = ">=0.3.0"

[dependencies]
vectors = { path = "../vectors" }
```

| Key | Meaning |
| --- | --- |
| `name` | The name of the package. It is required and has to be a valid identifier. |
| `version` | The version of the package. It is required and has to be a semantic version such as `1.2.3`. |
| `description` | A short description of the package. |
| `license` | The licence of the package. |
| `authors` | A list of authors. |
| `pebble` | The version of the toolchain the package needs, for example `>=0.3.0`. |
| `dependencies` | The packages this package depends on. |

A dependency is a package that lives in another directory. The `path` is
relative to the directory of the manifest and the optional `version` is a
version constraint. The supported constraints are `1.2.3`, `=1.2.3`,
`>=1.2.3`, `>1.2.3`, `<=1.2.3`, `<1.2.3` and `^1.2.3`.

A text in the manifest is written between quotes. A quote inside a text is
written as `\"` and a backslash as `\\`, so a path may hold any character that
a file name may hold.

The entry point of a project is `src/main.peb` or `main.peb`. The package
itself is the file `<name>.peb` in the directory of the manifest.

## The package commands

```sh
pebble init math_tools
```

Creates a new package with a manifest, the package file and an example
program.

```sh
pebble add ../vectors
```

Adds a package from another directory to the manifest. The name of the
dependency is taken from the manifest of that package.

```sh
pebble remove vectors
```

Removes a dependency from the manifest.

```sh
pebble install
```

Reads the manifest, resolves every path dependency, follows the
dependencies of the dependencies and writes `pebble.lock`. A dependency
that is missing, a version that does not match the constraint and a
dependency cycle are all reported.

```sh
pebble update
```

Resolves the dependencies from the manifest again and rewrites `pebble.lock`.
The command behaves like `pebble install` and always resolves the
dependencies from the manifest, so it picks up a new version of a path
dependency.

```sh
pebble tree
```

Prints the package and the packages it depends on.

```sh
pebble package
```

Validates the manifest and stages a copy of the package in
`dist/<name>-<version>`.

```sh
pebble build
pebble run
```

Build the project in the working directory, using the resolved dependencies.
A file argument builds a single file instead of the project.

## The lockfile

`pebble install` writes a `pebble.lock` next to the manifest. It records the
exact version of every resolved package, so a build can be repeated.

```
# Pebble lockfile. Generated by "pebble install", do not edit it by hand.
lockfile_version = 1

[[package]]
name = "vectors"
version = "0.2.0"
path = "../vectors"
```

## Error behaviour

The compiler reports every problem with the file, the line and the column of
the offending code.

| Situation | Message |
| --- | --- |
| The package cannot be found | `Cannot find package 'tools'. Looked for 'tools.peb' and 'packages/tools.peb'.` |
| The package has no such function | `Package 'tools' has no function 'helper'.` |
| The package is imported twice | `Package 'tools' is imported more than once.` |
| The package is used without an import | `Unknown package 'tools'.` |
| The package depends on itself | `Circular dependency detected: alpha -> beta -> alpha.` |
| The manifest is broken | `The manifest needs a package name.` |
| The dependency cannot be resolved | `The dependency 'tools' cannot be resolved from '../tools'.` |
| The version does not match | `The dependency 'tools' requires version '>=1.0.0' but 0.2.0 is installed.` |
| The dependency is needed twice | `The dependency 'tools' is required from two different locations.` |
