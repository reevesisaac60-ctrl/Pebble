#include "pebble/lexer.hpp"
#include "pebble/parser.hpp"

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

pebble::ParseResult parse(std::string text) {
    const pebble::Source source{"test.peb", std::move(text)};
    const pebble::LexResult lexed = pebble::Lexer{}.scan(source);
    return pebble::Parser{}.parse(lexed);
}

void testFunctionAndDeclarations() {
    auto result = parse(
        "fn main() {\n"
        "    let name: String = \"Pebble\";\n"
        "    var score: Int = 40 + 2 * 3;\n"
        "    score = score + 1;\n"
        "    print(name, score);\n"
        "    return;\n"
        "}\n"
    );

    require(result.ok(), "function with declarations should parse");
    require(result.root->kind == pebble::NodeKind::Program,
            "root should be a program node");
    require(result.root->children.size() == 1,
            "program should contain one function");

    const pebble::Node& function = *result.root->children[0];

    require(function.kind == pebble::NodeKind::Function,
            "top-level declaration should be a function");
    require(function.text == "main", "function name should be preserved");
    require(function.children.size() == 1,
            "function should contain its body");

    const pebble::Node& body = *function.children[0];

    require(body.kind == pebble::NodeKind::Block,
            "function body should be a block");
    require(body.children.size() == 5,
            "all five statements should be in the body");

    require(body.children[0]->kind == pebble::NodeKind::LetDeclaration,
            "let declaration should be represented in the AST");
    require(body.children[1]->kind == pebble::NodeKind::VarDeclaration,
            "var declaration should be represented in the AST");
    require(body.children[2]->kind == pebble::NodeKind::ExpressionStatement,
            "assignment should be represented as a statement");
    require(body.children[3]->children[0]->kind == pebble::NodeKind::CallExpression,
            "function calls should be represented in the AST");
    require(body.children[4]->kind == pebble::NodeKind::ReturnStatement,
            "return statement should be represented in the AST");
}

void testOperatorPrecedence() {
    auto result = parse("fn main() { let x = 1 + 2 * 3; }");

    require(result.ok(), "arithmetic expression should parse");

    const pebble::Node& body = *result.root->children[0]->children[0];
    const pebble::Node& declaration = *body.children[0];

    require(declaration.children.size() == 1,
            "unannotated variable should contain its initializer");
    require(declaration.children[0]->kind == pebble::NodeKind::BinaryExpression,
            "addition should form the outer expression");
    require(declaration.children[0]->text == "+",
            "addition should have lower precedence than multiplication");
    require(declaration.children[0]->children[1]->kind ==
                pebble::NodeKind::BinaryExpression,
            "multiplication should be nested on the right");
    require(declaration.children[0]->children[1]->text == "*",
            "multiplication operator should be preserved");
}

void testParametersAndReturnType() {
    auto result = parse(
        "fn add(a: Int, b: Int) -> Int { return a + b; }"
    );

    require(result.ok(), "typed parameters and return type should parse");

    const pebble::Node& function = *result.root->children[0];

    require(function.children.size() == 4,
            "function should contain two parameters, return type and body");
    require(function.children[0]->kind == pebble::NodeKind::Parameter,
            "first child should be a parameter");
    require(function.children[2]->kind == pebble::NodeKind::TypeName,
            "return type should be represented explicitly");
    require(function.children[2]->text == "Int",
            "return type name should be preserved");
}

void testMalformedSyntax() {
    auto result = parse("fn main( { let x = ; }");

    require(!result.ok(), "malformed syntax should fail parsing");
    require(!result.diagnostics.empty(),
            "parser should report at least one diagnostic");
    require(result.diagnostics[0].location.file == "test.peb",
            "syntax diagnostic should identify its source file");
}

void testControlFlowStatements() {
    auto result = parse(
        "fn main() {\n"
        "    var count: Int = 0;\n"
        "    while count < 3 {\n"
        "        if count == 1 { print(1); }\n"
        "        else if count == 2 { print(2); }\n"
        "        else { print(count); }\n"
        "        count = count + 1;\n"
        "    }\n"
        "}\n"
    );

    require(result.ok(), "if, else-if, else and while should parse");

    const pebble::Node& body = *result.root->children[0]->children[0];
    require(body.children.size() == 2, "loop should be a body statement");
    require(body.children[1]->kind == pebble::NodeKind::WhileStatement,
            "while should have its own AST node");

    const pebble::Node& loopBody = *body.children[1]->children[1];
    require(loopBody.children[0]->kind == pebble::NodeKind::IfStatement,
            "if should have its own AST node");
    const pebble::Node& branch = *loopBody.children[0];
    require(branch.children.size() == 3,
            "if with else-if should preserve all branches");
    require(branch.children[2]->kind == pebble::NodeKind::IfStatement,
            "else-if should nest another if node");
}

