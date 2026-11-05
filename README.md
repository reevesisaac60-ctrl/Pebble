# Pebble

**Pebble is a native programming language and development environment for building standalone programs.**

The project includes two compiler frontends, a built-in package system, native executable generation, and **Pebble Studio** — a desktop development environment designed specifically for Pebble.

Pebble aims to provide a complete programming workflow, from writing and checking source code to building and running native applications.

**Latest release: [v0.3.3](https://github.com/reevesisaac60-ctrl/Pebble/releases/tag/v0.3.3)**

## Features

- **Native compilation** — build Pebble programs into standalone executables.
- **Two compiler frontends** — `pebble` and `pebble_native`.
- **Pebble Studio** — a dedicated native desktop IDE.
- **Standard library** — built-in packages for common programming tasks.
- **Package management** — project manifests, dependencies, lockfiles, and package inspection.
- **OpenGL support** — create graphics applications using OpenGL functionality.
- **Language tooling** — lexical analysis, parsing, and type checking.
- **Examples and resources** — explore included programs and project assets.

## Project components

| Component | Description |
|---|---|
| `pebble` | Main Pebble compiler and command-line frontend. |
| `pebble_native` | C-based compiler frontend implementing the same language interface. |
| `pebble_studio` | Native desktop development environment. |

Both compiler frontends support the same language and package-management workflow.

## Installation

### Windows

Pebble provides a Windows x64 installer.

**[Download Pebble v0.3.3 for Windows](https://github.com/reevesisaac60-ctrl/Pebble/releases/download/v0.3.3/Pebble-0.3.3-Windows-x64-Setup.exe)**

The installer includes Pebble Studio and the compiler executables.

**Windows development requirements**

To compile and run Pebble programs on Windows, an external native development toolchain is currently required.

Install the MSYS2 UCRT64 toolchain and GLFW, and ensure the UCRT64 `bin` directory is available through your system `PATH`.

Pebble relies on the host system's native compiler and development libraries when building programs. The installer does not bundle the complete development toolchain.

### Linux Mint and Ubuntu

Prebuilt Debian packages are provided for:

- Linux Mint 22.3
- Ubuntu 24.04 LTS

**[Download Pebble v0.3.3 for Linux (amd64)](https://github.com/reevesisaac60-ctrl/Pebble/releases/download/v0.3.3/pebble_0.3.3_amd64.deb)**

Install the downloaded package using:

```bash
sudo apt install ./pebble_0.3.3_amd64.deb
```

APT automatically resolves the package's required runtime dependencies.

### Arch Linux

An Arch Linux package for v0.3.3 will be provided on the [GitHub Releases page](https://github.com/reevesisaac60-ctrl/Pebble/releases/tag/v0.3.3).

### Other Linux distributions

Building from source requires the appropriate C/C++ compiler, CMake, Ninja, Qt 6 development libraries, and the required graphics dependencies.


Platform-specific packaging and installation instructions will be expanded as support develops.

## Command-line interface

Pebble provides commands for working with individual source files and complete projects.

| Command | Description |
|---|---|
| `pebble lex <file.peb>` | Display the tokens produced by the lexer. |
| `pebble parse <file.peb>` | Display the parsed syntax tree. |
| `pebble check [<file.peb>]` | Type-check a file or the current project. |
| `pebble build [<file.peb>]` | Build a native executable. |
| `pebble run [<file.peb>]` | Build and run a program. |
| `pebble init [<name>]` | Create a new Pebble package. |
| `pebble add <path>` | Add a path dependency. |
| `pebble remove <package>` | Remove a package dependency. |
| `pebble install` | Resolve dependencies and generate a lockfile. |
| `pebble update` | Refresh dependency resolution and the lockfile. |
| `pebble tree` | Display the dependency tree. |
| `pebble package` | Stage a distributable copy of a package. |

`pebble_native` supports the same command-line operations.

For the complete list of options, run:

```sh
pebble --help
```

Or:

```sh
pebble_native --help
```

## Your first Pebble program

Create a file named `main.peb`:

```pebble
import package math

fn main() {
    let radius: Float = 2.0
    let area = math.pi() * radius * radius

    print("The area is", area)
}
```

Run the program:

```sh
pebble run main.peb
```

Pebble builds and executes the program using the native compilation toolchain.

## Standard library

Pebble includes built-in packages for commonly used functionality.

| Package | Description |
|---|---|
| `math` | Mathematical functionality. |
| `string` | String manipulation and related operations. |
| `time` | Time-related functionality. |
| `filesystem` | Filesystem operations. |
| `Log` | Logging functionality. |

These packages are available to Pebble programs without installing external dependencies.

## Pebble Studio

Pebble Studio is the native desktop IDE included with the Pebble toolchain.

It provides an integrated environment for developing Pebble applications, bringing together source editing and language tooling in one application.

Studio is designed to make working with Pebble projects easier without requiring a separate editor.

## Building from source

Clone the repository:

```sh
git clone https://github.com/reevesisaac60-ctrl/Pebble.git
cd Pebble
```

### Build requirements

- CMake 3.25 or newer
- Ninja
- A compatible C and C++ compiler
- Qt 6 development libraries
- GLFW and the required OpenGL development libraries

### Configure and build

```sh
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build --parallel
```

The project includes the `pebble`, `pebble_native`, and `pebble_studio` build targets.

Platform-specific dependencies and packaging requirements may vary.

## OpenGL

Pebble includes OpenGL functionality for graphics programming.

The repository provides examples demonstrating graphics applications, shader usage, and geometry data.

Explore `examples/opengl/` for the included OpenGL programs and resources.

## Examples

The repository includes example Pebble programs demonstrating language features, built-in packages, and graphics functionality.

Explore the `examples/` directory to see how Pebble programs are structured and how the available functionality can be used.

## Project status

**Pebble is actively developed.**

Version `0.3.3` is an early release of the language and its development environment. Features, APIs, tooling, and platform support may change as the project evolves.

Contributions, bug reports, and suggestions are welcome.

## License

See the [`LICENSE`](LICENSE) file for licensing information.

## Links

- [GitHub repository](https://github.com/reevesisaac60-ctrl/Pebble)
- [Latest release](https://github.com/reevesisaac60-ctrl/Pebble/releases/latest)
- [v0.3.3 release notes](https://github.com/reevesisaac60-ctrl/Pebble/releases/tag/v0.3.3)
- [Report an issue](https://github.com/reevesisaac60-ctrl/Pebble/issues)
