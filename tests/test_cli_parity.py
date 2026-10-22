#!/usr/bin/env python3
import filecmp
import subprocess
import sys
import tempfile
from pathlib import Path


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def invoke(arguments, cwd):
    return subprocess.run(
        arguments,
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
        timeout=60,
    )


def collect_files(directory):
    return sorted(
        str(path.relative_to(directory))
        for path in directory.rglob("*")
        if path.is_file() and not path.relative_to(directory).parts[0] == "build"
    )


def compare_trees(left, right):
    left_files = collect_files(left)
    right_files = collect_files(right)

    require(
        left_files == right_files,
        f"the frontends created different files\n{left_files}\n{right_files}",
    )

    for name in left_files:
        require(
            filecmp.cmp(left / name, right / name, shallow=False),
            f"the frontends created a different '{name}'",
        )


def run_workflow(executable, root):
    library = root / "library"
    application = root / "application"

    root.mkdir(parents=True, exist_ok=True)
    library.mkdir()
    application.mkdir()

    result = invoke([str(executable), "init", "math_tools"], library)
    require(result.returncode == 0, f"init failed:\n{result.stderr}")

    result = invoke(
        [str(executable), "init", "calculator"], application
    )
    require(result.returncode == 0, f"init failed:\n{result.stderr}")

    result = invoke(
        [str(executable), "add", "../library"], application
    )
    require(result.returncode == 0, f"add failed:\n{result.stderr}")

    result = invoke([str(executable), "install"], application)
    require(result.returncode == 0, f"install failed:\n{result.stderr}")

    result = invoke([str(executable), "update"], application)
    require(result.returncode == 0, f"update failed:\n{result.stderr}")

    result = invoke([str(executable), "tree"], application)
    require(result.returncode == 0, f"tree failed:\n{result.stderr}")

    result = invoke([str(executable), "package"], application)
    require(result.returncode == 0, f"package failed:\n{result.stderr}")

    result = invoke([str(executable), "build"], application)
    require(result.returncode == 0, f"build failed:\n{result.stderr}")

    result = invoke([str(executable), "run"], application)
    require(result.returncode == 0, f"run failed:\n{result.stderr}")

    require(
        "Hello, Pebble!" in result.stdout,
        f"unexpected program output: {result.stdout!r}",
    )

    result = invoke([str(executable), "remove", "math_tools"], application)
    require(result.returncode == 0, f"remove failed:\n{result.stderr}")

    manifest = (application / "pebble.toml").read_text(encoding="utf-8")
    require(
        "math_tools" not in manifest,
        f"the dependency was not removed:\n{manifest}",
    )

    return application, library, result.stdout


def run_cycle_workflow(executable, root):
    alpha = root / "alpha"
    beta = root / "beta"

    root.mkdir(parents=True, exist_ok=True)
    alpha.mkdir()
    beta.mkdir()

    (alpha / "pebble.toml").write_text(
        '[package]\n'
        'name = "alpha"\n'
        'version = "1.0.0"\n'
        '\n'
        '[dependencies]\n'
        'beta = { path = "../beta" }\n',
        encoding="utf-8",
    )
    (beta / "pebble.toml").write_text(
        '[package]\n'
        'name = "beta"\n'
        'version = "1.0.0"\n'
        '\n'
        '[dependencies]\n'
        'alpha = { path = "../alpha" }\n',
        encoding="utf-8",
    )

    result = invoke([str(executable), "install"], alpha)
    require(result.returncode != 0, f"the cycle was not reported:\n{result.stdout}")
    require(
        "Circular dependency detected: alpha -> beta -> alpha." in result.stderr,
        f"unexpected diagnostic: {result.stderr!r}",
    )


def main():
    require(len(sys.argv) == 3, "expected C++ and C executable paths")

    executables = (Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())

    with tempfile.TemporaryDirectory(prefix="pebble-cli-parity-") as temporary:
        root = Path(temporary)
        results = []

        for index, executable in enumerate(executables):
            results.append(run_workflow(executable, root / f"case_{index}"))
            run_cycle_workflow(executable, root / f"cycle_{index}")

        compare_trees(results[0][0], results[1][0])
        compare_trees(results[0][1], results[1][1])

        require(
            results[0][2] == results[1][2],
            f"the frontends printed different output\n"
            f"C++: {results[0][2]!r}\nC: {results[1][2]!r}",
        )

    print("Package command parity: the frontends agree")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