void testInvalidAssignmentTarget() {
    auto result = parse("fn main() { (1 + 2) = 3; }");

    require(!result.ok(), "non-variable assignment target should be rejected");
}


void testNewlineTerminatedStatements() {
    auto result = parse(
        "import package math\n"
        "\n"
        "fn add(a: Int, b: Int) -> Int {\n"
        "    return a + b\n"
        "}\n"
        "\n"
        "fn main() {\n"
        "    let total = add(10, 20)\n"
        "    print(total)\n"
        "    total = total + 1\n"
        "    return\n"
        "}\n"
    );

    require(result.ok(), "newline terminated statements should parse");
    require(result.root->children.size() == 3,
            "program should hold two imports and a function");

    const pebble::Node& main = *result.root->children[2];

    require(main.kind == pebble::NodeKind::Function,
            "the third declaration should be a function");

    const pebble::Node& body = *main.children[0];

    require(body.children.size() == 4,
            "the body should hold four statements");
    require(body.children[0]->kind == pebble::NodeKind::LetDeclaration,
            "the first statement should be a declaration");
    require(body.children[1]->kind == pebble::NodeKind::ExpressionStatement,
            "the second statement should be a call");
    require(body.children[2]->kind == pebble::NodeKind::ExpressionStatement,
            "the third statement should be an assignment");
    require(body.children[2]->children[0]->kind ==
                pebble::NodeKind::Assignment,
            "the third statement should hold the assignment");
    require(body.children[3]->kind == pebble::NodeKind::ReturnStatement,
            "the fourth statement should be a return");
    require(body.children[3]->children.empty(),
            "a return without a value should carry no expression");
}

void testSemicolonsStayOptional() {
    auto withSemicolons = parse(
        "fn main() {\n"
        "    let value = 1;\n"
        "    print(value);\n"
        "}\n"
    );

    auto withoutSemicolons = parse(
        "fn main() {\n"
        "    let value = 1\n"
        "    print(value)\n"
        "}\n"
    );

    require(withSemicolons.ok(), "a program with semicolons should parse");
    require(withoutSemicolons.ok(), "a program without semicolons should parse");
    require(
        withSemicolons.root->children[0]->children[0]->children.size() ==
            withoutSemicolons.root->children[0]->children[0]->children.size(),
        "both programs should hold the same number of statements"
    );
}

void testAStatementNeedsATerminator() {
    auto result = parse("fn main() { let value = 1 let other = 2 }");

    require(!result.ok(),
            "two statements on one line without a semicolon should fail");
    require(!result.diagnostics.empty(),
            "the missing terminator should be reported");
}

void testAnOperatorOnTheNextLineStartsANewStatement() {
    auto result = parse(
        "fn main() {\n"
        "    let value = 1\n"
        "    - 2\n"
        "}\n"
    );

    require(result.ok(), "an operator on its own line should not fail");
    require(result.diagnostics.empty(),
            "an operator on its own line should not be reported");

    const pebble::Node& body = *result.root->children[0]->children[0];

    require(body.children.size() == 2,
            "the operator should start a statement of its own");
    require(body.children[0]->children[0]->kind ==
                pebble::NodeKind::LiteralExpression,
            "the first statement should hold the literal alone");
    require(body.children[1]->kind == pebble::NodeKind::ExpressionStatement,
            "the second statement should be an expression");
    require(body.children[1]->children[0]->kind ==
                pebble::NodeKind::UnaryExpression,
            "the second statement should hold a unary expression");
}

void testAParenthesisOnTheNextLineDoesNotContinueACall() {
    auto result = parse(
        "fn main() {\n"
        "    let value = compute\n"
        "    (1 + 2)\n"
        "}\n"
    );

    require(result.ok(), "a parenthesis on the next line should not fail");

    const pebble::Node& body = *result.root->children[0]->children[0];

    require(body.children.size() == 2,
            "the parenthesis should start a statement of its own");
    require(body.children[0]->children[0]->kind ==
                pebble::NodeKind::NameExpression,
            "the first statement should hold the name alone");
    require(body.children[1]->kind == pebble::NodeKind::ExpressionStatement,
            "the second statement should be an expression");
}

