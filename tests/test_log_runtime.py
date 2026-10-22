#!/usr/bin/env python3
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def invoke(arguments, cwd, timeout=120):
    return subprocess.run(
        arguments,
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
        timeout=timeout,
    )


def compiler():
    configured = os.environ.get("CC")

    if configured:
        return configured

    return "cc"


def build(frontend, source, workdir):
    source_path = workdir / "log_case.peb"
    source_path.write_text(source, encoding="utf-8")

    built = invoke([str(frontend), "build", str(source_path)], workdir)

    require(
        built.returncode == 0,
        f"the C frontend rejected the program\nstdout:\n{built.stdout}"
        f"\nstderr:\n{built.stderr}",
    )

    generated = sorted((workdir / "build").glob("*.generated.c"))

    require(
        len(generated) == 1,
        f"expected one generated C file, found {len(generated)}",
    )

    return generated[0]


def run(frontend, source, workdir):
    generated = build(frontend, source, workdir)
    binaries = sorted(path for path in (workdir / "build").iterdir()
                      if path.is_file() and path.suffix != ".c")

    require(len(binaries) == 1, f"expected one program, found {len(binaries)}")

    executed = invoke([str(binaries[0])], workdir)

    require(
        executed.returncode == 0,
        f"the program failed (exit {executed.returncode})\n"
        f"stdout:\n{executed.stdout}\nstderr:\n{executed.stderr}",
    )

    return executed, generated


def test_the_three_functions_write_to_their_own_stream(frontend, root):
    source = (
        "import package Log\n"
        "fn main() {\n"
        "    Log:Print(\"printed\")\n"
        "    Log:Warning(\"careful\")\n"
        "    Log:Error(\"failed\")\n"
        "}\n"
    )
    workdir = root / "streams"
    workdir.mkdir()
    executed, _ = run(frontend, source, workdir)

    require(
        executed.stdout == "printed\n",
        f"the standard output should carry the message of Print, "
        f"got {executed.stdout!r}",
    )
    require(
        executed.stderr == "Pebble warning: careful\nPebble error: failed\n",
        f"the standard error output should carry the warnings and the errors, "
        f"got {executed.stderr!r}",
    )


def test_a_warning_keeps_the_standard_output_clean(frontend, root):
    source = (
        "import package Log\n"
        "fn main() {\n"
        "    Log:Warning(\"careful\")\n"
        "}\n"
    )
    workdir = root / "warning"
    workdir.mkdir()
    executed, _ = run(frontend, source, workdir)

    require(
        executed.stdout == "",
        f"a warning should not write to the standard output, "
        f"got {executed.stdout!r}",
    )
    require(
        executed.stderr == "Pebble warning: careful\n",
        f"a warning should be marked on the standard error output, "
        f"got {executed.stderr!r}",
    )


def test_an_error_keeps_the_standard_output_clean(frontend, root):
    source = (
        "import package Log\n"
        "fn main() {\n"
        "    Log:Error(\"failed\")\n"
        "}\n"
    )
    workdir = root / "error"
    workdir.mkdir()
    executed, _ = run(frontend, source, workdir)

    require(
        executed.stdout == "",
        f"an error should not write to the standard output, "
        f"got {executed.stdout!r}",
    )
    require(
        executed.stderr == "Pebble error: failed\n",
        f"an error should be marked on the standard error output, "
        f"got {executed.stderr!r}",
    )


def test_every_message_ends_its_line(frontend, root):
    source = (
        "import package Log\n"
        "fn main() {\n"
        "    Log:Print(\"first\")\n"
        "    Log:Print(\"second\")\n"
        "    Log:Warning(\"third\")\n"
        "}\n"
    )
    workdir = root / "lines"
    workdir.mkdir()
    executed, _ = run(frontend, source, workdir)

    require(
        executed.stdout == "first\nsecond\n",
        f"every message of the standard output should end its line, "
        f"got {executed.stdout!r}",
    )
    require(
        executed.stderr == "Pebble warning: third\n",
        f"every message of the standard error output should end its line, "
        f"got {executed.stderr!r}",
    )


