#include "pebble_c/lexer.h"
#include "pebble_c/parser.h"
#include "pebble_c/type_checker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static PebbleCTypeCheckResult check(const char *text)
{
    PebbleCSource source = {"types_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    PebbleCTypeCheckResult result;

    if (!pebble_c_parse_result_ok(&parsed)) {
        fprintf(stderr, "Test setup failed during parsing.\n");
        exit(EXIT_FAILURE);
    }

    result = pebble_c_type_check(parsed.root);
    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
    return result;
}

static int contains_message(const PebbleCTypeCheckResult *result,
                            const char *fragment)
{
    size_t index;

    for (index = 0; index < result->diagnostics.count; index++) {
        if (strstr(result->diagnostics.items[index].message, fragment) != NULL) {
            return 1;
        }
    }
    return 0;
}

static void test_valid_program(void)
{
    PebbleCTypeCheckResult result = check(
        "fn main() {\n"
        " let title: String = \"Pebble\";\n"
        " var score = add(20, 22);\n"
        " score = score + 1;\n"
        " print(title, score);\n"
        "}\n"
        "fn add(a: Int, b: Int) -> Int { return a + b; }\n");

    require(pebble_c_type_check_result_ok(&result),
            "valid program and forward function reference should pass");
    pebble_c_type_check_result_destroy(&result);
}

static void test_initializer_mismatch(void)
{
    PebbleCTypeCheckResult result =
        check("fn main() { let score: Int = \"wrong\"; }");

    require(!pebble_c_type_check_result_ok(&result),
            "incompatible initializer must fail");
    require(contains_message(&result, "Cannot initialise"),
            "initializer diagnostic should explain the mismatch");
    pebble_c_type_check_result_destroy(&result);
}

static void test_immutable_assignment(void)
{
    PebbleCTypeCheckResult result =
        check("fn main() { let score = 42; score = 10; }");

    require(contains_message(&result, "immutable variable"),
            "let assignment must be rejected");
    pebble_c_type_check_result_destroy(&result);
}

static void test_mutable_type_mismatch(void)
{
    PebbleCTypeCheckResult result =
        check("fn main() { var score = 42; score = \"wrong\"; }");

    require(contains_message(&result, "Cannot assign a value"),
            "var assignment must preserve its inferred type");
    pebble_c_type_check_result_destroy(&result);
}

static void test_unknown_name(void)
{
    PebbleCTypeCheckResult result =
        check("fn main() { print(missing); }");

    require(contains_message(&result, "Undeclared name 'missing'"),
            "unknown names should be reported");
    pebble_c_type_check_result_destroy(&result);
}

static void test_call_arguments_and_arity(void)
{
    PebbleCTypeCheckResult result = check(
        "fn greet(name: String) { print(name); }\n"
        "fn main() { greet(42); greet(); }\n");

    require(contains_message(&result, "expects 'String'"),
            "wrong argument type should be reported");
    require(contains_message(&result, "expects 1 argument"),
            "wrong argument count should be reported");
    pebble_c_type_check_result_destroy(&result);
}

static void test_return_type(void)
{
    PebbleCTypeCheckResult result =
        check("fn answer() -> Int { return \"wrong\"; }");

    require(contains_message(&result, "Return type mismatch"),
            "incorrect return type should be reported");
    pebble_c_type_check_result_destroy(&result);
}

static void test_missing_return(void)
{
    PebbleCTypeCheckResult result = check("fn answer() -> Int { }");

    require(contains_message(&result,
                             "The function 'answer' must return a value."),
            "a function without a return should be reported");
    pebble_c_type_check_result_destroy(&result);
}

static void test_return_on_every_path_is_accepted(void)
{
    PebbleCTypeCheckResult result = check(
        "fn classify(value: Int) -> Int {\n"
        " if value > 0 {\n"
        "  return 1;\n"
        " } else {\n"
        "  return 0;\n"
        " }\n"
        "}\n"
        "fn main() { print(classify(1)); }\n");

    require(!contains_message(&result, "must return a value"),
            "a function that returns on every path should pass");
    pebble_c_type_check_result_destroy(&result);
}

static void test_void_function_without_return_is_accepted(void)
{
    PebbleCTypeCheckResult result = check("fn greet() { print(\"hi\"); }");

    require(!contains_message(&result, "must return a value"),
            "a function without a value should pass");
    pebble_c_type_check_result_destroy(&result);
}

static void test_scopes_and_conditions(void)
{
    PebbleCTypeCheckResult result = check(
        "fn main() {\n"
        " if 42 { let hidden = 1; }\n"
        " print(hidden);\n"
        " while 1 { print(1); }\n"
        "}\n");

    require(contains_message(&result, "If condition must have type 'Bool'"),
            "if condition must be Bool");
    require(contains_message(&result, "Undeclared name 'hidden'"),
            "branch-local variables must not escape their scope");
    require(contains_message(&result, "While condition must have type 'Bool'"),
            "while condition must be Bool");
    pebble_c_type_check_result_destroy(&result);
}

static void test_numeric_widening(void)
{
    PebbleCTypeCheckResult result = check(
        "fn main() {\n"
        " let value: Float = 12;\n"
        " var other: Float = 2.5;\n"
        " other = value;\n"
        "}\n");

    require(pebble_c_type_check_result_ok(&result),
            "Int to Float widening should be allowed");
    pebble_c_type_check_result_destroy(&result);
}

static void test_duplicate_declarations(void)
{
    PebbleCTypeCheckResult result = check(
        "fn main() { let value = 1; let value = 2; }\n"
        "fn main() {}\n");

    require(contains_message(&result, "already declared"),
            "duplicate declarations should be rejected");
    pebble_c_type_check_result_destroy(&result);
}

static void test_null_literal_is_accepted(void)
{
    PebbleCTypeCheckResult result =
        check("fn main() { let empty = null; print(empty); }");

    require(pebble_c_type_check_result_ok(&result),
            "the null literal should be accepted");
    pebble_c_type_check_result_destroy(&result);
}

static void test_null_values_are_comparable(void)
{
    PebbleCTypeCheckResult result =
        check("fn main() { let empty = null; print(empty == null, empty != null); }");

    require(pebble_c_type_check_result_ok(&result),
            "two null values should be comparable");
    pebble_c_type_check_result_destroy(&result);
}

static void test_null_cannot_be_used_as_a_string(void)
{
    PebbleCTypeCheckResult result =
        check("fn main() { let text: String = null; }");

    require(contains_message(&result, "with a value of type 'Null'"),
            "a null value should not be a string");
    pebble_c_type_check_result_destroy(&result);
}

static void test_null_type_cannot_be_written(void)
{
    PebbleCTypeCheckResult result =
        check("fn main() { let empty: Null = null; }");

    require(contains_message(&result, "Unknown type 'Null'"),
            "the null type should not be a type name");
    pebble_c_type_check_result_destroy(&result);
}

static void test_null_cannot_be_used_as_a_condition(void)
{
    PebbleCTypeCheckResult result =
        check("fn main() { if null { print(1); } }");

    require(contains_message(&result, "If condition must have type 'Bool', not 'Null'"),
            "a null value should not be a condition");
    pebble_c_type_check_result_destroy(&result);
}

static void test_null_cannot_be_compared_with_a_string(void)
{
    PebbleCTypeCheckResult result =
        check("fn main() { print(null == \"text\"); }");

    require(contains_message(&result, "Cannot compare values of type 'Null' and 'String'"),
            "a null value should not be compared with a string");
    pebble_c_type_check_result_destroy(&result);
}

int main(void)
{
    test_valid_program();
    test_initializer_mismatch();
    test_immutable_assignment();
    test_mutable_type_mismatch();
    test_unknown_name();
    test_call_arguments_and_arity();
    test_return_type();
    test_missing_return();
    test_return_on_every_path_is_accepted();
    test_void_function_without_return_is_accepted();
    test_scopes_and_conditions();
    test_numeric_widening();
    test_duplicate_declarations();
    test_null_literal_is_accepted();
    test_null_values_are_comparable();
    test_null_cannot_be_used_as_a_string();
    test_null_type_cannot_be_written();
    test_null_cannot_be_used_as_a_condition();
    test_null_cannot_be_compared_with_a_string();

    puts("All C type checker tests passed.");
    return EXIT_SUCCESS;
}
