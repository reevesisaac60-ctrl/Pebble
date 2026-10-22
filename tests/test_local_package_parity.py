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


def invoke(arguments, cwd, timeout=60):
    return subprocess.run(
        arguments,
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
        timeout=timeout,
    )


CASES = [
    {
        "name": "a package next to the importing file is used",
        "files": {
            "mylib.peb": (
                "fn helper(x: Int) -> Int { return x * 2; }\n"
                "export fn add(a: Int, b: Int) -> Int { return helper(a) + b; }\n"
            ),
            "main.peb": (
                "import package mylib\n"
                "fn main() { print(mylib.add(1, 2)); }\n"
            ),
        },
        "output": "4",
    },
    {
        "name": "a package without semicolons is used",
        "files": {
            "mylib.peb": (
                "fn helper(x: Int) -> Int {\n"
                "    return x * 2\n"
                "}\n"
                "\n"
                "export fn add(a: Int, b: Int) -> Int {\n"
                "    return helper(a) + b\n"
                "}\n"
            ),
            "main.peb": (
                "import package mylib\n"
                "\n"
                "fn main() {\n"
                "    let value = mylib\n"
                "        .add(1, 2)\n"
                "    print(value)\n"
                "}\n"
            ),
        },
        "output": "4",
    },
    {
        "name": "a package in the packages directory is used",
        "files": {
            "packages/tools.peb": (
                "export fn triple(x: Int) -> Int { return x * 3; }\n"
            ),
            "main.peb": (
                "import package tools\n"
                "fn main() { print(tools.triple(5)); }\n"
            ),
        },
        "output": "15",
    },
    {
        "name": "a package may import another package",
        "files": {
            "packages/tools.peb": (
                "export fn triple(x: Int) -> Int { return x * 3; }\n"
            ),
            "outer.peb": (
                "import package tools\n"
                "fn helper(x: Int) -> Int { return tools.triple(x) + 1; }\n"
                "export fn compute(x: Int) -> Int { return helper(x); }\n"
            ),
            "main.peb": (
                "import package outer\n"
                "fn main() { print(outer.compute(2)); }\n"
            ),
        },
        "output": "7",
    },
    {
        "name": "a package may use the math package",
        "files": {
            "mylib.peb": (
                "import package math\n"
                "export fn pick(minimum: Int, maximum: Int) -> Int {\n"
                "    return math.abs(minimum) + math.max(minimum, maximum);\n"
                "}\n"
            ),
            "main.peb": (
                "import package mylib\n"
                "fn main() { print(mylib.pick(2, 5)); }\n"
            ),
        },
        "output": "7",
    },
    {
        "name": "a package function shadows a global function",
        "files": {
            "mylib.peb": (
                "fn helper(x: Int) -> Int { return x + 1; }\n"
                "export fn compute(x: Int) -> Int { return helper(x); }\n"
            ),
            "main.peb": (
                "import package mylib\n"
                "fn helper(x: Int) -> Int { return x * 100; }\n"
                "fn main() { print(mylib.compute(5)); }\n"
            ),
        },
        "output": "6",
    },
    {
        "name": "a dependency cycle is detected",
        "files": {
            "alpha.peb": (
                "import package beta\n"
                "export fn alpha_fn(value: Int) -> Int { return beta.beta_fn(value); }\n"
            ),
            "beta.peb": (
                "import package alpha\n"
                "export fn beta_fn(value: Int) -> Int { return alpha.alpha_fn(value); }\n"
            ),
            "main.peb": (
                "import package alpha\n"
                "fn main() { print(alpha.alpha_fn(1)); }\n"
            ),
        },
        "output": None,
        "stderr": "Circular dependency detected: alpha -> beta -> alpha.",
        "exit_code": 1,
    },
    {
        "name": "a private function is not visible outside its package",
        "files": {
            "mylib.peb": (
                "fn helper(x: Int) -> Int { return x * 2; }\n"
                "export fn add(a: Int, b: Int) -> Int { return helper(a) + b; }\n"
            ),
            "main.peb": (
                "import package mylib\n"
                "fn main() { print(mylib.helper(1)); }\n"
            ),
        },
        "output": None,
        "stderr": "Package 'mylib' has no function 'helper'.",
        "exit_code": 1,
    },
    {
        "name": "a missing package is rejected",
        "files": {
            "main.peb": (
                "import package nothere\n"
                "fn main() { print(1); }\n"
            ),
        },
        "output": None,
        "stderr": "Cannot find package 'nothere'",
        "exit_code": 1,
    },
    {
        "name": "a type error in a package points at the package",
        "files": {
            "mylib.peb": (
                "export fn add(a: Int, b: Int) -> Int { return a + \"text\"; }\n"
            ),
            "main.peb": (
                "import package mylib\n"
                "fn main() { print(mylib.add(1, 2)); }\n"
            ),
        },
        "output": None,
        "stderr": "mylib.peb:1:",
        "exit_code": 1,
    },
]


