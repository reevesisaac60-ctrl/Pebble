#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


def require(condition, message):
    if not condition:
        raise AssertionError(message)

def artifact_tag(filename: str) -> str:
    value = 14695981039346656037

    for byte in filename.encode("utf-8"):
        value ^= byte
        value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF

    return f"{value:016x}"

def invoke(arguments, cwd, timeout=30):
    return subprocess.run(
        arguments,
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
        timeout=timeout,
    )


def main():
    require(len(sys.argv) == 3, "expected C++ and C executable paths")

    cpp_executable = Path(sys.argv[1]).resolve()
    c_executable = Path(sys.argv[2]).resolve()

    cases = [
        (
            "arithmetic precedence",
            "fn main() { print(1 + 2 * 3, (10 - 4) / 2); }\n",
            "7 3",
            None,
        ),
        (
            "strings booleans and floats",
            'fn main() { print("Pebble", true, false, 3.5); }\n',
            "Pebble true false 3.5",
            None,
        ),
        (
            "function calls",
            "fn add(a: Int, b: Int) -> Int { return a + b; }\n"
            "fn main() { print(add(20, 22)); }\n",
            "42",
            None,
        ),
        (
            "nested function calls",
            "fn double(value: Int) -> Int { return value * 2; }\n"
            "fn add(a: Int, b: Int) -> Int { return a + b; }\n"
            "fn main() { print(double(add(3, 4))); }\n",
            "14",
            None,
        ),
        (
            "newline terminated statements",
            "fn add(a: Int, b: Int) -> Int {\n"
            "    return a + b\n"
            "}\n"
            "\n"
            "fn main() {\n"
            "    let total = add(10, 20)\n"
            "    print(total)\n"
            "}\n",
            "30",
            None,
        ),
        (
            "expressions over several lines",
            "fn add(a: Int, b: Int) -> Int { return a + b; }\n"
            "fn main() {\n"
            "    let total = add(\n"
            "        10,\n"
            "        20\n"
            "    )\n"
            "    print(total + 1)\n"
            "}\n",
            "31",
            None,
        ),
        (
            "a program without semicolons",
            "import package math\n"
            "\n"
            "fn add(a: Int, b: Int) -> Int {\n"
            "    return a + b\n"
            "}\n"
            "\n"
            "fn main() {\n"
            "    let x = 10\n"
            "    let y = 20\n"
            "    let result = add(x, y)\n"
            "\n"
            "    if result > 20 {\n"
            "        print(\"greater\")\n"
            "    } else {\n"
            "        print(\"smaller\")\n"
            "    }\n"
            "\n"
            "    var total = 0\n"
            "    var index = 0\n"
            "    while index < 3 {\n"
            "        total = total + index\n"
            "        index = index + 1\n"
            "    }\n"
            "\n"
            "    print(result, total, math.max(1, 2))\n"
            "}\n",
            "greater\n30 3 2",
            None,
        ),
        (
            "a stray semicolon does not change the program",
            "fn main() {\n"
            "    let value = 1;\n"
            "    print(value);\n"
            "};\n",
            "1",
            None,
        ),
        (
            "a package call over several lines",
            "import package math\n"
            "fn main() {\n"
            "    print(math\n"
            "        .max(3, 7))\n"
            "}\n",
            "7",
            None,
        ),
        (
            "the canonical package import",
            "import package math\n"
            "fn main() {\n"
            "    print(math.max(3, 7))\n"
            "}\n",
            "7",
            None,
        ),
        (
            "the shorthand package import",
            "import math\n"
            "fn main() {\n"
            "    print(math.max(3, 7))\n"
            "}\n",
            "7",
            None,
        ),
        (
            "void function",
            "fn announce() { print(\"started\"); }\n"
            "fn main() { announce(); print(2); }\n",
            "started\n2",
            None,
        ),
        (
            "variable reassignment",
            "fn main() { var score: Int = 10;\n"
            "score = score + 5;\n"
            "print(score); }\n",
            "15",
            None,
        ),
        (
            "false conditional branch",
            'fn main() { if false { print("wrong"); } '
            'else { print("correct"); } }\n',
            "correct",
            None,
        ),
        (
            "boolean variables",
            "fn main() { let enabled: Bool = true;\n"
            "let disabled: Bool = false;\n"
            "print(enabled, disabled); }\n",
            "true false",
            None,
        ),
        (
            "nested loops",
            "fn main() { var row: Int = 0;\n"
            "while row < 2 { var column: Int = 0;\n"
            "while column < 2 { print(row, column);\n"
            "column = column + 1; }\n"
            "row = row + 1; } }\n",
            "0 0\n0 1\n1 0\n1 1",
            None,
        ),
        (
            "conditional branches",
            'fn main() { if true { print("yes"); } '
            'else { print("no"); } }\n',
            "yes",
            None,
        ),
        (
            "loop execution",
            "fn main() { var i: Int = 0; "
            "while i < 3 { print(i); i = i + 1; } }\n",
            "0\n1\n2",
            None,
        ),
        (
            "logical operator precedence",
            'fn main() { let ready = !false || true && false; '
            'if ready { print("ready"); } else { print("not ready"); } }\n',
            "ready",
            None,
        ),
        (
            "integer to float widening",
            "fn main() { let value: Float = 12; print(value); }\n",
            "12",
            None,
        ),
        (
            "block scope shadowing",
            "fn main() { var value: Int = 1; { var value: Int = 2; print(value); } print(value); }\n",
            "2\n1",
            None,
        ),
        (
            "unary minus precedence",
            "fn main() { print(-2 + 3 * 4); }\n",
            "10",
            None,
        ),
        (
            "float arithmetic",
            "fn main() { print(1.5 + 2.5); }\n",
            "4",
            None,
        ),
        (
            "the null literal is printed",
            "fn main() { print(null); }\n",
            "null",
            None,
        ),
        (
            "a null variable keeps its value",
            "fn main() { let empty = null; var also = null;\n"
            "also = null; print(empty, also); }\n",
            "null null",
            None,
        ),
        (
            "null values are compared",
            "fn main() { let empty = null;\n"
            "print(empty == null, empty != null, null == null); }\n",
            "true false true",
            None,
        ),
        (
            "null is not a string",
            "fn main() { let text: String = null; }\n",
            None,
            "with a value of type 'Null'",
        ),
        (
            "the null type is not a type name",
            "fn main() { let empty: Null = null; }\n",
            None,
            "Unknown type 'Null'",
        ),
        (
            "null is not a condition",
            "fn main() { if null { print(1); } }\n",
            None,
            "If condition must have type 'Bool', not 'Null'",
        ),
        (
            "null is not compared with a string",
            "fn main() { print(null == \"text\"); }\n",
            None,
            "Cannot compare values of type 'Null' and 'String'",
        ),
        (
            "undeclared function call",
            "fn main() { print(missing(1)); }\n",
            None,
            "Call to undeclared function",
        ),
        (
            "wrong argument count",
            "fn add(a: Int, b: Int) -> Int { return a + b; } "
            "fn main() { let result = add(1); }\n",
            None,
            "expects 2 argument(s), but got 1",
        ),
        (
            "immutable variable assignment",
            "fn main() { let score = 42; score = 10; }\n",
            None,
            "Cannot assign to immutable variable",
        ),
        (
            "non-Boolean if condition",
            "fn main() { if 42 { print(1); } }\n",
            None,
            "If condition must have type 'Bool'",
        ),
        (
            "initializer type mismatch",
            "fn main() { let score: Int = \"wrong\"; }\n",
            None,
            "Cannot initialise 'score' of type 'Int' with a value of type 'String'",
        ),
    ]

    with tempfile.TemporaryDirectory(prefix="pebble-runtime-parity-") as temporary:
        root = Path(temporary)
        source_dir = root / "sources"
        cpp_dir = root / "cpp"
        c_dir = root / "c"

        for directory in (source_dir, cpp_dir, c_dir):
            directory.mkdir()

        for index, (name, source, expected_output, expected_diag) in enumerate(cases):
            source_path = source_dir / f"case_{index}.peb"
            source_path.write_text(source, encoding="utf-8")

            if expected_output is None:
                for label, executable, workdir in (
                    ("C++", cpp_executable, cpp_dir),
                    ("C", c_executable, c_dir),
                ):
                    built = invoke(
                        [str(executable), "build", str(source_path)],
                        workdir,
                    )
                    require(
                        built.returncode != 0,
                        f"{name}: {label} unexpectedly accepted invalid program "
                        f"(exit {built.returncode})\n"
                        f"stdout:\n{built.stdout}\nstderr:\n{built.stderr}",
                    )
                    require(
                        expected_diag in built.stderr,
                        f"{name}: {label} missing expected diagnostic "
                        f"{expected_diag!r}\nstderr:\n{built.stderr}",
                    )

                print(f"PASS: {name}")
                continue

            results = []

            for label, executable, workdir in (
                ("C++", cpp_executable, cpp_dir),
                ("C", c_executable, c_dir),
            ):
                built = invoke(
                    [str(executable), "build", str(source_path)],
                    workdir,
                )
                require(
                    built.returncode == 0,
                    f"{name}: {label} build failed "
                    f"(exit {built.returncode})\n"
                    f"stdout:\n{built.stdout}\nstderr:\n{built.stderr}",
                )

                binary = workdir / "build" / (
                    f"{source_path.stem}-{artifact_tag(str(source_path))}"
                )
                require(binary.is_file(), f"{name}: {label} binary missing")

                executed = invoke([str(binary)], workdir)
                require(
                    executed.returncode == 0,
                    f"{name}: {label} executable failed "
                    f"(exit {executed.returncode})\n"
                    f"stdout:\n{executed.stdout}\nstderr:\n{executed.stderr}",
                )
                results.append((label, executed.stdout.strip()))

            cpp_label, cpp_output = results[0]
            c_label, c_output = results[1]

            require(
                cpp_output == c_output,
                f"{name}: runtime output differs\n"
                f"{cpp_label}: {cpp_output!r}\n"
                f"{c_label}: {c_output!r}",
            )
            require(
                cpp_output == expected_output,
                f"{name}: unexpected output\n"
                f"Expected: {expected_output!r}\n"
                f"Actual: {cpp_output!r}",
            )

            print(f"PASS: {name}")

    print(f"Runtime parity: {len(cases)} cases passed")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
