#!/usr/bin/env python3
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def require(condition, message):
    if not condition:
        raise AssertionError(message)


BLOCK = re.compile(r"```pebble[ \t]*\n(.*?)```", re.DOTALL)


def blocks(path: Path) -> list[str]:
    return BLOCK.findall(path.read_text(encoding="utf-8"))


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
    documents = [root / "docs" / "language.md", root / "README.md"]

    checked = 0
    ran = 0

    with tempfile.TemporaryDirectory(prefix="pebble-doc-examples-") as temporary:
        directory = Path(temporary)

        for document in documents:
            for index, source in enumerate(blocks(document)):
                name = f"{document.stem}_{index}.peb"
                path = directory / name
                path.write_text(source, encoding="utf-8")

                checked += 1

                result = run(executable, ["check", name], directory)

                require(
                    result.returncode == 0,
                    f"{document.name} block {index} does not type check:\n"
                    f"{source}\n{result.stdout}{result.stderr}",
                )

                if "fn main" not in source:
                    continue

                result = run(executable, ["run", name], directory)

                require(
                    result.returncode == 0,
                    f"{document.name} block {index} does not run:\n"
                    f"{source}\n{result.stdout}{result.stderr}",
                )

                ran += 1

    print(f"Documentation examples: {checked} blocks checked, {ran} run")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
