#include "pebble_c/lexer.h"
#include "pebble_c/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static PebbleCParseResult parse_text(const char *text)
{
    PebbleCSource source = {"test.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult result = pebble_c_parse(&tokens);

    pebble_c_tokens_destroy(&tokens);
    return result;
}

static void test_function_and_declarations(void)
{
    PebbleCParseResult result = parse_text(
        "fn main() {"
        " let name: String = \"Pebble\";"
        " var score: Int = 40 + 2 * 3;"
        " score = score + 1;"
        " print(name, score);"
        " return;"
        "}"
    );
    PebbleCNode *function;
    PebbleCNode *body;

    require(pebble_c_parse_result_ok(&result),
            "function and declarations should parse");
    require(result.root->kind == PEBBLE_C_NODE_PROGRAM,
            "root should be a program");
    require(result.root->child_count == 1U,
            "program should contain one function");

    function = result.root->children[0];
    require(function->kind == PEBBLE_C_NODE_FUNCTION,
            "top-level item should be a function");
    require(strcmp(function->text, "main") == 0,
            "function name should be preserved");
    require(function->child_count == 1U,
            "function should contain its body");

    body = function->children[0];
    require(body->kind == PEBBLE_C_NODE_BLOCK,
            "function body should be a block");
    require(body->child_count == 5U,
            "all five statements should be preserved");
    require(body->children[0]->kind == PEBBLE_C_NODE_LET_DECLARATION,
            "let declaration should be represented");
    require(body->children[1]->kind == PEBBLE_C_NODE_VAR_DECLARATION,
            "var declaration should be represented");
    require(body->children[2]->kind ==
                PEBBLE_C_NODE_EXPRESSION_STATEMENT,
            "assignment statement should be represented");
    require(body->children[3]->children[0]->kind ==
                PEBBLE_C_NODE_CALL_EXPRESSION,
            "function call should be represented");
    require(body->children[4]->kind == PEBBLE_C_NODE_RETURN_STATEMENT,
            "return should be represented");

    pebble_c_parse_result_destroy(&result);
}

static void test_operator_precedence(void)
{
    PebbleCParseResult result =
        parse_text("fn main() { let x = 1 + 2 * 3; }");
    PebbleCNode *body;
    PebbleCNode *declaration;
    PebbleCNode *addition;

    require(pebble_c_parse_result_ok(&result),
            "arithmetic expression should parse");

    body = result.root->children[0]->children[0];
    declaration = body->children[0];
    addition = declaration->children[0];

    require(addition->kind == PEBBLE_C_NODE_BINARY_EXPRESSION,
            "initializer should be a binary expression");
    require(strcmp(addition->text, "+") == 0,
            "addition should be the outer operation");
    require(strcmp(addition->children[1]->text, "*") == 0,
            "multiplication should bind more tightly");

    pebble_c_parse_result_destroy(&result);
}

static void test_parameters_and_return_type(void)
{
    PebbleCParseResult result =
        parse_text("fn add(a: Int, b: Int) -> Int { return a + b; }");
    PebbleCNode *function;

    require(pebble_c_parse_result_ok(&result),
            "typed parameters and return type should parse");

    function = result.root->children[0];
    require(function->child_count == 4U,
            "function should have two parameters, return type and body");
    require(function->children[0]->kind == PEBBLE_C_NODE_PARAMETER,
            "first child should be a parameter");
    require(function->children[2]->kind == PEBBLE_C_NODE_TYPE_NAME,
            "return type should be represented explicitly");
    require(strcmp(function->children[2]->text, "Int") == 0,
            "return type should be preserved");

    pebble_c_parse_result_destroy(&result);
}

static void test_control_flow(void)
{
    PebbleCParseResult result = parse_text(
        "fn main() {"
        " var count: Int = 0;"
        " while count < 3 {"
        "  if count == 1 { print(1); }"
        "  else if count == 2 { print(2); }"
        "  else { print(count); }"
        "  count = count + 1;"
        " }"
        "}"
    );
    PebbleCNode *body;
    PebbleCNode *loop;
    PebbleCNode *branch;

    require(pebble_c_parse_result_ok(&result),
            "if, else-if, else and while should parse");

    body = result.root->children[0]->children[0];
    loop = body->children[1];
    require(loop->kind == PEBBLE_C_NODE_WHILE_STATEMENT,
            "loop should have a while node");

    branch = loop->children[1]->children[0];
    require(branch->kind == PEBBLE_C_NODE_IF_STATEMENT,
            "conditional should have an if node");
    require(branch->child_count == 3U,
            "if should preserve the else-if and else branches");
    require(branch->children[2]->kind == PEBBLE_C_NODE_IF_STATEMENT,
            "else-if should be nested as an if node");

    pebble_c_parse_result_destroy(&result);
}

static void test_invalid_assignment_and_malformed_syntax(void)
{
    PebbleCParseResult invalid =
        parse_text("fn main() { (1 + 2) = 3; }");
    PebbleCParseResult malformed =
        parse_text("fn main( { let x = ; }");

    require(!pebble_c_parse_result_ok(&invalid),
            "non-variable assignment target should be rejected");
    require(!pebble_c_parse_result_ok(&malformed),
            "malformed syntax should produce diagnostics");
    require(malformed.diagnostics.count > 0U,
            "syntax errors should be reported");
    require(malformed.diagnostics.items[0].location.file != NULL &&
            strcmp(malformed.diagnostics.items[0].location.file,
                   "test.peb") == 0,
            "diagnostic should identify the source file");

    pebble_c_parse_result_destroy(&invalid);
    pebble_c_parse_result_destroy(&malformed);
}

static void test_error_recovery_preserves_following_function(void)
{
    PebbleCParseResult result = parse_text(
        "let = ; fn recovered() { return; }"
    );

    require(!pebble_c_parse_result_ok(&result),
            "malformed declaration should produce a parse error");
    require(result.diagnostics.count > 0U,
            "malformed declaration should report diagnostics");
    require(result.root != NULL && result.root->child_count == 1U,
            "parser should preserve the valid function after the error");
    require(result.root->children[0]->kind == PEBBLE_C_NODE_FUNCTION,
            "recovered declaration should be a function");
    require(strcmp(result.root->children[0]->text, "recovered") == 0,
            "parser should preserve the recovered function name");

    pebble_c_parse_result_destroy(&result);
}

static void test_multiple_errors_preserve_later_code(void)
{
    PebbleCParseResult result = parse_text(
        "fn main() {"
        " let first = ;"
        " let second = ;"
        " print(3);"
        "}"
        " fn recovered() { return; }"
    );
    PebbleCNode *body;

    require(!pebble_c_parse_result_ok(&result),
            "multiple malformed initializers should fail parsing");
    require(result.diagnostics.count >= 2U,
            "parser should report both malformed initializers");
    require(result.root != NULL && result.root->child_count == 2U,
            "parser should preserve both functions after errors");

    body = result.root->children[0]->children[0];
    require(body->child_count == 3U,
            "parser should preserve statements following malformed initializers");
    require(body->children[2]->kind == PEBBLE_C_NODE_EXPRESSION_STATEMENT,
            "valid print statement should survive recovery");
    require(strcmp(result.root->children[1]->text, "recovered") == 0,
            "later function should survive earlier errors");

    pebble_c_parse_result_destroy(&result);
}

static void test_missing_call_delimiter_preserves_following_statement(void)
{
    PebbleCParseResult result =
        parse_text("fn main() { print(1; return; }");
    PebbleCNode *body;

    require(!pebble_c_parse_result_ok(&result),
            "missing call delimiter should fail parsing");
    require(result.diagnostics.count > 0U,
            "missing call delimiter should produce a diagnostic");
    require(result.root != NULL && result.root->child_count == 1U,
            "function should remain in the AST");

    body = result.root->children[0]->children[0];
    require(body->child_count == 2U,
            "statement after malformed call should be preserved");
    require(body->children[0]->kind == PEBBLE_C_NODE_EXPRESSION_STATEMENT,
            "malformed call should remain represented in the AST");
    require(body->children[1]->kind == PEBBLE_C_NODE_RETURN_STATEMENT,
            "return statement after missing delimiter should survive");

    pebble_c_parse_result_destroy(&result);
}

static void test_a_stray_semicolon_is_an_empty_statement(void)
{
    PebbleCParseResult plain = parse_text(
        "fn main() {\n"
        "    let value = 1\n"
        "    print(value)\n"
        "}\n"
    );
    PebbleCParseResult with_semicolons = parse_text(
        "fn main() {\n"
        "    let value = 1;\n"
        "    print(value);\n"
        "};\n"
        ";\n"
    );
    PebbleCNode *plain_body;
    PebbleCNode *body;

    require(pebble_c_parse_result_ok(&plain),
            "the plain program should parse");
    require(pebble_c_parse_result_ok(&with_semicolons),
            "a stray semicolon should not make a program fail");
    require(with_semicolons.diagnostics.count == 0U,
            "a stray semicolon should not be reported");

    plain_body = plain.root->children[0]->children[0];
    body = with_semicolons.root->children[0]->children[0];

    require(plain_body->child_count == body->child_count,
            "a stray semicolon should not add a statement to the body");

    pebble_c_parse_result_destroy(&plain);
    pebble_c_parse_result_destroy(&with_semicolons);
}

static void test_an_empty_program_of_semicolons_parses(void)
{
    PebbleCParseResult result = parse_text(";\n;\n");

    require(pebble_c_parse_result_ok(&result),
            "a program of empty statements should parse");
    require(result.diagnostics.count == 0U,
            "a program of empty statements should not be reported");
    require(result.root != NULL && result.root->child_count == 0U,
            "a program of empty statements should hold no declaration");

    pebble_c_parse_result_destroy(&result);
}

static void test_a_member_may_continue_on_the_next_line(void)
{
    PebbleCParseResult result = parse_text(
        "import package math\n"
        "fn main() {\n"
        "    let value = math\n"
        "        .max(3, 7)\n"
        "}\n"
    );
    PebbleCNode *body;

    require(pebble_c_parse_result_ok(&result),
            "a member on the next line should parse");

    body = result.root->children[1]->children[0];
    require(body->child_count == 1U, "the body should hold one statement");
    require(body->children[0]->kind == PEBBLE_C_NODE_LET_DECLARATION,
            "the statement should be a declaration");
    require(body->children[0]->children[0]->kind ==
                PEBBLE_C_NODE_CALL_EXPRESSION,
            "the initializer should be a call");
    require(body->children[0]->children[0]->children[0]->kind ==
                PEBBLE_C_NODE_MEMBER_EXPRESSION,
            "the call should be made on a member");
    require(strcmp(body->children[0]->children[0]->children[0]->text,
                   "max") == 0,
            "the name of the member should be preserved");

    pebble_c_parse_result_destroy(&result);
}

static void test_a_leading_binary_operator_is_reported(void)
{
    PebbleCParseResult result = parse_text(
        "fn main() {\n"
        "    let value = 1\n"
        "    + 2\n"
        "}\n"
    );

    require(!pebble_c_parse_result_ok(&result),
            "a binary operator on a line of its own should fail");
    require(result.diagnostics.count > 0U,
            "the operator should be reported");
    require(strstr(result.diagnostics.items[0].message, "'+'") != NULL,
            "the report should name the operator");
    require(strstr(result.diagnostics.items[0].message, "semicolon") == NULL,
            "the report should not ask for a semicolon");
    require(strstr(result.diagnostics.items[0].message, "';'") == NULL,
            "the report should not ask for a semicolon");

    pebble_c_parse_result_destroy(&result);
}

static void test_a_leading_assignment_is_reported(void)
{
    PebbleCParseResult result = parse_text(
        "fn main() {\n"
        "    var value = 1\n"
        "    = 2\n"
        "}\n"
    );

    require(!pebble_c_parse_result_ok(&result),
            "an assignment on a line of its own should fail");
    require(result.diagnostics.count > 0U,
            "the assignment should be reported");
    require(strstr(result.diagnostics.items[0].message, "'='") != NULL,
            "the report should name the assignment");
    require(strstr(result.diagnostics.items[0].message, "semicolon") == NULL,
            "the report should not ask for a semicolon");

    pebble_c_parse_result_destroy(&result);
}

static void test_recovery_stops_at_the_next_line(void)
{
    PebbleCParseResult result = parse_text(
        "fn main() {\n"
        "    let value = 1\n"
        "    + 2\n"
        "    print(value)\n"
        "}\n"
    );
    PebbleCNode *body;

    require(!pebble_c_parse_result_ok(&result),
            "the broken statement should be reported");

    body = result.root->children[0]->children[0];
    require(body->child_count == 2U,
            "the statement after the broken one should survive");
    require(body->children[1]->kind == PEBBLE_C_NODE_EXPRESSION_STATEMENT,
            "the statement after the broken one should be a call");

    pebble_c_parse_result_destroy(&result);
}

static void test_a_semicolon_free_program(void)
{
    PebbleCParseResult result = parse_text(
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
    PebbleCNode *body;

    require(pebble_c_parse_result_ok(&result),
            "a program without semicolons should parse");
    require(result.diagnostics.count == 0U,
            "a program without semicolons should not be reported");
    require(result.root->child_count == 4U,
            "the program should hold two imports and two functions");

    body = result.root->children[3]->children[0];
    require(body->child_count == 5U, "the body should hold five statements");
    require(body->children[3]->kind == PEBBLE_C_NODE_IF_STATEMENT,
            "the fourth statement should be the conditional");
    require(body->children[3]->child_count == 3U,
            "the conditional should hold both of its branches");
    require(body->children[4]->kind == PEBBLE_C_NODE_EXPRESSION_STATEMENT,
            "the fifth statement should be the call");

    pebble_c_parse_result_destroy(&result);
}

static void test_a_closing_brace_is_followed_by_the_next_statement(void)
{
    PebbleCParseResult result = parse_text(
        "fn main() {\n"
        "    if true {\n"
        "        print(1)\n"
        "    } else {\n"
        "        print(2)\n"
        "    }\n"
        "    print(3)\n"
        "}\n"
    );
    PebbleCNode *body;

    require(pebble_c_parse_result_ok(&result),
            "a statement after a closing brace should parse");

    body = result.root->children[0]->children[0];
    require(body->child_count == 2U,
            "the body should hold the conditional and the call");
    require(body->children[1]->kind == PEBBLE_C_NODE_EXPRESSION_STATEMENT,
            "the statement after the brace should be a call");

    pebble_c_parse_result_destroy(&result);
}

static void test_a_nested_call_over_several_lines(void)
{
    PebbleCParseResult result = parse_text(
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
    PebbleCNode *body;

    require(pebble_c_parse_result_ok(&result),
            "a call over several lines should parse");

    body = result.root->children[1]->children[0];
    require(body->child_count == 2U, "the body should hold two statements");
    require(body->children[0]->children[0]->kind ==
                PEBBLE_C_NODE_CALL_EXPRESSION,
            "the declaration should hold a call");
    require(body->children[0]->children[0]->child_count == 3U,
            "the call should hold the name and the two arguments");
    require(body->children[0]->children[0]->children[1]->kind ==
                PEBBLE_C_NODE_CALL_EXPRESSION,
            "the first argument should be a call of its own");

    pebble_c_parse_result_destroy(&result);
}

int main(void)
{
    test_function_and_declarations();
    test_operator_precedence();
    test_parameters_and_return_type();
    test_control_flow();
    test_invalid_assignment_and_malformed_syntax();
    test_error_recovery_preserves_following_function();
    test_multiple_errors_preserve_later_code();
    test_missing_call_delimiter_preserves_following_statement();
    test_a_stray_semicolon_is_an_empty_statement();
    test_an_empty_program_of_semicolons_parses();
    test_a_member_may_continue_on_the_next_line();
    test_a_leading_binary_operator_is_reported();
    test_a_leading_assignment_is_reported();
    test_recovery_stops_at_the_next_line();
    test_a_semicolon_free_program();
    test_a_closing_brace_is_followed_by_the_next_statement();
    test_a_nested_call_over_several_lines();

    (void)puts("All C parser tests passed.");
    return EXIT_SUCCESS;
}
