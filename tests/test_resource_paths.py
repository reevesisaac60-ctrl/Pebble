#!/usr/bin/env python3
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def invoke(arguments, cwd, environment=None, timeout=300):
    return subprocess.run(
        [str(argument) for argument in arguments],
        cwd=cwd,
        env=environment,
        capture_output=True,
        text=True,
        check=False,
        timeout=timeout,
    )


SOURCE = (
    "import package filesystem\n"
    "fn main() {\n"
    "    print(filesystem.read(\"data.txt\"));\n"
    "}\n"
)


def write_program(directory, name="read.peb", source=SOURCE):
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / name
    path.write_text(source, encoding="utf-8")
    return path


def build(frontend, program, cwd):
    result = invoke([frontend, "build", program], cwd)

    require(
        result.returncode == 0,
        f"the program did not build (exit {result.returncode})\n"
        f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
    )

    built = [
        path
        for path in (cwd / "build").iterdir()
        if path.is_file() and path.suffix != ".cpp"
    ]

    require(len(built) == 1, f"expected one program, found {len(built)}")

    return built[0]


def run_program(frontend, program, cwd, environment=None):
    binary = build(frontend, program, cwd)
    result = invoke([binary], cwd, environment)

    require(
        result.returncode == 0,
        f"the program failed (exit {result.returncode})\n"
        f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
    )

    return result.stdout.strip()


def test_a_file_of_the_working_directory_is_read(frontend, root):
    workdir = root / "working"
    workdir.mkdir()
    (workdir / "data.txt").write_text("from the working directory", encoding="utf-8")
    program = write_program(workdir)

    require(
        run_program(frontend, program, workdir) == "from the working directory",
        "the file beside the working directory should be read",
    )


def test_a_file_beside_the_program_is_read_from_anywhere(frontend, root):
    workdir = root / "beside"
    program = write_program(workdir / "program")
    (workdir / "program" / "data.txt").write_text("from the program", encoding="utf-8")

    elsewhere = root / "elsewhere"
    elsewhere.mkdir()

    require(
        run_program(frontend, program, elsewhere) == "from the program",
        "the file beside the program should be read from any directory",
    )


def test_a_file_of_the_project_root_is_read_from_anywhere(frontend, root):
    project = root / "project"
    project.mkdir()
    (project / "pebble.toml").write_text(
        '[package]\nname = "demo"\nversion = "0.1.0"\n', encoding="utf-8"
    )
    (project / "data.txt").write_text("from the project", encoding="utf-8")
    program = write_program(project / "src")

    elsewhere = root / "outside"
    elsewhere.mkdir()

    require(
        run_program(frontend, program, elsewhere) == "from the project",
        "the file of the project root should be read from any directory",
    )


def test_a_file_of_the_resource_path_is_read(frontend, root):
    workdir = root / "environment"
    workdir.mkdir()
    program = write_program(workdir)

    resources = root / "resources"
    resources.mkdir()
    (resources / "data.txt").write_text("from the environment", encoding="utf-8")

    environment = dict(os.environ)
    environment["PEBBLE_RESOURCE_PATH"] = str(resources)

    require(
        run_program(frontend, program, workdir, environment)
        == "from the environment",
        "the file of the resource path should be read",
    )


def test_the_resource_path_is_a_list(frontend, root):
    workdir = root / "list"
    workdir.mkdir()
    program = write_program(workdir)

    first = root / "first"
    first.mkdir()
    (first / "data.txt").write_text("from the first directory", encoding="utf-8")

    second = root / "second"
    second.mkdir()
    (second / "other.txt").write_text("from the second directory", encoding="utf-8")

    environment = dict(os.environ)
    environment["PEBBLE_RESOURCE_PATH"] = os.pathsep.join([str(first), str(second)])

    require(
        run_program(frontend, program, workdir, environment)
        == "from the first directory",
        "the first directory of the resource path should be read",
    )

    other = root / "list-other"
    other.mkdir()

    require(
        run_program(
            frontend,
            write_program(
                other,
                name="read-other.peb",
                source=(
                    "import package filesystem\n"
                    "fn main() {\n"
                    "    print(filesystem.read(\"other.txt\"));\n"
                    "}\n"
                ),
            ),
            other,
            environment,
        )
        == "from the second directory",
        "the second directory of the resource path should be read",
    )


def test_the_working_directory_wins_over_the_resource_paths(frontend, root):
    workdir = root / "precedence"
    workdir.mkdir()
    (workdir / "data.txt").write_text("from the working directory", encoding="utf-8")
    program = write_program(workdir / "program")
    (workdir / "program" / "data.txt").write_text("from the program", encoding="utf-8")

    require(
        run_program(frontend, program, workdir) == "from the working directory",
        "the file of the working directory should be read first",
    )


def test_an_absolute_path_is_read_as_it_is(frontend, root):
    workdir = root / "absolute"
    workdir.mkdir()
    elsewhere = root / "somewhere-else"
    elsewhere.mkdir()
    (elsewhere / "data.txt").write_text("from the absolute path", encoding="utf-8")

    program = write_program(
        workdir,
        source=(
            "import package filesystem\n"
            "fn main() {\n"
            f"    print(filesystem.read(\"{elsewhere / 'data.txt'}\"));\n"
            "}\n"
        ),
    )

    require(
        run_program(frontend, program, workdir) == "from the absolute path",
        "an absolute path should be read as it is",
    )


def test_a_file_that_is_nowhere_is_reported(frontend, root):
    workdir = root / "missing"
    workdir.mkdir()
    program = write_program(workdir)

    binary = build(frontend, program, workdir)
    result = invoke([binary], workdir)

    require(
        result.returncode != 0,
        "a program that reads a file that is nowhere should fail",
    )
    require(
        "Pebble runtime error: filesystem.read cannot read 'data.txt'"
        in result.stderr,
        f"the failure should be reported, got {result.stderr!r}",
    )


def main():
    require(
        len(sys.argv) == 2,
        "expected the path of the frontend executable",
    )

    frontend = Path(sys.argv[1]).resolve()

    with tempfile.TemporaryDirectory(prefix="pebble-resource-paths-") as temporary:
        root = Path(temporary)

        test_a_file_of_the_working_directory_is_read(frontend, root)
        test_a_file_beside_the_program_is_read_from_anywhere(frontend, root)
        test_a_file_of_the_project_root_is_read_from_anywhere(frontend, root)
        test_a_file_of_the_resource_path_is_read(frontend, root)
        test_the_resource_path_is_a_list(frontend, root)
        test_the_working_directory_wins_over_the_resource_paths(frontend, root)
        test_an_absolute_path_is_read_as_it_is(frontend, root)
        test_a_file_that_is_nowhere_is_reported(frontend, root)

    print("Resource path tests passed.")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
