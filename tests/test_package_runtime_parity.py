#!/usr/bin/env python3
import subprocess
import sys
import tempfile
import time
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


UNIX_TIME = int(time.time())


CASES = [
    {
        "name": "randint within an inclusive range",
        "source": (
            "import package math\n"
            "fn main() { print(math.randint(1, 10)); }\n"
        ),
        "output": None,
        "range": (1, 10),
        "compare": "bounds",
    },
    {
        "name": "randint with equal bounds",
        "source": (
            "import package math\n"
            "fn main() { print(math.randint(7, 7)); }\n"
        ),
        "output": "7",
    },
    {
        "name": "randint with a negative range",
        "source": (
            "import package math\n"
            "fn main() { print(math.randint(-5, -1)); }\n"
        ),
        "output": None,
        "range": (-5, -1),
        "compare": "bounds",
    },
    {
        "name": "randint across the whole Int range",
        "source": (
            "import package math\n"
            "fn main() { print(math.randint(-9223372036854775807, "
            "9223372036854775807)); }\n"
        ),
        "output": None,
        "range": (-9223372036854775807, 9223372036854775807),
        "compare": "bounds",
    },
    {
        "name": "randint with the smallest Int bound",
        "source": (
            "import package math\n"
            "fn main() { print(math.randint(-9223372036854775808, "
            "-9223372036854775808)); }\n"
        ),
        "output": "-9223372036854775808",
    },
    {
        "name": "several randint calls produce a sequence",
        "source": (
            "import package math\n"
            "fn main() {\n"
            "    print(math.randint(1, 100));\n"
            "    print(math.randint(1, 100));\n"
            "    print(math.randint(1, 100));\n"
            "}\n"
        ),
        "output": None,
        "range": (1, 100),
        "compare": "bounds",
    },
    {
        "name": "randint result assigned to a variable",
        "source": (
            "import package math\n"
            "fn main() {\n"
            "    let number = math.randint(1, 10);\n"
            "    print(number);\n"
            "}\n"
        ),
        "output": None,
        "range": (1, 10),
        "compare": "bounds",
    },
    {
        "name": "randint used in an expression",
        "source": (
            "import package math\n"
            "fn main() { print(math.randint(1, 10) + math.randint(1, 10)); }\n"
        ),
        "output": None,
        "range": (2, 20),
        "compare": "bounds",
    },
    {
        "name": "randint inside a loop",
        "source": (
            "import package math\n"
            "fn main() {\n"
            "    var index: Int = 0;\n"
            "    while index < 5 {\n"
            "        print(math.randint(1, 10));\n"
            "        index = index + 1;\n"
            "    }\n"
            "}\n"
        ),
        "output": None,
        "range": (1, 10),
        "compare": "bounds",
    },
    {
        "name": "math helper functions",
        "source": (
            "import package math\n"
            "fn main() { print(math.min(3, 9), math.max(3, 9), math.abs(-5)); }\n"
        ),
        "output": "3 9 5",
    },
    {
        "name": "randomseed changes the sequence",
        "source": (
            "import package math\n"
            "fn main() {\n"
            "    math.randomseed(12345);\n"
            "    print(math.randint(1, 1000000));\n"
            "}\n"
        ),
        "output": None,
        "range": (1, 1000000),
        "repeat": True,
    },
    {
        "name": "a seeded sequence is reproducible",
        "source": (
            "import package math\n"
            "fn main() {\n"
            "    math.randomseed(7);\n"
            "    print(math.randint(1, 1000));\n"
            "    print(math.randint(1, 1000));\n"
            "}\n"
        ),
        "output": None,
        "repeat": True,
    },
    {
        "name": "square roots are computed",
        "source": (
            "import package math\n"
            "fn main() { print(math.sqrt(16.0), math.sqrt(9)); }\n"
        ),
        "output": "4 3",
    },
    {
        "name": "rounding functions are computed",
        "source": (
            "import package math\n"
            "fn main() {\n"
            "    print(math.floor(3.7));\n"
            "    print(math.ceil(3.2));\n"
            "    print(math.round(3.5));\n"
            "}\n"
        ),
        "output": "3\n4\n4",
    },
    {
        "name": "pi is available",
        "source": (
            "import package math\n"
            "fn main() { print(math.pi()); }\n"
        ),
        "output": "3.14159",
    },
    {
        "name": "a negative square root is rejected",
        "source": (
            "import package math\n"
            "fn main() { print(math.sqrt(-1.0)); }\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "math.sqrt is undefined for negative values",
        "exit_code": 1,
    },
    {
        "name": "random returns a fraction",
        "source": (
            "import package math\n"
            "fn main() { print(math.random()); }\n"
        ),
        "output": None,
        "compare": "bounds",
        "float_range": (0.0, 1.0),
    },
    {
        "name": "reversed bounds report a runtime error",
        "source": (
            "import package math\n"
            "fn main() { print(math.randint(10, 1)); }\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "math.randint requires min <= max (min=10, max=1)",
        "exit_code": 1,
    },
    {
        "name": "reversed bounds stop the program",
        "source": (
            "import package math\n"
            "fn main() {\n"
            "    print(\"before\");\n"
            "    let value = math.randint(10, 1);\n"
            "    print(\"after\");\n"
            "}\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "math.randint requires min <= max",
        "exit_code": 1,
    },
    {
        "name": "abs reports a runtime error for the smallest Int",
        "source": (
            "import package math\n"
            "fn main() { print(math.abs(-9223372036854775808)); }\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "math.abs is undefined for -9223372036854775808",
        "exit_code": 1,
    },
    {
        "name": "an unknown package is rejected before compiling",
        "source": (
            "import package physics\n"
            "fn main() { print(1); }\n"
        ),
        "output": None,
        "stderr": "Cannot find package 'physics'",
        "exit_code": 1,
    },
    {
        "name": "a package call without an import is rejected",
        "source": (
            "fn main() { print(math.randint(1, 10)); }\n"
        ),
        "output": None,
        "stderr": "Unknown package 'math'",
        "exit_code": 1,
    },
    {
        "name": "a wrong argument count is rejected",
        "source": (
            "import package math\n"
            "fn main() { print(math.randint(1)); }\n"
        ),
        "output": None,
        "stderr": "expects 2 argument(s), but got 1",
        "exit_code": 1,
    },
    {
        "name": "a wrong argument type is rejected",
        "source": (
            "import package math\n"
            "fn main() { print(math.randint(1.5, 10)); }\n"
        ),
        "output": None,
        "stderr": "expects 'Int', but got 'Float'",
        "exit_code": 1,
    },
    {
        "name": "an unknown package function is rejected",
        "source": (
            "import package math\n"
            "fn main() { print(math.cbrt(2)); }\n"
        ),
        "output": None,
        "stderr": "has no function 'cbrt'",
        "exit_code": 1,
    },
    {
        "name": "the length of a string is counted",
        "source": (
            "import package string\n"
            "fn main() { print(string.length(\"hello\")); }\n"
        ),
        "output": "5",
    },
    {
        "name": "the length of an empty string is zero",
        "source": (
            "import package string\n"
            "fn main() { print(string.length(\"\")); }\n"
        ),
        "output": "0",
    },
    {
        "name": "a part of a string is taken",
        "source": (
            "import package string\n"
            "fn main() { print(string.substring(\"hello world\", 6, 5)); }\n"
        ),
        "output": "world",
    },
    {
        "name": "a part that reaches past the end is cut off",
        "source": (
            "import package string\n"
            "fn main() { print(string.substring(\"hello\", 2, 100)); }\n"
        ),
        "output": "llo",
    },
    {
        "name": "the left and the right of a string are taken",
        "source": (
            "import package string\n"
            "fn main() { print(string.left(\"hello\", 2), string.right(\"hello\", 2)); }\n"
        ),
        "output": "he lo",
    },
    {
        "name": "a part that is longer than the string is the whole string",
        "source": (
            "import package string\n"
            "fn main() { print(string.left(\"hi\", 9), string.right(\"hi\", 9)); }\n"
        ),
        "output": "hi hi",
    },
    {
        "name": "the case of a string is changed",
        "source": (
            "import package string\n"
            "fn main() { print(string.upper(\"Hello\"), string.lower(\"Hello\")); }\n"
        ),
        "output": "HELLO hello",
    },
    {
        "name": "the space around a string is removed",
        "source": (
            "import package string\n"
            "fn main() { print(\"[\" + string.trim(\"  hello  \") + \"]\"); }\n"
        ),
        "output": "[hello]",
    },
    {
        "name": "a string is searched",
        "source": (
            "import package string\n"
            "fn main() {\n"
            "    print(string.contains(\"hello\", \"ell\"));\n"
            "    print(string.contains(\"hello\", \"xyz\"));\n"
            "    print(string.startswith(\"hello\", \"he\"));\n"
            "    print(string.endswith(\"hello\", \"lo\"));\n"
            "    print(string.indexof(\"hello\", \"l\"));\n"
            "    print(string.indexof(\"hello\", \"z\"));\n"
            "}\n"
        ),
        "output": "true\nfalse\ntrue\ntrue\n2\n-1",
    },
    {
        "name": "a part of a string is replaced",
        "source": (
            "import package string\n"
            "fn main() { print(string.replace(\"a-b-c\", \"-\", \"+\")); }\n"
        ),
        "output": "a+b+c",
    },
    {
        "name": "a string is repeated",
        "source": (
            "import package string\n"
            "fn main() { print(string.repeat(\"ab\", 3), string.repeat(\"ab\", 0)); }\n"
        ),
        "output": "ababab",
    },
    {
        "name": "a string is parsed as a number",
        "source": (
            "import package string\n"
            "fn main() { print(string.toint(\"42\"), string.tofloat(\"2.5\")); }\n"
        ),
        "output": "42 2.5",
    },
    {
        "name": "a part of a string is out of range",
        "source": (
            "import package string\n"
            "fn main() { print(string.substring(\"abc\", 5, 1)); }\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "string.substring is out of range (start=5, length=3)",
        "exit_code": 1,
    },
    {
        "name": "a negative count is rejected",
        "source": (
            "import package string\n"
            "fn main() { print(string.repeat(\"a\", -1)); }\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "string.repeat needs a non negative count (count=-1)",
        "exit_code": 1,
    },
    {
        "name": "a number that cannot be parsed is rejected",
        "source": (
            "import package string\n"
            "fn main() { print(string.toint(\"abc\")); }\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "string.toint cannot parse 'abc'",
        "exit_code": 1,
    },
    {
        "name": "an unknown string function is rejected",
        "source": (
            "import package string\n"
            "fn main() { print(string.split(\"a,b\", \",\")); }\n"
        ),
        "output": None,
        "stderr": "has no function 'split'",
        "exit_code": 1,
    },
    {
        "name": "the first delta of a program is zero",
        "source": (
            "import package time\n"
            "fn main() { print(time.delta()); }\n"
        ),
        "output": "0",
    },
    {
        "name": "the shorthand import of the time package",
        "source": (
            "import time\n"
            "fn main() { print(time.delta()); }\n"
        ),
        "output": "0",
    },
    {
        "name": "the clock starts at the start of the program",
        "source": (
            "import package time\n"
            "fn main() { print(time.now()); }\n"
        ),
        "output": None,
        "compare": "bounds",
        "float_range": (0.0, 60.0),
    },
    {
        "name": "the clock counts the nanoseconds of the program",
        "source": (
            "import package time\n"
            "fn main() { print(time.ticks()); }\n"
        ),
        "output": None,
        "compare": "bounds",
        "range": (0, 60000000000),
    },
    {
        "name": "the machine time is the time since 1970",
        "source": (
            "import package time\n"
            "fn main() { print(time.unix()); }\n"
        ),
        "output": None,
        "compare": "bounds",
        "range": (UNIX_TIME - 600, UNIX_TIME + 600),
    },
    {
        "name": "a sleep waits for the time it is given",
        "source": (
            "import package time\n"
            "fn main() {\n"
            "    let first = time.delta();\n"
            "    time.sleep(0.05);\n"
            "    print(time.delta());\n"
            "}\n"
        ),
        "output": None,
        "compare": "bounds",
        "float_range": (0.05, 5.0),
    },
    {
        "name": "the delta measures the frames of a loop",
        "source": (
            "import package time\n"
            "fn main() {\n"
            "    let first = time.delta();\n"
            "    var frame = 0;\n"
            "    while frame < 3 {\n"
            "        time.sleep(0.01);\n"
            "        print(time.delta());\n"
            "        frame = frame + 1;\n"
            "    }\n"
            "}\n"
        ),
        "output": None,
        "compare": "bounds",
        "float_range": (0.01, 5.0),
    },
    {
        "name": "a negative sleep is rejected",
        "source": (
            "import package time\n"
            "fn main() { time.sleep(-1.0); print(\"not reached\"); }\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "time.sleep needs a non negative duration (duration=-1)",
        "exit_code": 1,
    },
    {
        "name": "a sleep without a duration is rejected",
        "source": (
            "import package time\n"
            "fn main() { time.sleep(); }\n"
        ),
        "output": None,
        "stderr": "expects 1 argument(s), but got 0",
        "exit_code": 1,
    },
    {
        "name": "an unknown time function is rejected",
        "source": (
            "import package time\n"
            "fn main() { print(time.frame()); }\n"
        ),
        "output": None,
        "stderr": "has no function 'frame'",
        "exit_code": 1,
    },
    {
        "name": "a file that is not there does not exist",
        "source": (
            "import package filesystem\n"
            "fn main() { print(filesystem.exists(\"pebble_missing.txt\")); }\n"
        ),
        "output": "false",
    },
    {
        "name": "the shorthand import of the filesystem package",
        "source": (
            "import filesystem\n"
            "fn main() { print(filesystem.exists(\"pebble_missing.txt\")); }\n"
        ),
        "output": "false",
    },
    {
        "name": "a file that is written is read back",
        "source": (
            "import package filesystem\n"
            "fn main() {\n"
            "    filesystem.write(\"note.txt\", \"hello\");\n"
            "    print(filesystem.read(\"note.txt\"));\n"
            "}\n"
        ),
        "output": "hello",
    },
    {
        "name": "a file that is written exists",
        "source": (
            "import package filesystem\n"
            "fn main() {\n"
            "    filesystem.write(\"note.txt\", \"hello\");\n"
            "    print(filesystem.exists(\"note.txt\"));\n"
            "}\n"
        ),
        "output": "true",
    },
    {
        "name": "a file that is written holds what was written into it",
        "source": (
            "import package filesystem\n"
            "fn main() {\n"
            "    filesystem.write(\"note.txt\", \"first\\nsecond\\n\");\n"
            "    print(filesystem.read(\"note.txt\"));\n"
            "}\n"
        ),
        "output": "first\nsecond",
    },
    {
        "name": "a directory that is created holds the file in it",
        "source": (
            "import package filesystem\n"
            "fn main() {\n"
            "    filesystem.mkdir(\"notes\");\n"
            "    filesystem.write(\"notes/note.txt\", \"hello\");\n"
            "    print(filesystem.list(\"notes\"));\n"
            "}\n"
        ),
        "output": "note.txt",
    },
    {
        "name": "a directory is created with the directories above it",
        "source": (
            "import package filesystem\n"
            "fn main() {\n"
            "    filesystem.mkdir(\"one/two/three\");\n"
            "    print(filesystem.exists(\"one/two/three\"));\n"
            "}\n"
        ),
        "output": "true",
    },
    {
        "name": "the names of a directory come in the order of the alphabet",
        "source": (
            "import package filesystem\n"
            "fn main() {\n"
            "    filesystem.mkdir(\"notes\");\n"
            "    filesystem.write(\"notes/c.txt\", \"c\");\n"
            "    filesystem.write(\"notes/a.txt\", \"a\");\n"
            "    filesystem.write(\"notes/b.txt\", \"b\");\n"
            "    print(filesystem.list(\"notes\"));\n"
            "}\n"
        ),
        "output": "a.txt\nb.txt\nc.txt",
    },
    {
        "name": "a file that cannot be read is reported",
        "source": (
            "import package filesystem\n"
            "fn main() { print(filesystem.read(\"pebble_missing.txt\")); }\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "filesystem.read cannot read 'pebble_missing.txt'",
        "exit_code": 1,
    },
    {
        "name": "a directory that cannot be read is reported",
        "source": (
            "import package filesystem\n"
            "fn main() {\n"
            "    filesystem.mkdir(\"notes\");\n"
            "    print(filesystem.read(\"notes\"));\n"
            "}\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "filesystem.read cannot read 'notes'",
        "exit_code": 1,
    },
    {
        "name": "a file that cannot be written is reported",
        "source": (
            "import package filesystem\n"
            "fn main() { filesystem.write(\"missing/note.txt\", \"hello\"); }\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "filesystem.write cannot write 'missing/note.txt'",
        "exit_code": 1,
    },
    {
        "name": "a directory that cannot be listed is reported",
        "source": (
            "import package filesystem\n"
            "fn main() { print(filesystem.list(\"pebble_missing\")); }\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "filesystem.list cannot list 'pebble_missing'",
        "exit_code": 1,
    },
    {
        "name": "a directory that cannot be created is reported",
        "source": (
            "import package filesystem\n"
            "fn main() {\n"
            "    filesystem.mkdir(\"notes\");\n"
            "    filesystem.mkdir(\"notes\");\n"
            "}\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "filesystem.mkdir cannot create 'notes'",
        "exit_code": 1,
    },
    {
        "name": "a path that is not a path is reported",
        "source": (
            "import package filesystem\n"
            "fn main() { print(filesystem.exists(\"\")); }\n"
        ),
        "output": None,
        "stage": "run",
        "stderr": "filesystem.exists needs a path",
        "exit_code": 1,
    },
    {
        "name": "a read without a path is rejected",
        "source": (
            "import package filesystem\n"
            "fn main() { print(filesystem.read()); }\n"
        ),
        "output": None,
        "stderr": "expects 1 argument(s), but got 0",
        "exit_code": 1,
    },
    {
        "name": "a write with a wrong argument type is rejected",
        "source": (
            "import package filesystem\n"
            "fn main() { filesystem.write(\"note.txt\", 42); }\n"
        ),
        "output": None,
        "stderr": "expects 'String', but got 'Int'",
        "exit_code": 1,
    },
    {
        "name": "an unknown filesystem function is rejected",
        "source": (
            "import package filesystem\n"
            "fn main() { print(filesystem.remove(\"note.txt\")); }\n"
        ),
        "output": None,
        "stderr": "has no function 'remove'",
        "exit_code": 1,
    },
    {
        "name": "Log:Print writes to the standard output",
        "source": (
            "import package Log\n"
            "fn main() { Log:Print(\"hello\"); }\n"
        ),
        "output": "hello",
        "expected_stderr": "",
    },
    {
        "name": "Log:Warning writes to the standard error output",
        "source": (
            "import package Log\n"
            "fn main() { Log:Warning(\"careful\"); }\n"
        ),
        "output": "",
        "expected_stderr": "Pebble warning: careful\n",
    },
    {
        "name": "Log:Error writes to the standard error output",
        "source": (
            "import package Log\n"
            "fn main() { Log:Error(\"failed\"); }\n"
        ),
        "output": "",
        "expected_stderr": "Pebble error: failed\n",
    },
    {
        "name": "the Log package keeps the two streams apart",
        "source": (
            "import package Log\n"
            "fn main() {\n"
            "    Log:Print(\"printed\")\n"
            "    Log:Warning(\"careful\")\n"
            "    Log:Error(\"failed\")\n"
            "}\n"
        ),
        "output": "printed",
        "expected_stderr": "Pebble warning: careful\nPebble error: failed\n",
    },
    {
        "name": "a Log message is not read as a format string",
        "source": (
            "import package Log\n"
            "fn main() { Log:Print(\"100% sure %d %s %n\"); }\n"
        ),
        "output": "100% sure %d %s %n",
        "expected_stderr": "",
    },
    {
        "name": "a Log message of the error output is not read as a format string",
        "source": (
            "import package Log\n"
            "fn main() { Log:Error(\"100% sure %d %s %n\"); }\n"
        ),
        "output": "",
        "expected_stderr": "Pebble error: 100% sure %d %s %n\n",
    },
    {
        "name": "an empty Log message writes an empty line",
        "source": (
            "import package Log\n"
            "fn main() { Log:Print(\"\"); }\n"
        ),
        "output": "",
        "expected_stderr": "",
    },
    {
        "name": "a Log message may come from another package",
        "source": (
            "import package Log\n"
            "import package string\n"
            "fn main() { Log:Print(string.upper(\"quiet\")); }\n"
        ),
        "output": "QUIET",
        "expected_stderr": "",
    },
    {
        "name": "an unknown Log function is rejected",
        "source": (
            "import package Log\n"
            "fn main() { Log:Info(\"hello\"); }\n"
        ),
        "output": None,
        "stderr": "has no function 'Info'",
        "exit_code": 1,
    },
    {
        "name": "a Log function keeps the case of its name",
        "source": (
            "import package Log\n"
            "fn main() { Log:print(\"hello\"); }\n"
        ),
        "output": None,
        "stderr": "has no function 'print'",
        "exit_code": 1,
    },
    {
        "name": "a Log function with a wrong argument type is rejected",
        "source": (
            "import package Log\n"
            "fn main() { Log:Print(42); }\n"
        ),
        "output": None,
        "stderr": "expects 'String', but got 'Int'",
        "exit_code": 1,
    },
    {
        "name": "a Log function with a wrong argument count is rejected",
        "source": (
            "import package Log\n"
            "fn main() { Log:Print(); }\n"
        ),
        "output": None,
        "stderr": "expects 1 argument(s), but got 0",
        "exit_code": 1,
    },
    {
        "name": "the Log package needs an import",
        "source": (
            "fn main() { Log:Print(\"hello\"); }\n"
        ),
        "output": None,
        "stderr": "Unknown package 'Log'",
        "exit_code": 1,
    },
    {
        "name": "the name of the Log package keeps its case",
        "source": (
            "import package log\n"
            "fn main() { log:print(\"hello\"); }\n"
        ),
        "output": None,
        "stderr": "Cannot find package 'log'",
        "exit_code": 1,
    },
]


def build_and_run(executable, source_path, workdir):
    built = invoke([str(executable), "build", str(source_path)], workdir)

    if built.returncode != 0:
        return None, built, None

    binary = workdir / "build" / (
        f"{source_path.stem}-{artifact_tag(str(source_path))}"
    )

    if not binary.is_file():
        raise AssertionError(f"native executable missing for {source_path}")

    return invoke([str(binary)], workdir), built, binary


def check_float_bounds(name, output, low, high):
    values = output.split()
    require(len(values) == 1, f"{name}: expected a single value, got {output!r}")

    try:
        value = float(values[0])
    except ValueError as error:
        raise AssertionError(f"{name}: {output!r} is not a number") from error

    require(
        low <= value <= high,
        f"{name}: {value} is outside the expected range [{low}, {high}]",
    )


def check_bounds(name, output, low, high):
    values = output.split()
    require(len(values) == 1, f"{name}: expected a single value, got {output!r}")

    try:
        value = int(values[0])
    except ValueError as error:
        raise AssertionError(f"{name}: {output!r} is not an integer") from error

    require(
        low <= value <= high,
        f"{name}: {value} is outside the expected range [{low}, {high}]",
    )


def main():
    require(len(sys.argv) == 3, "expected C++ and C executable paths")

    cpp_executable = Path(sys.argv[1]).resolve()
    c_executable = Path(sys.argv[2]).resolve()

    with tempfile.TemporaryDirectory(prefix="pebble-package-parity-") as temporary:
        root = Path(temporary)
        source_dir = root / "sources"
        cpp_dir = root / "cpp"
        c_dir = root / "c"

        for directory in (source_dir, cpp_dir, c_dir):
            directory.mkdir()

        for index, case in enumerate(CASES):
            name = case["name"]
            stage = case.get("stage", "build")
            source_path = source_dir / f"case_{index}.peb"
            source_path.write_text(case["source"], encoding="utf-8")

            results = []
            binaries = []

            for label, executable, base in (
                ("C++", cpp_executable, cpp_dir),
                ("C", c_executable, c_dir),
            ):
                workdir = base / f"case_{index}"
                workdir.mkdir()

                executed, built, binary = build_and_run(
                    executable, source_path, workdir
                )
                binaries.append(binary)

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

            if case.get("compare") == "bounds":
                cpp_lines = cpp_output.splitlines()
                c_lines = c_output.splitlines()

                require(
                    len(cpp_lines) == len(c_lines),
                    f"{name}: the frontends printed a different number of "
                    f"values\nC++: {cpp_output!r}\nC: {c_output!r}",
                )

                if "float_range" in case:
                    low, high = case["float_range"]
                    check = check_float_bounds
                else:
                    low, high = case["range"]
                    check = check_bounds

                for line in cpp_lines:
                    check(name, line, low, high)

                for line in c_lines:
                    check(name, line, low, high)
            else:
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

            if "expected_stderr" in case:
                require(
                    results[0].stderr == case["expected_stderr"],
                    f"{name}: unexpected diagnostics\n"
                    f"Expected: {case['expected_stderr']!r}\n"
                    f"Actual: {results[0].stderr!r}",
                )

            if case.get("repeat"):
                for label, binary in (("C++", binaries[0]), ("C", binaries[1])):
                    repeated = invoke([str(binary)], binary.parent.parent)

                    require(
                        repeated.stdout == results[0 if label == "C++" else 1].stdout,
                        f"{name}: {label} is not reproducible\n"
                        f"First: {results[0 if label == 'C++' else 1].stdout!r}\n"
                        f"Second: {repeated.stdout!r}",
                    )

            if stage == "run":
                print(f"PASS: {name}")
                continue

            if case.get("output") is not None:
                require(
                    cpp_output == case["output"],
                    f"{name}: unexpected output\n"
                    f"Expected: {case['output']!r}\n"
                    f"Actual: {cpp_output!r}",
                )

            if "range" in case and case.get("compare") != "bounds":
                low, high = case["range"]
                for line in cpp_output.splitlines():
                    check_bounds(name, line, low, high)

            print(f"PASS: {name}")

    print(f"Package runtime parity: {len(CASES)} cases passed")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