def build_and_run(executable, source_path, workdir):
    built = invoke([str(executable), "build", str(source_path)], workdir)

    if built.returncode != 0:
        return None, built

    binary = workdir / "build" / (
        f"{source_path.stem}-{artifact_tag(str(source_path))}"
    )

    if not binary.is_file():
        raise AssertionError(f"native executable missing for {source_path}")

    return invoke([str(binary)], workdir), built


def main():
    require(len(sys.argv) == 3, "expected C++ and C executable paths")

    cpp_executable = Path(sys.argv[1]).resolve()
    c_executable = Path(sys.argv[2]).resolve()

    with tempfile.TemporaryDirectory(prefix="pebble-local-parity-") as temporary:
        root = Path(temporary)
        source_dir = root / "sources"
        cpp_dir = root / "cpp"
        c_dir = root / "c"

        for directory in (source_dir, cpp_dir, c_dir):
            directory.mkdir()

        for index, case in enumerate(CASES):
            name = case["name"]
            stage = case.get("stage", "build")
            project = source_dir / f"case_{index}"

            project.mkdir()

            for path, contents in case["files"].items():
                target = project / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(contents, encoding="utf-8")

            source_path = project / case.get("main", "main.peb")
            results = []

            for label, executable, workdir in (
                ("C++", cpp_executable, cpp_dir),
                ("C", c_executable, c_dir),
            ):
                executed, built = build_and_run(
                    executable, source_path, workdir
                )

                if executed is None:
                    require(
                        stage == "build",
                        f"{name}: {label} rejected a valid program\n"
                        f"stdout:\n{built.stdout}\nstderr:\n{built.stderr}",
                    )
                    require(
                        built.returncode == case.get("exit_code", 1),
                        f"{name}: {label} exit {built.returncode}, "
                        f"expected {case.get('exit_code', 1)}\n"
                        f"stderr:\n{built.stderr}",
                    )
                    require(
                        case.get("stderr", "") in built.stderr,
                        f"{name}: {label} missing expected diagnostic "
                        f"{case.get('stderr', '')!r}\nstderr:\n{built.stderr}",
                    )
                    results.append(None)
                    continue

                if stage == "run":
                    require(
                        executed.returncode == case.get("exit_code", 1),
                        f"{name}: {label} executable exit "
                        f"{executed.returncode}, expected "
                        f"{case.get('exit_code', 1)}\n"
                        f"stdout:\n{executed.stdout}\nstderr:\n{executed.stderr}",
                    )
                    require(
                        case.get("stderr", "") in executed.stderr,
                        f"{name}: {label} missing expected runtime diagnostic "
                        f"{case.get('stderr', '')!r}\nstderr:\n{executed.stderr}",
                    )
                    results.append(executed)
                    continue

                require(
                    executed.returncode == 0,
                    f"{name}: {label} executable failed "
                    f"(exit {executed.returncode})\n"
                    f"stdout:\n{executed.stdout}\nstderr:\n{executed.stderr}",
                )
                results.append(executed)

            if results[0] is None:
                require(
                    results[1] is None,
                    f"{name}: the frontends disagree on accepting the program",
                )
                print(f"PASS: {name}")
                continue

            cpp_output = results[0].stdout.strip()
            c_output = results[1].stdout.strip()

            require(
                cpp_output == c_output,
                f"{name}: runtime output differs\n"
                f"C++: {cpp_output!r}\nC: {c_output!r}",
            )

            require(
                results[0].stderr == results[1].stderr,
                f"{name}: runtime diagnostics differ\n"
                f"C++: {results[0].stderr!r}\nC: {results[1].stderr!r}",
            )

            if case.get("output") is not None:
                require(
                    cpp_output == case["output"],
                    f"{name}: unexpected output\n"
                    f"Expected: {case['output']!r}\n"
                    f"Actual: {cpp_output!r}",
                )

            print(f"PASS: {name}")

    print(f"Local package parity: {len(CASES)} cases passed")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
