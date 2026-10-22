# The Pebble language

A Pebble program is a list of function declarations. Every program has a
`main` function, which is the entry point of the executable.

```pebble
fn main() {
    print("Hello, Pebble!")
}
```

A statement is ended by a line break. The semicolon is an optional separator
between two statements, never a mandatory terminator, so the two blocks below
mean the same thing:

```pebble
fn main() {
    print("Hello, Pebble!");
}
```

```pebble
fn main() {
    print("Hello, Pebble!")
}
```

A semicolon on its own is an empty statement. It carries nothing and it never
makes a program fail, so a program that was written with semicolons keeps
working.

## Statements

A statement is one of the following:

| Statement | Written like this |
| --- | --- |
| A declaration | `let name = "Pebble"` |
| A declaration | `var count: Int = 0` |
| An assignment | `count = count + 1` |
| A call | `print(count)` |
| A return | `return count` |
| A conditional | `if count > 0 { print(count) } else { print(0) }` |
| A loop | `while count < 3 { count = count + 1 }` |
| A block | `{ print(count) }` |
| An import | `import package math` |
| A function | `fn add(a: Int, b: Int) -> Int { return a + b }` |
| An empty statement | `;` |

Every statement is ended by a line break. The semicolon is an optional
separator between two statements that stand on one line, so a program never
needs one. A whole program without a single semicolon looks like this:

```pebble
import package math

fn add(a: Int, b: Int) -> Int {
    return a + b
}

fn main() {
    let x = 10
    let y = 20
    let result = add(x, y)

    if result > 20 {
        print("Result is greater than 20")
    } else {
        print("Result is 20 or less")
    }

    print(math.max(result, 100))
}
```

The language is statically typed and every value has one of the types `Int`,
`Float`, `Bool`, `String` or `Void`.

## Values and types

| Type | Literals | Notes |
| --- | --- | --- |
| `Int` | `42`, `-7` | A signed 64 bit integer. The smallest value is `-9223372036854775808`. |
| `Float` | `3.5`, `-0.25` | A double precision number. |
| `Bool` | `true`, `false` | |
| `String` | `"hello"` | A UTF-8 string. `"a" + "b"` concatenates two strings. |
| `Void` | | The type of a function that returns nothing. |

An integer literal that does not fit into an `Int` is rejected with a
diagnostic. An `Int` is widened to a `Float` where a `Float` is expected, but
a `Float` is never narrowed to an `Int` automatically.

```pebble
fn widen(value: Float) -> Float {
    return value
}

fn main() {
    print(widen(3))    // 3 is widened to 3.0
}
```

## Variables

`let` declares an immutable variable, `var` a mutable one. A type
annotation is optional; without one the type is taken from the value.

```pebble
fn main() {
    let name: String = "Pebble"
    var count = 0

    count = count + 1
    print(name, count)
}
```

Assigning to a `let` variable is rejected. A variable that is declared inside
a block only lives inside that block.

## Functions

A function has a name, a list of parameters, an optional return type and a
body.

```pebble
fn add(left: Int, right: Int) -> Int {
    return left + right
}
```

Every parameter carries its type. A parameter without a type is reported with
`Parameter 'left' needs an explicit type annotation.`

The return type is optional. A function without a return type returns `Void`.
A function that returns a value needs a `return` statement on every path,
otherwise the compiler reports `The function 'name' must return a value.`.

```pebble
fn greet(who: String) {
    print("Hello,", who)
}
```

A function is called with its name and its arguments. The number of the
arguments and their types are checked.

```pebble
fn add(left: Int, right: Int) -> Int {
    return left + right
}

fn greet(who: String) {
    print("Hello,", who)
}

fn main() {
    let total = add(10, 20)
    print(total)
    greet("Pebble")
}
```

The value of a `return` stands on the line of the keyword. A `return` on a
line of its own returns nothing.

## Control flow

```pebble
fn classify(value: Int) -> String {
    if value < 0 {
        return "negative"
    } else if value == 0 {
        return "zero"
    }

    return "positive"
}

fn count_to(limit: Int) {
    var current = 0

    while current < limit {
        print(current)
        current = current + 1
    }
}
```

The condition of an `if` or a `while` statement has to be a `Bool`.

## Operators

| Operator | Meaning | Operand types |
| --- | --- | --- |
| `+` | addition, concatenation | two numbers, or two strings |
| `-`, `*`, `/`, `%` | arithmetic | two numbers, `%` needs two `Int` |
| `==`, `!=` | equality | two values of the same type, or two numbers |
| `<`, `<=`, `>`, `>=` | comparison | two numbers |
| `&&`, `\|\|` | logic | two `Bool` |
| `!` | negation | one `Bool` |
| `=` | assignment | a mutable variable |