void testMultilineExpressionsContinue() {
    auto result = parse(
        "fn main() {\n"
        "    let total = add(\n"
        "        10,\n"
        "        20\n"
        "    )\n"
        "    let doubled = total +\n"
        "        total\n"
        "    print(doubled)\n"
        "}\n"
    );

    require(result.ok(), "expressions over several lines should parse");

    const pebble::Node& body = *result.root->children[0]->children[0];

    require(body.children.size() == 3, "the body should hold three statements");
    require(body.children[0]->children[0]->kind ==
                pebble::NodeKind::CallExpression,
            "the call should be one expression");
    require(body.children[0]->children[0]->children.size() == 3,
            "the call should hold the name and the two arguments");
    require(body.children[1]->children[0]->kind ==
                pebble::NodeKind::BinaryExpression,
            "the addition should be one expression");
}

void testBlankLinesAndCommentsDoNotChangeTheProgram() {
    auto plain = parse(
        "fn main() {\n"
        "    let value = 1\n"
        "    print(value)\n"
        "}\n"
    );

    auto withGaps = parse(
        "// a comment before the function\n"
        "\n"
        "fn main() {\n"
        "\n"
        "    // a comment before the statement\n"
        "    let value = 1\n"
        "\n"
        "    print(value) // a comment after the statement\n"
        "\n"
        "}\n"
        "\n"
        "// a comment after the function\n"
    );

    require(plain.ok(), "the plain program should parse");
    require(withGaps.ok(), "the program with comments should parse");

    const pebble::Node& plainBody = *plain.root->children[0]->children[0];
    const pebble::Node& gapBody = *withGaps.root->children[0]->children[0];

    require(plainBody.children.size() == gapBody.children.size(),
            "the comments should not change the number of statements");
}

void testNestedBlocksAreDelimited() {
    auto result = parse(
        "fn main() {\n"
        "    if true {\n"
        "        while false {\n"
        "            let value = 1\n"
        "        }\n"
        "    }\n"
        "    print(1)\n"
        "}\n"
    );

    require(result.ok(), "nested blocks should parse");

    const pebble::Node& body = *result.root->children[0]->children[0];

    require(body.children.size() == 2, "the body should hold two statements");
    require(body.children[0]->kind == pebble::NodeKind::IfStatement,
            "the first statement should be an if");

    const pebble::Node& branch = *body.children[0]->children[1];

    require(branch.kind == pebble::NodeKind::Block, "the branch should be a block");
    require(branch.children[0]->kind == pebble::NodeKind::WhileStatement,
            "the block should hold the while");

    const pebble::Node& loop = *branch.children[0]->children[1];

    require(loop.kind == pebble::NodeKind::Block, "the loop should be a block");
    require(loop.children[0]->kind == pebble::NodeKind::LetDeclaration,
            "the loop should hold the declaration");
    require(body.children[1]->kind == pebble::NodeKind::ExpressionStatement,
            "the statement after the blocks should be a call");
}

void testTheCanonicalImportAndItsShorthand() {
    auto canonical = parse(
        "import package math\n"
        "import package log\n"
        "fn main() { print(1) }\n"
    );

    auto shorthand = parse(
        "import math\n"
        "import log\n"
        "fn main() { print(1) }\n"
    );

    require(canonical.ok(), "the canonical import should parse");
    require(shorthand.ok(), "the shorthand import should parse");
    require(canonical.root->children.size() == 3,
            "the program should hold two imports and a function");
    require(canonical.root->children[0]->kind ==
                pebble::NodeKind::ImportDeclaration,
            "an import should be represented in the AST");
    require(canonical.root->children[0]->text == "math",
            "the name of the package should be preserved");
    require(canonical.root->children[1]->text == "log",
            "the name of the second package should be preserved");
    require(shorthand.root->children[0]->kind ==
                pebble::NodeKind::ImportDeclaration,
            "the shorthand should be an import as well");
    require(shorthand.root->children[0]->text == "math",
            "the shorthand should carry the same name");
}

void testAnImportNeedsAName() {
    auto result = parse("import package\nfn main() { print(1) }\n");

    require(!result.ok(), "an import without a name should fail");
}

