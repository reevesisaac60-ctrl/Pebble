# Pebble

Pebble is a native programming language and development environment for building programs. Its compiler translates Pebble source code into **C++20 or C11**, then uses the platform's native compiler to produce standalone executables.

The project includes two compiler frontends and Pebble Studio, a native desktop IDE.

| Component | Sources | Generated code |
|---|---|---|
| `pebble` | `include/pebble/`, `src/` | C++20 |
| `pebble_native` | `include/pebble_c/`, `src/c/` | C11 |

Both compiler frontends implement the same language, package system and command-line interface.

## Features

- Native compilation through C++20 and C11.
- Two compiler frontends: `pebble` and `pebble_native`.
- Pebble Studio desktop development environment.
- Built-in standard library packages.
- Project manifests and dependency management.
- Native executable generation.
- OpenGL support.
- Included examples and project resources.

## Command-line interface

The `pebble` command provides the following operations:

| Command | Description |
|---|---|
| `pebble lex <file.peb>` | Print the tokens of a program. |
| `pebble parse <file.peb>` | Print the syntax tree of a program. |
| `pebble check [<file.peb>]` | Type-check a file or the project in the working directory. |
| `pebble build [<file.peb>]` | Build a native executable. |
| `pebble run [<file.peb>]` | Build and run a program. |
| `pebble init [<name>]` | Create a new package. |
| `pebble add <path>` | Add a path dependency to the manifest. |
| `pebble remove <package>` | Remove a dependency from the manifest. |
| `pebble install` | Resolve dependencies and write the lockfile. |
| `pebble update` | Resolve dependencies again and refresh the lockfile. |
| `pebble tree` | Print the dependency tree. |
| `pebble package` | Stage a distributable copy of the package. |

`pebble_native` supports the same commands.

## A first program

Create a file named `main.peb`:

```pebble
import package math

fn main() {
    let radius: Float = 2.0
    let area = math.pi() * radius * radius

    print("The area is", area)
}
```

Run it with:

```sh
pebble run main.peb
```

Pebble compiles the source into a native executable using the platform's C++ or C compiler.

## Standard library

Pebble includes five built-in packages that are available to programs without installing external dependencies:

| Package | Description |
|---|---|
| `math` | Mathematical functionality. |
| `string` | String-related functionality. |
| `time` | Time-related functionality. |
| `filesystem` | Filesystem operations. |
| `Log` | Logging functionality. |

## Pebble Studio

Pebble Studio is Pebble's native desktop development environment.

It provides an integrated workspace for editing and working with Pebble projects, alongside the compiler and language tooling.

## Installation

### Arch Linux and Arch-based distributions

Install the build prerequisites:

```sh
sudo pacman -S --needed base-devel cmake ninja python qt6-base qt6-tools
```

From the repository root, build the Arch package:

```sh
./scripts/build-arch-package.sh
```

The script creates an Arch Linux package from the current source tree using `makepkg`.

The package includes:

- Pebble CLI.
- C compiler frontend.
- Pebble Studio.
- Examples.
- Studio assets.

The removed engine is not included.

### Windows

Pebble provides a Windows x64 installer through the **Windows build and installer** GitHub Actions workflow.

To obtain the installer:

1. Open the repository on GitHub.
2. Navigate to the **Actions** tab.
3. Select the Windows build and installer workflow.
4. Open a completed workflow run.
5. Download the `pebble-windows-x64-installer` artifact.

The Windows installer packages Pebble Studio and the Pebble CLI executables.

**Additional development tools are currently required to compile and run Pebble programs on Windows.**

Install the MSYS2 UCRT64 toolchain and GLFW, then ensure the UCRT64 `bin` directory is included in your system `PATH`.

This is necessary because Pebble compiles generated C and C++ source code using the host system's compiler.

The current Windows installer does not bundle the complete native development toolchain.

## Building from source

Clone the repository:

```sh
git clone https://github.com/reevesisaac60-ctrl/Pebble.git
cd Pebble
```

Build requirements depend on the platform and the components being built. On Arch Linux, use the prerequisites listed in the installation section.

Build the Arch package:

```sh
./scripts/build-arch-package.sh
```

## Examples

The repository includes example Pebble programs demonstrating language features and built-in functionality.

The examples are included with the Arch package and can also be explored directly in the source repository.

## Project status

Pebble is under active development. Features, APIs, tooling and platform support may change as the project evolves.

Windows installation is available through the GitHub Actions workflow, but compiling and running Pebble programs on Windows currently requires an externally installed development toolchain.

## License

See the repository's `LICENSE` file for licensing information.