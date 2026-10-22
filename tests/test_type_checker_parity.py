#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def check(executable, source_path):
    return subprocess.run(
        [str(executable), "check", str(source_path)],
        capture_output=True,
        text=True,
        check=False,
        timeout=10,
    )


def main():
    require(len(sys.argv) == 3, "expected C++ and C executable paths")

    cpp_executable = Path(sys.argv[1]).resolve()
    c_executable = Path(sys.argv[2]).resolve()

    cases = [
        ("valid typed program",
         "fn add(a: Int, b: Int) -> Int { return a + b; } "
         "fn main() { let title: String = \"Pebble\"; "
         "var score = add(20, 22); score = score + 1; print(title, score); }\n",
         True),
        ("initializer type mismatch",
         "fn main() { let score: Int = \"wrong\"; }\n",
         False),
        ("immutable assignment",
         "fn main() { let score = 42; score = 10; }\n",
         False),
        ("mutable assignment type mismatch",
         "fn main() { var score = 42; score = \"wrong\"; }\n",
         False),
        ("undeclared name",
         "fn main() { print(missing); }\n",
         False),
        ("wrong function argument type",
         "fn greet(name: String) { print(name); } "
         "fn main() { greet(42); }\n",
         False),
        ("wrong function arity",
         "fn add(a: Int, b: Int) -> Int { return a + b; } "
         "fn main() { let result = add(1); }\n",
         False),
        ("return type mismatch",
         "fn answer() -> Int { return \"wrong\"; }\n",
         False),
        ("unknown type name",
         "fn main() { let item: Banana = 1; }\n",
         False),
        ("integer to float widening",
         "fn main() { let value: Float = 12; "
         "var other: Float = 2.5; other = value; }\n",
         True),
        ("non-Boolean if condition",
         "fn main() { if 42 { print(1); } }\n",
         False),
        ("non-Boolean while condition",
         "fn main() { while 1 { print(1); } }\n",
         False),
        ("branch-local scope",
         "fn main() { if true { let hidden = 1; } print(hidden); }\n",
         False),
        ("duplicate declarations",
         "fn main() { let value = 1; let value = 2; } "
         "fn main() {}\n",
         False),
        ("block scope shadowing allowed",
         "fn main() { var value = 1; { var value = 2; print(value); } print(value); }\n",
         True),
        ("parameter shadowing rejected",
         "fn f(x: Int) { var x = x + 1; print(x); }\n",
         False),
        ("package import",
         "import package math\n"
         "fn main() { let value = math.randint(1, 10); print(value); }\n",
         True),
        ("unknown package",
         "import package physics\n"
         "fn main() { print(1); }\n",
         False),
        ("package call without an import",
         "fn main() { let value = math.randint(1, 10); print(value); }\n",
         False),
        ("duplicate package import",
         "import package math\n"
         "import package math\n"
         "fn main() { print(1); }\n",
         False),
        ("unknown package function",
         "import package math\n"
         "fn main() { let value = math.cbrt(2); print(value); }\n",
         False),
        ("package argument type mismatch",
         "import package math\n"
         "fn main() { let value = math.randint(1.5, 10); print(value); }\n",
         False),
        ("import inside a function",
         "fn main() { import package math; print(1); }\n",
         False),
    ]

    with tempfile.TemporaryDirectory(prefix="pebble-type-parity-") as temporary:
        directory = Path(temporary)

        for index, (name, source, should_succeed) in enumerate(cases):
            source_path = directory / f"case_{index}.peb"
            source_path.write_text(source, encoding="utf-8")

            cpp = check(cpp_executable, source_path)
            native = check(c_executable, source_path)

            cpp_succeeded = cpp.returncode == 0
            native_succeeded = native.returncode == 0

            require(
                cpp_succeeded == native_succeeded,
                f"{name}: acceptance differs\n"
                f"C++ exit={cpp.returncode}, stderr:\n{cpp.stderr}"
                f"C exit={native.returncode}, stderr:\n{native.stderr}",
            )
            require(
                cpp_succeeded == should_succeed,
                f"{name}: expected success={should_succeed}, "
                f"C++ returned {cpp.returncode}:\n{cpp.stderr}",
            )
            require(
                native_succeeded == should_succeed,
                f"{name}: expected success={should_succeed}, "
                f"C returned {native.returncode}:\n{native.stderr}",
            )

            if not should_succeed:
                require(
                    "error:" in cpp.stderr,
                    f"{name}: C++ frontend did not emit an error diagnostic",
                )
                require(
                    "error:" in native.stderr,
                    f"{name}: C frontend did not emit an error diagnostic",
                )

            print(f"PASS: {name}")

    print(f"Type-checker parity: {len(cases)} cases passed")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
