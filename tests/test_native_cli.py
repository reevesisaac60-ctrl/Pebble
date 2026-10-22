import pathlib
import subprocess
import sys
import tempfile


def run(executable, arguments, cwd, expected=0):
    result = subprocess.run(
        [str(executable), *arguments],
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != expected:
        raise AssertionError(
            f"Command {arguments!r} returned {result.returncode}, "
            f"expected {expected}\nstdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    return result


def require(condition, message):
    if not condition:
        raise AssertionError(message)

def artifact_tag(filename: str) -> str:
    value = 14695981039346656037

    for byte in filename.encode("utf-8"):
        value ^= byte
        value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF

    return f"{value:016x}"

def main():
    executable = pathlib.Path(sys.argv[1]).resolve()

    with tempfile.TemporaryDirectory(prefix="pebble-native-test-") as temp:
        directory = pathlib.Path(temp)
        source = directory / "cli_smoke.peb"
        invalid = directory / "invalid.peb"

        source.write_text(
            'fn add(a: Int, b: Int) -> Int {\n'
            '    return a + b;\n'
            '}\n\n'
            'fn main() {\n'
            '    let name: String = "Pebble";\n'
            '    var score = add(20, 22);\n'
            '    score = score + 1;\n'
            '    print(name, score, true, 3.5);\n'
            '}\n',
            encoding="utf-8",
        )

        lexed = run(executable, ["lex", str(source)], directory)
        require("Fn" in lexed.stdout, "Lexer did not report function tokens")
        require("Identifier" in lexed.stdout, "Lexer did not report identifiers")

        parsed = run(executable, ["parse", str(source)], directory)
        require("Program" in parsed.stdout, "Parser did not print the program AST")
        require("Function `main`" in parsed.stdout, "AST did not contain main")

        checked = run(executable, ["check", str(source)], directory)
        require("Type check passed" in checked.stdout, "Type check success was not reported")

        built = run(executable, ["build", str(source)], directory)
        generated = directory / "build" / (
            f"cli_smoke-{artifact_tag(str(source))}.generated.c"
        )
        binary = directory / "build" / f"cli_smoke-{artifact_tag(str(source))}"

        require(generated.is_file(), "Generated C file was not created")
        require(binary.is_file(), "Native executable was not created")
        require("Built native executable" in built.stdout, "Build success was not reported")

        expected_output = "Pebble 43 true 3.5"
        executed = run(executable, ["run", str(source)], directory)
        require(
            expected_output in executed.stdout,
            f"Unexpected run output: {executed.stdout!r}",
        )

        direct = run(binary, [], directory)
        require(
            expected_output in direct.stdout,
            f"Unexpected executable output: {direct.stdout!r}",
        )

        invalid.write_text("fn main( {\n", encoding="utf-8")
        rejected = run(executable, ["check", str(invalid)], directory, expected=1)
        require("error:" in rejected.stderr, "Invalid source had no error diagnostic")

        unknown = run(executable, ["unknown", str(source)], directory, expected=2)
        require("Usage:" in unknown.stderr, "Unknown command did not print usage")

        missing = run(
            executable,
            ["check", str(directory / "missing.peb")],
            directory,
            expected=1,
        )
        require("cannot open" in missing.stderr, "Missing file error was not reported")

    print("Native CLI regression tests passed")


if __name__ == "__main__":
    main()