## Line breaks

A line break ends a statement. It does not end an expression that is not
finished, so an expression may be written over several lines.

An expression is not finished when one of these is true:

* it stands inside a parenthesis that is not closed yet,
* the line ends with a binary operator or with the `=` of an assignment, so
  the expression still waits for its right operand,
* the next line begins with a `.` or with a `:` that belongs to the name
  above it.

| Written like this | It means |
| --- | --- |
| `let total = add(10, 20)` | one statement |
| `let total = add(`<br>`    10,`<br>`    20`<br>`)` | one statement: the call is not finished until the parenthesis is closed |
| `let total = a +`<br>`    b` | one statement: the operator asks for its right operand |
| `let total = a`<br>`    + b` | a mistake: a binary operator cannot start a line |
| `let total = compute`<br>`    (1 + 2)` | two statements: a parenthesis that starts a line does not turn into a call |
| `let value = math`<br>`    .max(3, 7)` | one statement: the member belongs to the name above it |
| `let value = math`<br>`    :max(3, 7)` | one statement: the member belongs to the name above it |

A binary operator continues the expression above it only when it stands at the
end of that line. A line that begins with a binary operator is reported with
`Unexpected '+'. A binary operator cannot start a line; it stands at the end
of the line of the expression it continues.` The unary operators `-` and `!`
may start a statement, so `let total = a` on one line and `- 2` on the next
are two statements.

The `=` of an assignment stands on the line of the name it assigns to, and
the value of a `return` stands on the line of the keyword. A `return` on a
line of its own returns nothing.

```pebble
fn add(left: Int, right: Int) -> Int {
    return left + right
}

fn double(value: Int) -> Int {
    var result = value
    result = result +
        2

    return result
}
```

Two statements that stand on one line need a semicolon between them, because
a line break is what separates them. The report is
`Expected a line break or ';' after the variable declaration.`

Blank lines and comments do not change the meaning of a program. A comment
runs from the `//` to the end of its line.

## Printing

`print` is built into the language. It takes any number of values and writes
them to the standard output, separated by a space, followed by a newline.

```pebble
fn main() {
    print(1, 2.5, "three", true)
}
```

## Packages

A program imports a package with `import package <name>`. This is the
canonical form of an import. The shorthand `import <name>` is kept, so that a
program that was written with it keeps working.

```pebble
import package math

fn main() {
    print(math.max(3, 7))
}
```

The functions of a package are called with the package name in front of them,
separated by a dot or by a colon. The two lines below call the same function.

```pebble
import package math

fn main() {
    print(math.max(3, 7))
    print(math:max(3, 7))
}
```

The separator may also stand on the line below the package name, so the call
below is the same call again.

```pebble
import package math

fn main() {
    print(math
        .max(3, 7))
}
```

A package that is written for a project exports the functions that other
files may use.

```pebble
export fn add(a: Int, b: Int) -> Int {
    return a + b
}
```

The rules of the package system, the standard library and the manifest format
are described in [packages.md](packages.md).

## Errors

Every problem is reported with the file, the line and the column of the
offending code, for example:

```
main.peb:4:9: error: Cannot assign a value of type 'String' to a variable of type 'Int'.
```

The compiler reports as many problems as it can find and stops before the
program is built. The most common diagnostics are:

| Situation | Message |
| --- | --- |
| The name is not known | `Undeclared name 'value'.` |
| The types do not match | `Cannot initialise 'count' of type 'Int' with a value of type 'String'.` |
| A `let` variable is changed | `Cannot assign to immutable variable 'count'.` |
| The condition is not a `Bool` | `If condition must have type 'Bool', not 'Int'.` |
| The function does not return | `The function 'answer' must return a value.` |
| The return type is wrong | `Return type mismatch: expected 'Int' but got 'String'.` |
| The function does not exist | `Call to undeclared function 'greet'.` |
| The argument does not match | `Argument 2 of 'add' expects 'Int', but got 'String'.` |
| The argument is missing | `Function 'add' expects 2 argument(s), but got 1.` |
| The parameter has no type | `Parameter 'value' needs an explicit type annotation.` |
| The type is unknown | `Unknown type 'Banana'.` |
| The integer is too large | `Integer literal '99999999999999999999' is out of range for type 'Int'.` |
| The operator is used wrongly | `Operator '%' requires Int operands.` |
| A statement is not ended | `Expected a line break or ';' after the expression.` |
| A binary operator starts a line | `Unexpected '+'. A binary operator cannot start a line; it stands at the end of the line of the expression it continues.` |
| An assignment starts a line | `Unexpected '='. The '=' of an assignment stands on the line of the name it assigns to.` |
| An import has no name | `Expected a package name after 'import'.` |

The two frontends report the same problems in the same order.
