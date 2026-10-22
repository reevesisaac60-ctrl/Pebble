#include "pebble_c/codegen.h"
#include "pebble_c/lexer.h"
#include "pebble_c/parser.h"
#include "pebble_c/type_checker.h"

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

static char *generate(const char *text)
{
    PebbleCSource source = {"codegen_c_test.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    PebbleCTypeCheckResult checked;
    char *generated;

    require(parsed.root != NULL && pebble_c_parse_result_ok(&parsed),
            "sample should parse");

    checked = pebble_c_type_check(parsed.root);
    require(pebble_c_type_check_result_ok(&checked),
            "sample should pass type checking");
    pebble_c_type_check_result_destroy(&checked);

    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "C source should be generated");

    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
    return generated;
}

static void test_sample_program(void)
{
    const char *text =
        "fn add(a: Int, b: Int) -> Int { return a + b; }\n"
        "fn main() {\n"
        "  let name: String = \"Pebble\";\n"
        "  var score = add(20, 22);\n"
        "  score = score + 1;\n"
        "  print(name, score, true, 3.5);\n"
        "  if score > 0 && true { print(\"positive\"); }\n"
        "  while false { print(\"unreachable\"); }\n"
        "}\n";
    char *generated = generate(text);

    require(strstr(generated, "#include <stdbool.h>") != NULL,
            "generated source should include C boolean definitions");
    require(strstr(generated, "int64_t pebble_fn_add(int64_t pebble_var_a, int64_t pebble_var_b)") != NULL,
            "generated source should declare typed C functions");
    require(strstr(generated, "return (pebble_var_a + pebble_var_b);") != NULL,
            "generated source should preserve arithmetic");
    require(strstr(generated, "pebble_fn_add(INT64_C(20), INT64_C(22))") != NULL,
            "generated source should preserve function calls");
    require(strstr(generated, "pebble_print_string(pebble_var_name);\n") != NULL,
            "generated source should use the C print helper");
    require(strstr(generated, "while (false)") != NULL,
            "generated source should preserve loops");

    free(generated);
}

static void test_print_dispatch(void)
{
    const char *text =
        "fn main() {\n"
        "  let name: String = \"Pebble\";\n"
        "  let score = 42;\n"
        "  print(name, score, true, 3.5, null);\n"
        "}\n";
    char *generated = generate(text);

    require(strstr(generated, "pebble_print_string(pebble_var_name);") != NULL,
            "strings should be printed with the string helper");
    require(strstr(generated, "pebble_print_int(pebble_var_score);") != NULL,
            "integers should be printed with the integer helper");
    require(strstr(generated, "pebble_print_bool(true);") != NULL,
            "booleans should be printed with the boolean helper");
    require(strstr(generated, "pebble_print_float(3.5);") != NULL,
            "floats should be printed with the float helper");
    require(strstr(generated, "pebble_print_null();") != NULL,
            "null values should be printed with the null helper");
    require(strstr(generated, "_Generic") == NULL,
            "generated source should not rely on generic dispatch");
    require(strstr(generated, ");\\n") == NULL,
            "generated source should not contain literal escape sequences");

    free(generated);
}

static void test_unused_print_helpers_are_omitted(void)
{
    const char *text =
        "fn main() {\n"
        "  print(1);\n"
        "}\n";
    char *generated = generate(text);

    require(strstr(generated, "pebble_print_int(INT64_C(1));") != NULL,
            "integers should be printed with the integer helper");
    require(strstr(generated, "pebble_print_float") == NULL,
            "unused float helpers should not be generated");
    require(strstr(generated, "pebble_print_bool") == NULL,
            "unused boolean helpers should not be generated");
    require(strstr(generated, "pebble_print_string") == NULL,
            "unused string helpers should not be generated");
    require(strstr(generated, "pebble_print_null") == NULL,
            "unused null helpers should not be generated");
    require(strstr(generated, "pebble_print_separator") == NULL,
            "unused separator helpers should not be generated");

    free(generated);
}

static void test_print_without_arguments(void)
{
    const char *text =
        "fn main() {\n"
        "  print();\n"
        "}\n";
    char *generated = generate(text);

    require(strstr(generated, "pebble_print_end();") != NULL,
            "an empty print should still finish the line");
    require(strstr(generated, "pebble_print_separator") == NULL,
            "an empty print should not separate values");
    require(strstr(generated, "pebble_print_int") == NULL,
            "an empty print should not generate value helpers");

    free(generated);
}

static void test_string_declaration(void)
{
    const char *text =
        "fn main() {\n"
        "  let name: String = \"Pebble\";\n"
        "}\n";
    char *generated = generate(text);

    require(strstr(generated, "const char * pebble_var_name = \"Pebble\";") != NULL,
            "string declarations should use a single const qualifier");
    require(strstr(generated, "const const") == NULL,
            "string declarations should not repeat the const qualifier");

    free(generated);
}

static void test_numeric_widening(void)
{
    const char *text =
        "fn main() {\n"
        "  let value: Float = 12;\n"
        "  var other: Float = 2.5;\n"
        "  other = value;\n"
        "  print(other);\n"
        "}\n";
    char *generated = generate(text);

    require(strstr(generated, "double pebble_var_value = (double)(INT64_C(12));") != NULL,
            "widened integers should be converted explicitly");
    require(strstr(generated, "pebble_print_float(pebble_var_other);") != NULL,
            "floats should be printed with the float helper");

    free(generated);
}

static void test_null_value(void)
{
    const char *text =
        "fn main() {\n"
        "  let empty = null;\n"
        "  print(empty, null, empty == null);\n"
        "}\n";
    char *generated = generate(text);

    require(strstr(generated, "pebble_print_null();") != NULL,
            "a null value should be printed with the null helper");
    require(strstr(generated, "pebble_print_null();") != NULL,
            "the null helper should be emitted");

    free(generated);
}

int main(void)
{
    test_sample_program();
    test_print_dispatch();
    test_unused_print_helpers_are_omitted();
    test_print_without_arguments();
    test_string_declaration();
    test_numeric_widening();
    test_null_value();

    (void)puts("All C code generator tests passed.");

    return EXIT_SUCCESS;
}
