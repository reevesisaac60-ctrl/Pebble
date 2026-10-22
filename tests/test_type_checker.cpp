#include "pebble/lexer.hpp"
#include "pebble/parser.hpp"
#include "pebble/type_checker.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

pebble::TypeCheckResult check(std::string text) {
    const pebble::Source source{"types.peb", std::move(text)};
    const pebble::LexResult lexed = pebble::Lexer{}.scan(source);
    const pebble::ParseResult parsed = pebble::Parser{}.parse(lexed);

    if (!parsed.ok() || !parsed.root) {
        std::cerr << "Test setup failed during parsing.\n";
        std::exit(EXIT_FAILURE);
    }

    return pebble::TypeChecker{}.check(*parsed.root);
}

bool containsMessage(
    const pebble::TypeCheckResult& result,
    std::string_view fragment
) {
    for (const pebble::Diagnostic& diagnostic : result.diagnostics) {
        if (diagnostic.message.find(fragment) != std::string::npos) {
            return true;
        }
    }

    return false;
}

void testValidProgram() {
    const auto result = check(
        "fn add(a: Int, b: Int) -> Int { return a + b; }\n"
        "fn main() {\n"
        "    let title: String = \"Pebble\";\n"
        "    var score = add(20, 22);\n"
        "    score = score + 1;\n"
        "    print(title, score);\n"
        "}\n"
    );

    require(result.ok(), "valid typed program should pass");
}

void testRejectsIncompatibleInitializer() {
    const auto result = check(
        "fn main() { let score: Int = \"not a number\"; }"
    );

    require(!result.ok(), "String cannot initialise an Int");
    require(
        containsMessage(result, "Cannot initialise"),
        "initializer error should explain the mismatch"
    );
}

void testRejectsInferredTypeChange() {
    const auto result = check(
        "fn main() { let score = 42; score = \"hello\"; }"
    );

    require(!result.ok(), "inferred variable type must remain fixed");
    require(
        containsMessage(result, "immutable variable"),
        "assignment to let should be rejected"
    );
}

void testMutableAssignmentStillTypeChecked() {
    const auto result = check(
        "fn main() { var score = 42; score = \"hello\"; }"
    );

    require(!result.ok(), "var should not permit a type change");
    require(
        containsMessage(result, "Cannot assign a value"),
        "type mismatch on mutable assignment should be reported"
    );
}

void testUndeclaredName() {
    const auto result = check(
        "fn main() { print(missingValue); }"
    );

    require(!result.ok(), "undeclared names should fail");
    require(
        containsMessage(result, "Undeclared name"),
        "diagnostic should identify the missing name"
    );
}

void testFunctionCallArguments() {
    const auto result = check(
        "fn greet(name: String) { print(name); }\n"
        "fn main() { greet(42); }"
    );

    require(!result.ok(), "wrong argument type should fail");
    require(
        containsMessage(result, "expects 'String'"),
        "diagnostic should identify expected argument type"
    );
}

void testReturnType() {
    const auto result = check(
        "fn answer() -> Int { return \"wrong\"; }"
    );

    require(!result.ok(), "incorrect return type should fail");
    require(
        containsMessage(result, "Return type mismatch"),
        "diagnostic should describe the return type mismatch"
    );
}

void testFunctionWithoutReturnIsRejected() {
    const auto result = check("fn answer() -> Int { }");

    require(!result.ok(), "a function without a return should fail");
    require(
        containsMessage(result, "The function 'answer' must return a value."),
        "diagnostic should describe the missing return"
    );
}

void testFunctionWithReturnOnEveryPathIsAccepted() {
    const auto result = check(
        "fn classify(value: Int) -> Int {\n"
        "    if value > 0 {\n"
        "        return 1;\n"
        "    } else {\n"
        "        return 0;\n"
        "    }\n"
        "}\n"
        "fn main() { print(classify(1)); }\n"
    );

    require(result.ok(), "a function that returns on every path should pass");
}

void testVoidFunctionWithoutReturnIsAccepted() {
    const auto result = check("fn greet() { print(\"hi\"); }");

    require(result.ok(), "a function without a value should pass");
}

void testNullLiteralIsAccepted() {
    const auto result = check(
        "fn main() { let empty = null; print(empty); }"
    );

    require(result.ok(), "the null literal should be accepted");
}

void testNullValuesAreComparable() {
    const auto result = check(
        "fn main() { let empty = null; print(empty == null, empty != null); }"
    );

    require(result.ok(), "two null values should be comparable");
}

void testNullCannotBeUsedAsAString() {
    const auto result = check(
        "fn main() { let text: String = null; }"
    );

    require(!result.ok(), "a null value should not be a string");
    require(
        containsMessage(result, "with a value of type 'Null'"),
        "diagnostic should report the type of the value"
    );
}

void testNullTypeCannotBeWritten() {
    const auto result = check(
        "fn main() { let empty: Null = null; }"
    );

    require(!result.ok(), "the null type should not be a type name");
    require(
        containsMessage(result, "Unknown type 'Null'"),
        "diagnostic should report the unknown type"
    );
}

