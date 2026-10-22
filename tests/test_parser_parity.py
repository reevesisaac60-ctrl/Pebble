#!/usr/bin/env python3
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def parse(executable, source_path):
    return subprocess.run(
        [str(executable), "parse", str(source_path)],
        capture_output=True,
        text=True,
        check=False,
    )


def normalized_ast(output):
    return [
        re.sub(r" \[\d+:\d+\]$", "", line)
        for line in output.splitlines()
        if line.strip()
    ]


def main():
    require(len(sys.argv) == 3, "expected C++ and C executable paths")

    cpp_executable = Path(sys.argv[1]).resolve()
    c_executable = Path(sys.argv[2]).resolve()

    cases = [
        (
            "empty function",
            "fn main() { return; }\n",
            True,
            (),
        ),
        (
            "variables and arithmetic",
            "fn main() { let first = 1; let second = first + 2 * 3; print(second); }\n",
            True,
            (),
        ),
        (
            "multiple functions",
            "fn helper() { return; } fn main() { helper(); return; }\n",
            True,
            (),
        ),
        (
            "recovery after malformed declaration",
            "let = ; fn recovered() { return; }\n",
            False,
            ("Function `recovered`",),
        ),
        (
            "recovery after malformed call",
            "fn main() { print(1; return; }\n",
            False,
            ("ReturnStatement",),
        ),
        (
            "recovery after multiple errors",
            "fn main() { let first = ; let second = ; print(3); } fn recovered() { return; }\n",
            False,
            ("Function `recovered`", "NameExpression `print`", "LiteralExpression `3`"),
        ),
        (
            "typed function signatures",
            "fn add(a: Int, b: Int) -> Int { return a + b; } fn main() { let result = add(1, 2); }\n",
            True,
            (),
        ),
        (
            "package import",
            "import package math\nfn main() { let value = math.randint(1, 10); }\n",
            True,
            ("ImportDeclaration `math`", "MemberExpression `randint`"),
        ),
        (
            "package import with a semicolon",
            "import package math;\nfn main() { let value = math.randint(1, 10); }\n",
            True,
            ("ImportDeclaration `math`",),
        ),
        (
            "package import without a package name",
            "import package\nfn main() { print(1); }\n",
            False,
            (),
        ),
        (
            "the shorthand package import",
            "import math\nfn main() { let value = math.randint(1, 10); }\n",
            True,
            ("ImportDeclaration `math`",),
        ),
        (
            "newline terminated statements",
            "import package math\n"
            "fn add(a: Int, b: Int) -> Int {\n"
            "    return a + b\n"
            "}\n"
            "fn main() {\n"
            "    let total = add(10, 20)\n"
            "    print(total)\n"
            "}\n",
            True,
            ("ImportDeclaration `math`", "Function `add`"),
        ),
        (
            "an operator on a line of its own",
            "fn main() {\n    let value = 1\n    - 2\n}\n",
            True,
            ("UnaryExpression `-`",),
        ),
        (
            "a statement without a terminator",
            "fn main() { let value = 1 let other = 2 }\n",
            False,
            (),
        ),
        (
            "exported function",
            "export fn add(a: Int, b: Int) -> Int { return a + b; }\nfn main() { print(add(1, 2)); }\n",
            True,
            ("Function `add`",),
        ),
        (
            "export without a function",
            "export let value = 1;\n",
            False,
            (),
        ),
        (
            "nested control flow",
            "fn main() { var count: Int = 0; while count < 3 { if count == 1 { print(1); } else if count == 2 { print(2); } else { print(count); } count = count + 1; } }\n",
            True,
            (),
        ),
        (
            "unary and logical precedence",
            "fn main() { let ready = !false || true && false; if ready { print(1); } }\n",
            True,
            (),
        ),
        (
            "invalid assignment target",
            "fn main() { (1 + 2) = 3; }\n",
            False,
            (),
        ),
        (
            "missing block close at EOF",
            "fn main() { let value = 1;\n",
            False,
            (),
        ),
        (
            "missing call delimiter at EOF",
            "fn main() { print(1\n",
            False,
            (),
        ),
        (
            "a stray semicolon is an empty statement",
            "fn main() {\n"
            "    let value = 1;\n"
            "    print(value);\n"
            "};\n"
            ";\n",
            True,
            ("LetDeclaration `value`", "ExpressionStatement"),
        ),
        (
            "a program of empty statements",
            ";\n;\n",
            True,
            (),
        ),
        (
            "a member of a package call on the next line",
            "import package math\n"
            "fn main() {\n"
            "    let value = math\n"
            "        .max(3, 7)\n"
            "}\n",
            True,
            ("MemberExpression `max`", "CallExpression"),
        ),
        (
            "a member of a package call on the next line with a colon",
            "import package Log\n"
            "fn main() {\n"
            "    Log\n"
            "        :Info(\"hello\")\n"
            "}\n",
            True,
            ("MemberExpression `Info`", "CallExpression"),
        ),
        (
            "a binary operator on a line of its own",
            "fn main() {\n"
            "    let value = 1\n"
            "    + 2\n"
            "}\n",
            False,
            ("LetDeclaration `value`",),
        ),
        (
            "an assignment on a line of its own",
            "fn main() {\n"
            "    var value = 1\n"
            "    = 2\n"
            "}\n",
            False,
            ("VarDeclaration `value`",),
        ),
        (
            "a statement after a broken statement survives",
            "fn main() {\n"
            "    let value = 1\n"
            "    + 2\n"
            "    print(value)\n"
            "}\n",
            False,
            ("NameExpression `print`",),
        ),
        (
            "a program without semicolons",
            "import package math\n"
            "import package Log\n"
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
            "        Log:Info(\"Result is greater than 20\")\n"
            "    } else {\n"
            "        Log:Info(\"Result is 20 or less\")\n"
            "    }\n"
            "\n"
            "    Log:Info(result)\n"
            "}\n",
            True,
            ("ImportDeclaration `math`", "IfStatement"),
        ),
        (
            "a statement after a closing brace",
            "fn main() {\n"
            "    if true {\n"
            "        print(1)\n"
            "    } else {\n"
            "        print(2)\n"
            "    }\n"
            "    print(3)\n"
            "}\n",
            True,
            ("IfStatement", "ExpressionStatement"),
        ),
        (
            "a nested call over several lines",
            "fn add(a: Int, b: Int) -> Int {\n"
            "    return a + b\n"
            "}\n"
            "fn main() {\n"
            "    let total = add(\n"
            "        add(1, 2),\n"
            "        add(3, 4)\n"
            "    )\n"
            "    print(total)\n"
            "}\n",
            True,
            ("CallExpression",),
        ),
        (
            "an import without a name",
            "import package\nfn main() { print(1) }\n",
            False,
            (),
        ),
        (
            "a member that starts a line",
            "fn main() {\n"
            "    let value = 1\n"
            "    .toString()\n"
            "}\n",
            False,
            (),
        ),
    ]

    with tempfile.TemporaryDirectory(prefix="pebble-parser-parity-") as temporary:
        directory = Path(temporary)

        for index, (name, source, should_succeed, markers) in enumerate(cases):
            source_path = directory / f"case_{index}.peb"
            source_path.write_text(source, encoding="utf-8")

            cpp = parse(cpp_executable, source_path)
            native = parse(c_executable, source_path)

            require(
                (cpp.returncode == 0) == should_succeed,
                f"{name}: C++ parser returned {cpp.returncode}\n{cpp.stderr}",
            )
            require(
                (native.returncode == 0) == should_succeed,
                f"{name}: C parser returned {native.returncode}\n{native.stderr}",
            )

            if should_succeed:
                cpp_ast = normalized_ast(cpp.stdout)
                native_ast = normalized_ast(native.stdout)
                require(cpp_ast == native_ast,
                    f"{name}: AST mismatch\nC++:\n"
                    + "\n".join(cpp_ast)
                    + "\nC:\n"
                    + "\n".join(native_ast))
            else:
                for marker in markers:
                    require(marker in cpp.stdout,
                        f"{name}: C++ AST missing {marker!r}\n{cpp.stdout}")
                    require(marker in native.stdout,
                        f"{name}: C AST missing {marker!r}\n{native.stdout}")

            print(f"PASS: {name}")

    print(f"Parser parity: {len(cases)} cases passed")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