void testAFunctionWithACallAndAReturn() {
    auto result = parse(
        "fn add(a: Int, b: Int) -> Int {\n"
        "    let sum = a + b\n"
        "    return sum\n"
        "}\n"
        "\n"
        "fn main() {\n"
        "    let result = add(10, 20)\n"
        "    print(result)\n"
        "}\n"
    );

    require(result.ok(), "a function with a call and a return should parse");
    require(result.root->children.size() == 2,
            "the program should hold two functions");

    const pebble::Node& add = *result.root->children[0];

    require(add.kind == pebble::NodeKind::Function, "the first is a function");
    require(add.text == "add", "the name of the function should be preserved");
    require(add.children.size() == 4,
            "the function should hold two parameters, a type and a body");

    const pebble::Node& body = *add.children[3];

    require(body.children.size() == 2, "the body should hold two statements");
    require(body.children[0]->kind == pebble::NodeKind::LetDeclaration,
            "the first statement should be a declaration");
    require(body.children[1]->kind == pebble::NodeKind::ReturnStatement,
            "the second statement should be a return");
    require(body.children[1]->children.size() == 1,
            "the return should carry its value");
    require(body.children[1]->children[0]->kind ==
                pebble::NodeKind::NameExpression,
            "the return should carry the name of the variable");

    const pebble::Node& main = *result.root->children[1];
    const pebble::Node& mainBody = *main.children[0];

    require(mainBody.children[0]->children[0]->kind ==
                pebble::NodeKind::CallExpression,
            "the call should be the value of the declaration");
    require(mainBody.children[0]->children[0]->children.size() == 3,
            "the call should hold the name and the two arguments");
}

void testTheValueOfAReturnStandsOnItsLine() {
    auto result = parse(
        "fn main() {\n"
        "    return\n"
        "    1\n"
        "}\n"
    );

    require(result.ok(), "a return without a value should parse");

    const pebble::Node& body = *result.root->children[0]->children[0];

    require(body.children.size() == 2,
            "the number after the return should be a statement of its own");
    require(body.children[0]->kind == pebble::NodeKind::ReturnStatement,
            "the first statement should be the return");
    require(body.children[0]->children.empty(),
            "the return should carry no value");
    require(body.children[1]->kind == pebble::NodeKind::ExpressionStatement,
            "the second statement should be an expression");
}


void testAStraySemicolonIsAnEmptyStatement() {
    auto plain = parse(
        "fn main() {\n"
        "    let value = 1\n"
        "    print(value)\n"
        "}\n"
    );

    auto withSemicolons = parse(
        "fn main() {\n"
        "    let value = 1;\n"
        "    print(value);\n"
        "};\n"
        ";\n"
    );

    require(plain.ok(), "the plain program should parse");
    require(withSemicolons.ok(),
            "a stray semicolon should not make a program fail");
    require(withSemicolons.diagnostics.empty(),
            "a stray semicolon should not be reported");
    require(
        plain.root->children[0]->children[0]->children.size() ==
            withSemicolons.root->children[0]->children[0]->children.size(),
        "a stray semicolon should not add a statement to the body"
    );
}

void testAnEmptyProgramOfSemicolonsParses() {
    auto result = parse(";\n;\n");

    require(result.ok(), "a program of empty statements should parse");
    require(result.diagnostics.empty(),
            "a program of empty statements should not be reported");
    require(result.root->children.empty(),
            "a program of empty statements should hold no declaration");
}


void testAMemberMayContinueOnTheNextLine() {
    auto result = parse(
        "import package math\n"
        "fn main() {\n"
        "    let value = math\n"
        "        .max(3, 7)\n"
        "}\n"
    );

    require(result.ok(), "a member on the next line should parse");

    const pebble::Node& body = *result.root->children[1]->children[0];

    require(body.children.size() == 1, "the body should hold one statement");
    require(body.children[0]->kind == pebble::NodeKind::LetDeclaration,
            "the statement should be a declaration");
    require(body.children[0]->children[0]->kind ==
                pebble::NodeKind::CallExpression,
            "the initializer should be a call");
    require(body.children[0]->children[0]->children[0]->kind ==
                pebble::NodeKind::MemberExpression,
            "the call should be made on a member");
    require(body.children[0]->children[0]->children[0]->text == "max",
            "the name of the member should be preserved");
}


void testALeadingBinaryOperatorIsReported() {
    auto result = parse(
        "fn main() {\n"
        "    let value = 1\n"
        "    + 2\n"
        "}\n"
    );

    require(!result.ok(), "a binary operator on a line of its own should fail");
    require(!result.diagnostics.empty(), "the operator should be reported");

    const std::string& message = result.diagnostics[0].message;

    require(message.find("'+'") != std::string::npos,
            "the report should name the operator");
    require(message.find("semicolon") == std::string::npos,
            "the report should not ask for a semicolon");
    require(message.find("';'") == std::string::npos,
            "the report should not ask for a semicolon");
}