def test_a_message_is_not_read_as_a_format_string(frontend, root):
    source = (
        "import package Log\n"
        "fn main() {\n"
        "    Log:Print(\"100% sure %d %s %n\")\n"
        "    Log:Warning(\"50% off %d\")\n"
        "    Log:Error(\"%s %d\")\n"
        "}\n"
    )
    workdir = root / "format"
    workdir.mkdir()
    executed, _ = run(frontend, source, workdir)

    require(
        executed.stdout == "100% sure %d %s %n\n",
        f"a message should be printed as it is, got {executed.stdout!r}",
    )
    require(
        executed.stderr == "Pebble warning: 50% off %d\nPebble error: %s %d\n",
        f"a message should be printed as it is, got {executed.stderr!r}",
    )


def test_a_missing_string_does_not_stop_the_program(frontend, root):
    source = (
        "import package Log\n"
        "fn main() {\n"
        "    Log:Print(\"hello\")\n"
        "    Log:Warning(\"careful\")\n"
        "    Log:Error(\"failed\")\n"
        "}\n"
    )
    workdir = root / "null"
    workdir.mkdir()
    generated = build(frontend, source, workdir)

    harness = workdir / "harness.c"
    harness.write_text(
        "#define main pebble_log_program_main\n"
        f'#include "{generated.resolve()}"\n'
        "#undef main\n"
        "\n"
        "int main(void)\n"
        "{\n"
        "    pebble_log_print(NULL);\n"
        "    pebble_log_warning(NULL);\n"
        "    pebble_log_error(NULL);\n"
        "    return 0;\n"
        "}\n",
        encoding="utf-8",
    )

    binary = workdir / "harness"
    compiled = invoke(
        [
            compiler(),
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Wconversion",
            "-Wshadow",
            "-Werror",
            "-Wno-return-type",
            str(harness),
            "-o",
            str(binary),
        ],
        workdir,
    )

    require(
        compiled.returncode == 0,
        f"the harness did not build\nstdout:\n{compiled.stdout}"
        f"\nstderr:\n{compiled.stderr}",
    )

    executed = invoke([str(binary)], workdir)

    require(
        executed.returncode == 0,
        f"the harness failed (exit {executed.returncode})\n"
        f"stdout:\n{executed.stdout}\nstderr:\n{executed.stderr}",
    )
    require(
        executed.stdout == "null\n",
        f"a missing string should stand in for the text null, "
        f"got {executed.stdout!r}",
    )
    require(
        executed.stderr == "Pebble warning: null\nPebble error: null\n",
        f"a missing string should stand in for the text null, "
        f"got {executed.stderr!r}",
    )


def test_only_the_used_helper_is_built(frontend, root):
    source = (
        "import package Log\n"
        "fn main() {\n"
        "    Log:Print(\"hello\")\n"
        "}\n"
    )
    workdir = root / "unused"
    workdir.mkdir()
    generated = build(frontend, source, workdir)
    text = generated.read_text(encoding="utf-8")

    require(
        "pebble_log_print" in text,
        "the helper of Print should be in the generated program",
    )
    require(
        "pebble_log_warning" not in text,
        "the helper of Warning should be left out when it is not used",
    )
    require(
        "pebble_log_error" not in text,
        "the helper of Error should be left out when it is not used",
    )


def test_a_program_without_the_package_has_no_helper(frontend, root):
    source = "fn main() { print(\"ordinary output\"); }\n"
    workdir = root / "ordinary"
    workdir.mkdir()
    generated = build(frontend, source, workdir)
    text = generated.read_text(encoding="utf-8")

    require(
        "pebble_log_print" not in text
        and "pebble_log_warning" not in text
        and "pebble_log_error" not in text,
        "a program that does not use the Log package should not carry its "
        "helpers",
    )


def main():
    require(
        len(sys.argv) == 2,
        "expected the path of the C frontend executable",
    )

    frontend = Path(sys.argv[1]).resolve()

    with tempfile.TemporaryDirectory(prefix="pebble-log-runtime-") as temporary:
        root = Path(temporary)

        test_the_three_functions_write_to_their_own_stream(frontend, root)
        test_a_warning_keeps_the_standard_output_clean(frontend, root)
        test_an_error_keeps_the_standard_output_clean(frontend, root)
        test_every_message_ends_its_line(frontend, root)
        test_a_message_is_not_read_as_a_format_string(frontend, root)
        test_a_missing_string_does_not_stop_the_program(frontend, root)
        test_only_the_used_helper_is_built(frontend, root)
        test_a_program_without_the_package_has_no_helper(frontend, root)

    print("Log runtime tests passed.")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