void testNullCannotBeUsedAsACondition() {
    const auto result = check(
        "fn main() { if null { print(1); } }"
    );

    require(!result.ok(), "a null value should not be a condition");
    require(
        containsMessage(result, "If condition must have type 'Bool', not 'Null'"),
        "diagnostic should report the type of the condition"
    );
}

void testNullCannotBeComparedWithAString() {
    const auto result = check(
        "fn main() { print(null == \"text\"); }"
    );

    require(!result.ok(), "a null value should not be compared with a string");
    require(
        containsMessage(result, "Cannot compare values of type 'Null' and 'String'"),
        "diagnostic should report the types of the operands"
    );
}

void testUnknownType() {
    const auto result = check(
        "fn main() { let item: Banana = 1; }"
    );

    require(!result.ok(), "unknown type names should fail");
    require(
        containsMessage(result, "Unknown type"),
        "diagnostic should identify the unknown type"
    );
}

void testNumericWidening() {
    const auto result = check(
        "fn main() { let value: Float = 12; var other: Float = 2.5; other = value; }"
    );

    require(result.ok(), "Int to Float widening should be allowed");
}

void testControlFlowConditionsMustBeBool() {
    const auto ifResult = check(
        "fn main() { if 42 { print(1); } }"
    );

    require(!ifResult.ok(), "if must reject a non-Bool condition");
    require(containsMessage(ifResult, "If condition must have type 'Bool'"),
            "if condition diagnostic should explain the required type");

    const auto whileResult = check(
        "fn main() { while 1 { print(1); } }"
    );

    require(!whileResult.ok(), "while must reject a non-Bool condition");
    require(containsMessage(whileResult, "While condition must have type 'Bool'"),
            "while condition diagnostic should explain the required type");
}

void testControlFlowScopes() {
    const auto result = check(
        "fn main() {\n"
        "    if true { let hidden = 1; }\n"
        "    print(hidden);\n"
        "}"
    );

    require(!result.ok(), "branch-local variables must not escape their block");
    require(containsMessage(result, "Undeclared name 'hidden'"),
            "scope diagnostic should identify the branch-local variable");
}

void testValidControlFlow() {
    const auto result = check(
        "fn main() {\n"
        "    var count: Int = 0;\n"
        "    while count < 3 {\n"
        "        if count == 1 { print(1); } else { print(count); }\n"
        "        count = count + 1;\n"
        "    }\n"
        "}"
    );

    require(result.ok(), "valid nested control flow should type-check");
}

void testWrongFunctionArity() {
    const auto result = check(
        "fn add(a: Int, b: Int) -> Int { return a + b; }\n"
        "fn main() { let result = add(1); }"
    );

    require(!result.ok(), "incorrect function argument count should fail");
    require(
        containsMessage(result, "expects 2 argument"),
        "diagnostic should report expected argument count"
    );
}

void testNewlineTerminatedProgram() {
    const auto result = check(
        "import package math\n"
        "\n"
        "fn add(a: Int, b: Int) -> Int {\n"
        "    return a + b\n"
        "}\n"
        "\n"
        "fn main() {\n"
        "    let total = add(10, 20)\n"
        "    var score = total\n"
        "    score = score + 1\n"
        "    if score > 30 {\n"
        "        print(math.max(score, 0))\n"
        "    }\n"
        "}\n"
    );

    require(result.ok(), "a newline terminated program should type-check");
}

void testTheShorthandImportTypeChecks() {
    const auto result = check(
        "import math\n"
        "fn main() {\n"
        "    print(math.max(3, 7))\n"
        "}\n"
    );

    require(result.ok(), "the shorthand import should type-check");
}

void testTheReturnValueNeedsItsType() {
    const auto result = check(
        "fn add(a: Int, b: Int) -> Int {\n"
        "    return \"wrong\"\n"
        "}\n"
        "fn main() {\n"
        "    print(add(1, 2))\n"
        "}\n"
    );

    require(!result.ok(), "a return of the wrong type should be rejected");
    require(
        containsMessage(result, "Return type mismatch"),
        "diagnostic should report the return type mismatch"
    );
}

}

int main() {
    testValidProgram();
    testRejectsIncompatibleInitializer();
    testRejectsInferredTypeChange();
    testMutableAssignmentStillTypeChecked();
    testUndeclaredName();
    testFunctionCallArguments();
    testReturnType();
    testFunctionWithoutReturnIsRejected();
    testFunctionWithReturnOnEveryPathIsAccepted();
    testVoidFunctionWithoutReturnIsAccepted();
    testNullLiteralIsAccepted();
    testNullValuesAreComparable();
    testNullCannotBeUsedAsAString();
    testNullTypeCannotBeWritten();
    testNullCannotBeUsedAsACondition();
    testNullCannotBeComparedWithAString();
    testUnknownType();
    testNumericWidening();
    testControlFlowConditionsMustBeBool();
    testControlFlowScopes();
    testValidControlFlow();
    testWrongFunctionArity();
    testNewlineTerminatedProgram();
    testTheShorthandImportTypeChecks();
    testTheReturnValueNeedsItsType();

    std::cout << "All type checker tests passed.\n";
    return EXIT_SUCCESS;
}