void testALeadingAssignmentIsReported() {
    auto result = parse(
        "fn main() {\n"
        "    var value = 1\n"
        "    = 2\n"
        "}\n"
    );

    require(!result.ok(), "an assignment on a line of its own should fail");
    require(!result.diagnostics.empty(), "the assignment should be reported");

    const std::string& message = result.diagnostics[0].message;

    require(message.find("'='") != std::string::npos,
            "the report should name the assignment");
    require(message.find("semicolon") == std::string::npos,
            "the report should not ask for a semicolon");
}


void testRecoveryStopsAtTheNextLine() {
    auto result = parse(
        "fn main() {\n"
        "    let value = 1\n"
        "    + 2\n"
        "    print(value)\n"
        "}\n"
    );

    require(!result.ok(), "the broken statement should be reported");

    const pebble::Node& body = *result.root->children[0]->children[0];

    require(body.children.size() == 2,
            "the statement after the broken one should survive");
    require(body.children[1]->kind == pebble::NodeKind::ExpressionStatement,
            "the statement after the broken one should be a call");
}


void testASemicolonFreeProgram() {
    auto result = parse(
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
        "}\n"
    );

    require(result.ok(), "a program without semicolons should parse");
    require(result.diagnostics.empty(),
            "a program without semicolons should not be reported");
    require(result.root->children.size() == 4,
            "the program should hold two imports and two functions");

    const pebble::Node& main = *result.root->children[3];
    const pebble::Node& body = *main.children[0];

    require(body.children.size() == 5,
            "the body should hold five statements");
    require(body.children[3]->kind == pebble::NodeKind::IfStatement,
            "the fourth statement should be the conditional");
    require(body.children[3]->children.size() == 3,
            "the conditional should hold both of its branches");
    require(body.children[4]->kind == pebble::NodeKind::ExpressionStatement,
            "the fifth statement should be the call");
}


void testAClosingBraceIsFollowedByTheNextStatement() {
    auto result = parse(
        "fn main() {\n"
        "    if true {\n"
        "        print(1)\n"
        "    } else {\n"
        "        print(2)\n"
        "    }\n"
        "    print(3)\n"
        "}\n"
    );

    require(result.ok(), "a statement after a closing brace should parse");

    const pebble::Node& body = *result.root->children[0]->children[0];

    require(body.children.size() == 2,
            "the body should hold the conditional and the call");
    require(body.children[1]->kind == pebble::NodeKind::ExpressionStatement,
            "the statement after the brace should be a call");
}


void testANestedCallOverSeveralLines() {
    auto result = parse(
        "fn add(a: Int, b: Int) -> Int {\n"
        "    return a + b\n"
        "}\n"
        "fn main() {\n"
        "    let total = add(\n"
        "        add(1, 2),\n"
        "        add(3, 4)\n"
        "    )\n"
        "    print(total)\n"
        "}\n"
    );

    require(result.ok(), "a call over several lines should parse");

    const pebble::Node& body = *result.root->children[1]->children[0];

    require(body.children.size() == 2, "the body should hold two statements");
    require(body.children[0]->children[0]->kind ==
                pebble::NodeKind::CallExpression,
            "the declaration should hold a call");
    require(body.children[0]->children[0]->children.size() == 3,
            "the call should hold the name and the two arguments");
    require(body.children[0]->children[0]->children[1]->kind ==
                pebble::NodeKind::CallExpression,
            "the first argument should be a call of its own");
}

}

int main() {
    testFunctionAndDeclarations();
    testOperatorPrecedence();
    testParametersAndReturnType();
    testMalformedSyntax();
    testControlFlowStatements();
    testInvalidAssignmentTarget();
    testNewlineTerminatedStatements();
    testSemicolonsStayOptional();
    testAStatementNeedsATerminator();
    testAnOperatorOnTheNextLineStartsANewStatement();
    testAParenthesisOnTheNextLineDoesNotContinueACall();
    testMultilineExpressionsContinue();
    testBlankLinesAndCommentsDoNotChangeTheProgram();
    testNestedBlocksAreDelimited();
    testTheCanonicalImportAndItsShorthand();
    testAnImportNeedsAName();
    testAFunctionWithACallAndAReturn();
    testTheValueOfAReturnStandsOnItsLine();
    testAStraySemicolonIsAnEmptyStatement();
    testAnEmptyProgramOfSemicolonsParses();
    testAMemberMayContinueOnTheNextLine();
    testALeadingBinaryOperatorIsReported();
    testALeadingAssignmentIsReported();
    testRecoveryStopsAtTheNextLine();
    testASemicolonFreeProgram();
    testAClosingBraceIsFollowedByTheNextStatement();
    testANestedCallOverSeveralLines();

    std::cout << "All parser tests passed.\n";
    return EXIT_SUCCESS;
}
