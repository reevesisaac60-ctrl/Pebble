#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def run(executable: Path, arguments: list[str], cwd: Path) -> subprocess.CompletedProcess:
    return subprocess.run(
        [str(executable), *arguments],
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
        timeout=60,
    )


def main() -> None:
    require(len(sys.argv) == 2, "expected the pebble executable path")

    executable = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parent.parent
    examples = sorted((root / "examples").rglob("*.peb"))

    require(examples, "the repository should hold example programs")

    checked = 0
    ran = 0

    with tempfile.TemporaryDirectory(prefix="pebble-examples-") as temporary:
        directory = Path(temporary)

        for example in examples:
            source = example.read_text(encoding="utf-8")
            name = example.name
            path = directory / name
            path.write_text(source, encoding="utf-8")

            checked += 1

            result = run(executable, ["check", name], directory)

            require(
                result.returncode == 0,
                f"{example} does not type check:\n{result.stdout}{result.stderr}",
            )

            if "fn main" not in source or "package Window" in source:
                if "package OpenGL" in source:
                    result = run(executable, ["build", name], directory)
                    require(
                        result.returncode == 0,
                        f"{example} does not build through Pebble:\n{result.stdout}{result.stderr}",
                    )
                continue

            result = run(executable, ["run", name], directory)

            require(
                result.returncode == 0,
                f"{example} does not run:\n{result.stdout}{result.stderr}",
            )

            ran += 1

    print(f"Examples: {checked} programs checked, {ran} run")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
