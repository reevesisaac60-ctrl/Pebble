#include "pebble_c/codegen.h"
#include "pebble_c/lexer.h"
#include "pebble_c/package.h"
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
    PebbleCSource source = {"packages_c.peb", text, strlen(text)};
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

static void test_import_keywords_are_lexed(void)
{
    PebbleCSource source = {"packages_c.peb", "import package math\n", 20U};
    PebbleCTokenList tokens = pebble_c_lex(source);

    require(tokens.diagnostics.count == 0U, "import statement should lex");
    require(tokens.count == 4U,
            "import statement should produce three tokens and an end marker");
    require(tokens.items[0].kind == PEBBLE_C_TOKEN_IMPORT,
            "'import' should be an import token");
    require(tokens.items[1].kind == PEBBLE_C_TOKEN_PACKAGE,
            "'package' should be a package token");
    require(tokens.items[2].kind == PEBBLE_C_TOKEN_IDENTIFIER,
            "the package name should be an identifier");
    require(strcmp(tokens.items[2].lexeme, "math") == 0,
            "the package name should be preserved");

    pebble_c_tokens_destroy(&tokens);
}

static void test_import_declaration_is_parsed(void)
{
    PebbleCSource source = {
        "packages_c.peb",
        "import package math\nfn main() { print(1); }\n",
        0U
    };
    source.length = strlen(source.text);
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    const PebbleCNode *import;

    require(pebble_c_parse_result_ok(&parsed),
            "import statement should parse");
    require(parsed.root->child_count == 2U,
            "the program should hold the import and the function");

    import = parsed.root->children[0];
    require(import->kind == PEBBLE_C_NODE_IMPORT_DECLARATION,
            "the import should be represented in the AST");
    require(strcmp(import->text, "math") == 0,
            "the import should name the package");

    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_member_expression_is_parsed(void)
{
    PebbleCSource source = {
        "packages_c.peb",
        "import package math\n"
        "fn main() { let value = math.randint(1, 10); }\n",
        0U
    };
    source.length = strlen(source.text);
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    const PebbleCNode *body;
    const PebbleCNode *declaration;
    const PebbleCNode *call;
    const PebbleCNode *member;

    require(pebble_c_parse_result_ok(&parsed), "package call should parse");

    body = parsed.root->children[1]->children[0];
    declaration = body->children[0];
    call = declaration->children[0];

    require(call->kind == PEBBLE_C_NODE_CALL_EXPRESSION,
            "the call should be represented in the AST");

    member = call->children[0];
    require(member->kind == PEBBLE_C_NODE_MEMBER_EXPRESSION,
            "the callee should be a member expression");
    require(strcmp(member->text, "randint") == 0,
            "the member should name the function");
    require(member->child_count == 1U,
            "the member should reference its package");
    require(member->children[0]->kind == PEBBLE_C_NODE_NAME_EXPRESSION,
            "the package should be referenced by name");
    require(strcmp(member->children[0]->text, "math") == 0,
            "the package name should be preserved");
    require(call->child_count == 3U, "the call should carry both arguments");

    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_valid_package_call_type_checks(void)
{
    PebbleCTypeCheckResult result = check(
        "import package math\n"
        "fn main() {\n"
        " let value = math.randint(1, 10);\n"
        " print(value);\n"
        "}\n");

    require(pebble_c_type_check_result_ok(&result),
            "a valid package call should pass type checking");
    pebble_c_type_check_result_destroy(&result);
}

static void test_unknown_package_is_rejected(void)
{
    PebbleCTypeCheckResult result = check(
        "import package physics\n"
        "fn main() { print(1); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "an unknown package should be rejected");
    require(contains_message(&result, "Unknown package 'physics'"),
            "the diagnostic should name the unknown package");
    pebble_c_type_check_result_destroy(&result);
}

static void test_package_use_without_import_is_rejected(void)
{
    PebbleCTypeCheckResult result = check(
        "fn main() { let value = math.randint(1, 10); print(value); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "using a package without importing it is rejected");
    require(contains_message(&result, "Unknown package 'math'"),
            "the diagnostic should report the missing package");
    pebble_c_type_check_result_destroy(&result);
}

static void test_duplicate_import_is_rejected(void)
{
    PebbleCTypeCheckResult result = check(
        "import package math\n"
        "import package math\n"
        "fn main() { print(1); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "importing a package twice should be rejected");
    require(contains_message(&result, "imported more than once"),
            "the diagnostic should report the duplicate import");
    pebble_c_type_check_result_destroy(&result);
}

static void test_unknown_package_function_is_rejected(void)
{
    PebbleCTypeCheckResult result = check(
        "import package math\n"
        "fn main() { let value = math.cbrt(2); print(value); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "an unknown package function should be rejected");
    require(contains_message(&result, "has no function 'cbrt'"),
            "the diagnostic should name the missing function");
    pebble_c_type_check_result_destroy(&result);
}

static void test_wrong_package_argument_count_is_rejected(void)
{
    PebbleCTypeCheckResult result = check(
        "import package math\n"
        "fn main() { let value = math.randint(1); print(value); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "a wrong argument count should be rejected");
    require(contains_message(&result, "expects 2 argument(s), but got 1"),
            "the diagnostic should report the argument count");
    pebble_c_type_check_result_destroy(&result);
}

static void test_wrong_package_argument_type_is_rejected(void)
{
    PebbleCTypeCheckResult result = check(
        "import package math\n"
        "fn main() { let value = math.randint(1.5, 10); print(value); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "a wrong argument type should be rejected");
    require(contains_message(&result, "expects 'Int', but got 'Float'"),
            "the diagnostic should report the argument type");
    pebble_c_type_check_result_destroy(&result);
}

static void test_package_function_value_is_rejected(void)
{
    PebbleCTypeCheckResult result = check(
        "import package math\n"
        "fn main() { let value = math.randint; print(value); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "using a package function as a value is rejected");
    require(contains_message(&result, "can only be used in a direct call"),
            "the diagnostic should explain the restriction");
    pebble_c_type_check_result_destroy(&result);
}

static void test_import_inside_function_is_rejected(void)
{
    PebbleCTypeCheckResult result = check(
        "fn main() { import package math; print(1); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "importing inside a function should be rejected");
    require(contains_message(&result, "only allowed at the top level"),
            "the diagnostic should explain the restriction");
    pebble_c_type_check_result_destroy(&result);
}

static void test_package_runtime_is_generated(void)
{
    const char *text =
        "import package math\n"
        "fn main() { let value = math.randint(1, 10); print(value); }\n";
    PebbleCSource source = {"packages_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    PebbleCTypeCheckResult checked;
    char *generated;

    require(pebble_c_parse_result_ok(&parsed), "package program should parse");

    checked = pebble_c_type_check(parsed.root);
    require(pebble_c_type_check_result_ok(&checked),
            "package program should pass type checking");

    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "code generation should succeed");
    require(strstr(generated, "pebble_math_randint(INT64_C(1), INT64_C(10))")
            != NULL,
            "the package call should be emitted");
    require(strstr(generated, "static int64_t pebble_math_randint(") != NULL,
            "the package runtime should be emitted");
    require(strstr(generated, "pebble_rng_next") != NULL,
            "the random source should be emitted");

    free(generated);
    pebble_c_type_check_result_destroy(&checked);
    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_float_package_functions_type_check(void)
{
    const char *text =
        "import package math\n"
        "fn main() {\n"
        "    let root = math.sqrt(2.0);\n"
        "    let lower = math.floor(1.5);\n"
        "    let upper = math.ceil(1.5);\n"
        "    let rounded = math.round(1.5);\n"
        "    let fraction = math.random();\n"
        "    let circle = math.pi();\n"
        "    print(root, lower, upper, rounded, fraction, circle);\n"
        "}\n";
    PebbleCTypeCheckResult result = check(text);

    require(pebble_c_type_check_result_ok(&result),
            "the float package functions should type check");

    pebble_c_type_check_result_destroy(&result);
}

static void test_int_arguments_widen_to_float(void)
{
    PebbleCTypeCheckResult result = check(
        "import package math\n"
        "fn main() { print(math.sqrt(9)); }\n"
    );

    require(pebble_c_type_check_result_ok(&result),
            "an Int argument should widen to a Float");

    pebble_c_type_check_result_destroy(&result);
}

static void test_float_package_runtime_is_generated(void)
{
    const char *text =
        "import package math\n"
        "fn main() { print(math.sqrt(2.0), math.floor(1.5), math.ceil(1.5), "
        "math.round(1.5), math.random(), math.pi()); }\n";
    PebbleCSource source = {"packages_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    char *generated;

    require(pebble_c_parse_result_ok(&parsed), "package program should parse");

    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "code generation should succeed");
    require(strstr(generated, "pebble_math_sqrt") != NULL,
            "the square root runtime should be emitted");
    require(strstr(generated, "pebble_math_floor") != NULL,
            "the floor runtime should be emitted");
    require(strstr(generated, "pebble_math_ceil") != NULL,
            "the ceiling runtime should be emitted");
    require(strstr(generated, "pebble_math_round") != NULL,
            "the rounding runtime should be emitted");
    require(strstr(generated, "pebble_math_random") != NULL,
            "the random runtime should be emitted");
    require(strstr(generated, "pebble_math_pi") != NULL,
            "the pi runtime should be emitted");
    require(strstr(generated, "sqrt(value)") != NULL,
            "the square root should use the standard library");

    free(generated);
    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_random_source_is_seeded_from_the_environment(void)
{
    const char *text =
        "import package math\n"
        "fn main() { print(math.random()); }\n";
    PebbleCSource source = {"packages_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    char *generated;

    require(pebble_c_parse_result_ok(&parsed), "package program should parse");

    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "code generation should succeed");
    require(strstr(generated, "pebble_rng_seed") != NULL,
            "the random source should be seeded from the environment");
    require(strstr(generated, "pebble_rng_ready") != NULL,
            "the random source should only seed itself once");

    free(generated);
    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_unused_package_runtime_is_omitted(void)
{
    const char *text =
        "import package math\n"
        "fn main() { print(1); }\n";
    PebbleCSource source = {"packages_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    char *generated;

    require(pebble_c_parse_result_ok(&parsed), "package program should parse");

    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "code generation should succeed");
    require(strstr(generated, "pebble_math_randint") == NULL,
            "an unused package function should not be emitted");
    require(strstr(generated, "pebble_rng_state") == NULL,
            "an unused random source should not be emitted");

    free(generated);
    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_string_package_functions_type_check(void)
{
    const char *text =
        "import package string\n"
        "fn main() {\n"
        "    let size = string.length(\"hello\");\n"
        "    let part = string.substring(\"hello\", 1, 3);\n"
        "    let head = string.left(\"hello\", 2);\n"
        "    let tail = string.right(\"hello\", 2);\n"
        "    let loud = string.upper(\"hello\");\n"
        "    let quiet = string.lower(\"HELLO\");\n"
        "    let trimmed = string.trim(\"  hello  \");\n"
        "    let has = string.contains(\"hello\", \"ell\");\n"
        "    let first = string.startswith(\"hello\", \"he\");\n"
        "    let last = string.endswith(\"hello\", \"lo\");\n"
        "    let index = string.indexof(\"hello\", \"l\");\n"
        "    let swapped = string.replace(\"a-b\", \"-\", \"+\");\n"
        "    let many = string.repeat(\"ab\", 2);\n"
        "    let number = string.toint(\"42\");\n"
        "    let fraction = string.tofloat(\"2.5\");\n"
        "    print(size, part, head, tail, loud, quiet, trimmed);\n"
        "    print(has, first, last, index, swapped, many, number, fraction);\n"
        "}\n";
    PebbleCTypeCheckResult result = check(text);

    require(pebble_c_type_check_result_ok(&result),
            "the string package functions should type check");
    pebble_c_type_check_result_destroy(&result);
}

static void test_string_package_runtime_is_generated(void)
{
    const char *text =
        "import package string\n"
        "fn main() { print(string.length(\"hello\"), string.upper(\"hello\")); }\n";
    PebbleCSource source = {"packages_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    char *generated;

    require(pebble_c_parse_result_ok(&parsed), "package program should parse");

    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "code generation should succeed");
    require(strstr(generated, "pebble_string_length(\"hello\")") != NULL,
            "the package call should be emitted");
    require(strstr(generated, "static int64_t pebble_string_length(") != NULL,
            "the package runtime should be emitted");
    require(strstr(generated, "static char *pebble_string_upper(") != NULL,
            "the package runtime should be emitted");
    require(strstr(generated, "pebble_string_replace") == NULL,
            "an unused package function should not be emitted");

    free(generated);
    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_string_package_rejects_wrong_types(void)
{
    const char *text =
        "import package string\n"
        "fn main() { print(string.length(42)); }\n";
    PebbleCTypeCheckResult result = check(text);

    require(!pebble_c_type_check_result_ok(&result),
            "a wrong argument type should be rejected");
    require(contains_message(&result, "expects 'String', but got 'Int'"),
            "the diagnostic should report the argument type");
    pebble_c_type_check_result_destroy(&result);
}

static void test_string_package_rejects_unknown_function(void)
{
    const char *text =
        "import package string\n"
        "fn main() { print(string.split(\"a,b\", \",\")); }\n";
    PebbleCTypeCheckResult result = check(text);

    require(!pebble_c_type_check_result_ok(&result),
            "an unknown function should be rejected");
    require(contains_message(&result, "has no function 'split'"),
            "the diagnostic should name the missing function");
    pebble_c_type_check_result_destroy(&result);
}

static void test_time_package_functions_type_check(void)
{
    const char *text =
        "import package time\n"
        "fn main() {\n"
        "    let now = time.now();\n"
        "    let ticks = time.ticks();\n"
        "    let delta = time.delta();\n"
        "    time.sleep(0.5);\n"
        "    let stamp = time.unix();\n"
        "    print(now, ticks, delta, stamp);\n"
        "}\n";
    PebbleCTypeCheckResult result = check(text);

    require(pebble_c_type_check_result_ok(&result),
            "the time package functions should type check");
    pebble_c_type_check_result_destroy(&result);
}

static void test_time_package_runtime_is_generated(void)
{
    const char *text =
        "import package time\n"
        "fn main() {\n"
        "    print(time.now(), time.ticks(), time.delta(), time.unix());\n"
        "    time.sleep(0.5);\n"
        "}\n";
    PebbleCSource source = {"packages_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    char *generated;

    require(pebble_c_parse_result_ok(&parsed), "time program should parse");

    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "code generation should succeed");
    require(strstr(generated, "pebble_time_now") != NULL,
            "the now runtime should be emitted");
    require(strstr(generated, "pebble_time_ticks") != NULL,
            "the ticks runtime should be emitted");
    require(strstr(generated, "pebble_time_delta") != NULL,
            "the delta runtime should be emitted");
    require(strstr(generated, "pebble_time_sleep") != NULL,
            "the sleep runtime should be emitted");
    require(strstr(generated, "pebble_time_unix") != NULL,
            "the unix runtime should be emitted");
    require(strstr(generated, "CLOCK_MONOTONIC") != NULL,
            "the clock should be a monotonic clock");
    require(strstr(generated, "pebble_time_origin") != NULL,
            "the clock should start at the start of the program");

    free(generated);
    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_unused_time_runtime_is_omitted(void)
{
    const char *text =
        "import package time\n"
        "fn main() { print(1); }\n";
    PebbleCSource source = {"packages_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    char *generated;

    require(pebble_c_parse_result_ok(&parsed), "time program should parse");

    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "code generation should succeed");
    require(strstr(generated, "pebble_time_now") == NULL,
            "an unused package function should not be emitted");
    require(strstr(generated, "pebble_time_origin") == NULL,
            "an unused clock should not be emitted");

    free(generated);
    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_time_package_rejects_wrong_argument_count(void)
{
    PebbleCTypeCheckResult result = check(
        "import package time\n"
        "fn main() { time.sleep(); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "a wrong argument count should be rejected");
    require(contains_message(&result, "expects 1 argument(s), but got 0"),
            "the diagnostic should report the argument count");
    pebble_c_type_check_result_destroy(&result);
}

static void test_time_package_rejects_unknown_function(void)
{
    PebbleCTypeCheckResult result = check(
        "import package time\n"
        "fn main() { print(time.frame()); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "an unknown package function should be rejected");
    require(contains_message(&result, "has no function 'frame'"),
            "the diagnostic should name the missing function");
    pebble_c_type_check_result_destroy(&result);
}

static void test_filesystem_package_functions_type_check(void)
{
    const char *text =
        "import package filesystem\n"
        "fn main() {\n"
        "    let contents = filesystem.read(\"note.txt\");\n"
        "    filesystem.write(\"note.txt\", contents);\n"
        "    let there = filesystem.exists(\"note.txt\");\n"
        "    let names = filesystem.list(\"notes\");\n"
        "    filesystem.mkdir(\"notes\");\n"
        "    print(contents, there, names);\n"
        "}\n";
    PebbleCTypeCheckResult result = check(text);

    require(pebble_c_type_check_result_ok(&result),
            "the filesystem package functions should type check");
    pebble_c_type_check_result_destroy(&result);
}

static void test_filesystem_package_runtime_is_generated(void)
{
    const char *text =
        "import package filesystem\n"
        "fn main() {\n"
        "    filesystem.write(\"note.txt\", \"hello\");\n"
        "    print(filesystem.read(\"note.txt\"));\n"
        "    print(filesystem.exists(\"note.txt\"));\n"
        "    print(filesystem.list(\"notes\"));\n"
        "    filesystem.mkdir(\"notes\");\n"
        "}\n";
    PebbleCSource source = {"packages_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    char *generated;

    require(pebble_c_parse_result_ok(&parsed), "filesystem program should parse");

    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "code generation should succeed");
    require(strstr(generated, "pebble_filesystem_read") != NULL,
            "the read runtime should be emitted");
    require(strstr(generated, "pebble_filesystem_write") != NULL,
            "the write runtime should be emitted");
    require(strstr(generated, "pebble_filesystem_exists") != NULL,
            "the exists runtime should be emitted");
    require(strstr(generated, "pebble_filesystem_list") != NULL,
            "the list runtime should be emitted");
    require(strstr(generated, "pebble_filesystem_mkdir") != NULL,
            "the mkdir runtime should be emitted");
    require(strstr(generated, "opendir") != NULL,
            "the listing should read the directory of the system");

    free(generated);
    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_unused_filesystem_runtime_is_omitted(void)
{
    const char *text =
        "import package filesystem\n"
        "fn main() { print(1); }\n";
    PebbleCSource source = {"packages_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    char *generated;

    require(pebble_c_parse_result_ok(&parsed), "filesystem program should parse");

    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "code generation should succeed");
    require(strstr(generated, "pebble_filesystem_read") == NULL,
            "an unused package function should not be emitted");
    require(strstr(generated, "pebble_filesystem_needs_path") == NULL,
            "an unused helper should not be emitted");

    free(generated);
    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_filesystem_package_rejects_wrong_argument_count(void)
{
    PebbleCTypeCheckResult result = check(
        "import package filesystem\n"
        "fn main() { print(filesystem.read()); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "a wrong argument count should be rejected");
    require(contains_message(&result, "expects 1 argument(s), but got 0"),
            "the diagnostic should report the argument count");
    pebble_c_type_check_result_destroy(&result);
}

static void test_filesystem_package_rejects_wrong_argument_type(void)
{
    PebbleCTypeCheckResult result = check(
        "import package filesystem\n"
        "fn main() { filesystem.write(\"note.txt\", 42); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "a wrong argument type should be rejected");
    require(contains_message(&result, "expects 'String', but got 'Int'"),
            "the diagnostic should report the argument type");
    pebble_c_type_check_result_destroy(&result);
}

static void test_filesystem_package_rejects_unknown_function(void)
{
    PebbleCTypeCheckResult result = check(
        "import package filesystem\n"
        "fn main() { print(filesystem.remove(\"note.txt\")); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "an unknown package function should be rejected");
    require(contains_message(&result, "has no function 'remove'"),
            "the diagnostic should name the missing function");
    pebble_c_type_check_result_destroy(&result);
}

static void test_integer_literal_range_is_checked(void)
{
    PebbleCTypeCheckResult result = check(
        "fn main() { let value = 99999999999999999999; print(value); }\n");

    require(!pebble_c_type_check_result_ok(&result),
            "an out of range literal should be rejected");
    require(contains_message(&result, "out of range"),
            "the diagnostic should report the literal range");
    pebble_c_type_check_result_destroy(&result);
}

static void test_minimum_integer_literal_is_accepted(void)
{
    PebbleCTypeCheckResult result = check(
        "fn main() { let value = -9223372036854775808; print(value); }\n");

    require(pebble_c_type_check_result_ok(&result),
            "the smallest Int literal should be accepted");
    pebble_c_type_check_result_destroy(&result);
}

static void test_log_package_functions_type_check(void)
{
    PebbleCTypeCheckResult result = check(
        "import package Log\n"
        "fn main() {\n"
        "    Log:Print(\"hello\")\n"
        "    Log:Warning(\"careful\")\n"
        "    Log:Error(\"failed\")\n"
        "}\n"
    );

    require(pebble_c_type_check_result_ok(&result),
            "Log Print, Warning, and Error should type check");
    pebble_c_type_check_result_destroy(&result);
}

static void test_log_package_runtime_is_generated(void)
{
    const char *text =
        "import package Log\n"
        "fn main() {\n"
        "    Log:Print(\"hello\")\n"
        "    Log:Warning(\"careful\")\n"
        "    Log:Error(\"failed\")\n"
        "}\n";
    PebbleCSource source = {"log_packages_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    PebbleCTypeCheckResult checked;
    char *generated;

    require(pebble_c_parse_result_ok(&parsed),
            "Log package program should parse");
    checked = pebble_c_type_check(parsed.root);
    require(pebble_c_type_check_result_ok(&checked),
            "Log package program should pass type checking");

    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "Log code generation should succeed");
    if (generated != NULL) {
        require(strstr(generated, "pebble_log_print(\"hello\")") != NULL,
                "Log:Print should emit its runtime call");
        require(strstr(generated, "pebble_log_warning(\"careful\")") != NULL,
                "Log:Warning should emit its runtime call");
        require(strstr(generated, "pebble_log_error(\"failed\")") != NULL,
                "Log:Error should emit its runtime call");
        require(strstr(generated, "static void pebble_log_print(") != NULL,
                "the Print runtime helper should be generated");
        require(strstr(generated, "Pebble warning: ") != NULL,
                "the warning prefix should be generated");
        require(strstr(generated, "Pebble error: ") != NULL,
                "the error prefix should be generated");
        free(generated);
    }

    pebble_c_type_check_result_destroy(&checked);
    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_unused_log_runtime_is_omitted(void)
{
    const char *text = "fn main() { print(\"ordinary output\"); }\n";
    PebbleCSource source = {"no_log_packages_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    char *generated;

    require(pebble_c_parse_result_ok(&parsed),
            "ordinary program should parse");
    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "ordinary code generation should succeed");
    if (generated != NULL) {
        require(strstr(generated, "static void pebble_log_print(") == NULL &&
                strstr(generated, "static void pebble_log_warning(") == NULL &&
                strstr(generated, "static void pebble_log_error(") == NULL,
                "unused Log runtime helpers should be omitted");
        free(generated);
    }

    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_log_package_rejects_unknown_function(void)
{
    PebbleCTypeCheckResult result = check(
        "import package Log\n"
        "fn main() {\n"
        "    Log:Info(\"hello\")\n"
        "}\n"
    );

    require(!pebble_c_type_check_result_ok(&result),
            "an unknown Log function should be rejected");
    require(contains_message(&result, "has no function 'Info'"),
            "the diagnostic should name the missing function");
    pebble_c_type_check_result_destroy(&result);
}

static void test_log_package_keeps_the_case_of_its_functions(void)
{
    PebbleCTypeCheckResult result = check(
        "import package Log\n"
        "fn main() {\n"
        "    Log:print(\"hello\")\n"
        "}\n"
    );

    require(!pebble_c_type_check_result_ok(&result),
            "a Log function with a different case should be rejected");
    require(contains_message(&result, "has no function 'print'"),
            "the diagnostic should name the function that was not found");
    pebble_c_type_check_result_destroy(&result);
}

static void test_log_package_rejects_wrong_argument_type(void)
{
    PebbleCTypeCheckResult result = check(
        "import package Log\n"
        "fn main() {\n"
        "    Log:Print(42)\n"
        "}\n"
    );

    require(!pebble_c_type_check_result_ok(&result),
            "a Log function with a number should be rejected");
    require(contains_message(&result, "expects 'String', but got 'Int'"),
            "the diagnostic should name the type that was expected");
    pebble_c_type_check_result_destroy(&result);
}

static void test_log_package_rejects_wrong_argument_count(void)
{
    PebbleCTypeCheckResult result = check(
        "import package Log\n"
        "fn main() {\n"
        "    Log:Print()\n"
        "}\n"
    );

    require(!pebble_c_type_check_result_ok(&result),
            "a Log function without a message should be rejected");
    require(contains_message(&result, "expects 1 argument(s), but got 0"),
            "the diagnostic should name the number of arguments");
    pebble_c_type_check_result_destroy(&result);
}

static void test_log_package_needs_an_import(void)
{
    PebbleCTypeCheckResult result = check(
        "fn main() {\n"
        "    Log:Print(\"hello\")\n"
        "}\n"
    );

    require(!pebble_c_type_check_result_ok(&result),
            "the Log package should need an import");
    require(contains_message(&result, "Unknown package 'Log'"),
            "the diagnostic should name the package that was not imported");
    pebble_c_type_check_result_destroy(&result);
}

static void test_log_package_keeps_the_case_of_its_name(void)
{
    PebbleCTypeCheckResult result = check(
        "import package log\n"
        "fn main() {\n"
        "    log:print(\"hello\")\n"
        "}\n"
    );

    require(!pebble_c_type_check_result_ok(&result),
            "the name of the Log package should keep its case");
    require(contains_message(&result, "Unknown package 'log'"),
            "the diagnostic should name the package that was not found");
    pebble_c_type_check_result_destroy(&result);
}

static void test_only_the_used_log_runtime_is_generated(void)
{
    const char *text =
        "import package Log\n"
        "fn main() {\n"
        "    Log:Print(\"hello\")\n"
        "}\n";
    PebbleCSource source = {"one_log_package_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    char *generated;

    require(pebble_c_parse_result_ok(&parsed),
            "a program with one Log call should parse");
    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "code generation should succeed");
    if (generated != NULL) {
        require(strstr(generated, "static void pebble_log_print(") != NULL,
                "the Print runtime helper should be generated");
        require(strstr(generated, "static void pebble_log_warning(") == NULL,
                "the warning helper should be left out when it is not used");
        require(strstr(generated, "static void pebble_log_error(") == NULL,
                "the error helper should be left out when it is not used");
        free(generated);
    }

    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_log_runtime_guards_against_a_null_string(void)
{
    const char *text =
        "import package Log\n"
        "fn main() {\n"
        "    Log:Print(\"hello\")\n"
        "}\n";
    PebbleCSource source = {"null_log_package_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    char *generated;

    require(pebble_c_parse_result_ok(&parsed),
            "a program with a Log call should parse");
    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "code generation should succeed");
    if (generated != NULL) {
        require(strstr(generated, "value == NULL ? \"null\" : value") != NULL,
                "the Log runtime should stand in for a missing string");
        free(generated);
    }

    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_log_runtime_keeps_the_streams_apart(void)
{
    const char *text =
        "import package Log\n"
        "fn main() {\n"
        "    Log:Print(\"hello\")\n"
        "    Log:Warning(\"careful\")\n"
        "    Log:Error(\"failed\")\n"
        "}\n";
    PebbleCSource source = {"streams_log_package_c.peb", text, strlen(text)};
    PebbleCTokenList tokens = pebble_c_lex(source);
    PebbleCParseResult parsed = pebble_c_parse(&tokens);
    char *generated;

    require(pebble_c_parse_result_ok(&parsed),
            "a program with a Log call should parse");
    generated = pebble_c_generate(parsed.root);
    require(generated != NULL, "code generation should succeed");
    if (generated != NULL) {
        require(strstr(generated, "value, stdout)") != NULL,
                "Print should write to the standard output");
        require(strstr(generated, "null\" : value, stderr)") != NULL,
                "the other messages should write to the standard error");
        require(strstr(generated, "Pebble warning: \", stderr") != NULL,
                "the warning should go to the standard error output");
        require(strstr(generated, "Pebble error: \", stderr") != NULL,
                "the error should go to the standard error output");
        free(generated);
    }

    pebble_c_parse_result_destroy(&parsed);
    pebble_c_tokens_destroy(&tokens);
}

static void test_package_registry_is_consistent(void)
{
    size_t count = 0U;
    const PebbleCPackage *math;
    const PebbleCPackageFunction *randint;

    (void)pebble_c_builtin_packages(&count);

    require(count > 0U, "the registry should not be empty");

    math = pebble_c_find_builtin_package("math");
    require(math != NULL, "the math package should be registered");
    require(pebble_c_find_builtin_package("physics") == NULL,
            "an unknown package should not resolve");

    randint = pebble_c_find_package_function(math, "randint");
    require(randint != NULL, "math.randint should be exported");
    require(randint->parameter_count == 2U,
            "math.randint should take two parameters");
    require(strcmp(randint->parameters[0], "Int") == 0 &&
            strcmp(randint->parameters[1], "Int") == 0,
            "math.randint should take integer parameters");
    require(strcmp(randint->return_type, "Int") == 0,
            "math.randint should return an integer");
    require(pebble_c_find_package_function(math, "cbrt") == NULL,
            "a missing export should not resolve");

    {
        const PebbleCPackage *text = pebble_c_find_builtin_package("string");
        const PebbleCPackageFunction *length;

        require(text != NULL, "the string package should be registered");

        length = pebble_c_find_package_function(text, "length");
        require(length != NULL, "string.length should be exported");
        require(length->parameter_count == 1U,
                "string.length should take one parameter");
        require(strcmp(length->parameters[0], "String") == 0,
                "string.length should take a string parameter");
        require(strcmp(length->return_type, "Int") == 0,
                "string.length should return an integer");
        require(pebble_c_find_package_function(text, "split") == NULL,
                "a missing export should not resolve");
    }

    {
        const PebbleCPackage *clock = pebble_c_find_builtin_package("time");
        const PebbleCPackageFunction *now;
        const PebbleCPackageFunction *sleep;
        const PebbleCPackageFunction *ticks;

        require(clock != NULL, "the time package should be registered");

        now = pebble_c_find_package_function(clock, "now");
        require(now != NULL, "time.now should be exported");
        require(now->parameter_count == 0U,
                "time.now should take no parameter");
        require(strcmp(now->return_type, "Float") == 0,
                "time.now should return a float");

        sleep = pebble_c_find_package_function(clock, "sleep");
        require(sleep != NULL, "time.sleep should be exported");
        require(sleep->parameter_count == 1U,
                "time.sleep should take one parameter");
        require(strcmp(sleep->parameters[0], "Float") == 0,
                "time.sleep should take a float parameter");
        require(strcmp(sleep->return_type, "Void") == 0,
                "time.sleep should not return a value");

        ticks = pebble_c_find_package_function(clock, "ticks");
        require(ticks != NULL, "time.ticks should be exported");
        require(strcmp(ticks->return_type, "Int") == 0,
                "time.ticks should return an integer");

        require(pebble_c_find_package_function(clock, "frame") == NULL,
                "a missing export should not resolve");
    }

    {
        const PebbleCPackage *files =
            pebble_c_find_builtin_package("filesystem");
        const PebbleCPackageFunction *read;
        const PebbleCPackageFunction *write;
        const PebbleCPackageFunction *exists;
        const PebbleCPackageFunction *list;
        const PebbleCPackageFunction *mkdir;

        require(files != NULL, "the filesystem package should be registered");

        read = pebble_c_find_package_function(files, "read");
        require(read != NULL, "filesystem.read should be exported");
        require(read->parameter_count == 1U,
                "filesystem.read should take one parameter");
        require(strcmp(read->parameters[0], "String") == 0,
                "filesystem.read should take a string parameter");
        require(strcmp(read->return_type, "String") == 0,
                "filesystem.read should return a string");

        write = pebble_c_find_package_function(files, "write");
        require(write != NULL, "filesystem.write should be exported");
        require(write->parameter_count == 2U,
                "filesystem.write should take two parameters");
        require(strcmp(write->parameters[0], "String") == 0 &&
                strcmp(write->parameters[1], "String") == 0,
                "filesystem.write should take string parameters");
        require(strcmp(write->return_type, "Void") == 0,
                "filesystem.write should not return a value");

        exists = pebble_c_find_package_function(files, "exists");
        require(exists != NULL, "filesystem.exists should be exported");
        require(strcmp(exists->return_type, "Bool") == 0,
                "filesystem.exists should return a boolean");

        list = pebble_c_find_package_function(files, "list");
        require(list != NULL, "filesystem.list should be exported");
        require(strcmp(list->return_type, "String") == 0,
                "filesystem.list should return a string");

        mkdir = pebble_c_find_package_function(files, "mkdir");
        require(mkdir != NULL, "filesystem.mkdir should be exported");
        require(strcmp(mkdir->return_type, "Void") == 0,
                "filesystem.mkdir should not return a value");

        require(pebble_c_find_package_function(files, "remove") == NULL,
                "a missing export should not resolve");
    }

    {
        const PebbleCPackage *log = pebble_c_find_builtin_package("Log");
        const char *const names[] = {"Print", "Warning", "Error"};
        const char *const symbols[] = {
            "pebble_log_print", "pebble_log_warning", "pebble_log_error"
        };
        size_t index;

        require(log != NULL, "the capitalized Log package should be registered");
        require(pebble_c_find_builtin_package("log") == NULL,
                "the Log package name should remain case-sensitive");

        if (log != NULL) {
            for (index = 0U; index < 3U; ++index) {
                const PebbleCPackageFunction *function =
                    pebble_c_find_package_function(log, names[index]);

                require(function != NULL, "each Log function should be exported");
                if (function != NULL) {
                    require(function->parameter_count == 1U,
                            "each Log function should take one parameter");
                    require(strcmp(function->parameters[0], "String") == 0,
                            "each Log function should take a String");
                    require(strcmp(function->return_type, "Void") == 0,
                            "each Log function should return Void");
                    require(strcmp(function->runtime_symbol, symbols[index]) == 0,
                            "each Log function should have the correct runtime symbol");
                }
            }

            require(pebble_c_find_package_function(log, "Info") == NULL,
                    "Log.Info should not be added without an API definition");
        }
    }
}

int main(void)
{
    test_import_keywords_are_lexed();
    test_import_declaration_is_parsed();
    test_member_expression_is_parsed();
    test_valid_package_call_type_checks();
    test_unknown_package_is_rejected();
    test_package_use_without_import_is_rejected();
    test_duplicate_import_is_rejected();
    test_unknown_package_function_is_rejected();
    test_wrong_package_argument_count_is_rejected();
    test_wrong_package_argument_type_is_rejected();
    test_package_function_value_is_rejected();
    test_import_inside_function_is_rejected();
    test_package_runtime_is_generated();
    test_float_package_functions_type_check();
    test_int_arguments_widen_to_float();
    test_float_package_runtime_is_generated();
    test_random_source_is_seeded_from_the_environment();
    test_unused_package_runtime_is_omitted();
    test_string_package_functions_type_check();
    test_string_package_runtime_is_generated();
    test_string_package_rejects_wrong_types();
    test_string_package_rejects_unknown_function();
    test_time_package_functions_type_check();
    test_time_package_runtime_is_generated();
    test_unused_time_runtime_is_omitted();
    test_time_package_rejects_wrong_argument_count();
    test_time_package_rejects_unknown_function();
    test_filesystem_package_functions_type_check();
    test_filesystem_package_runtime_is_generated();
    test_unused_filesystem_runtime_is_omitted();
    test_filesystem_package_rejects_wrong_argument_count();
    test_filesystem_package_rejects_wrong_argument_type();
    test_filesystem_package_rejects_unknown_function();
    test_log_package_functions_type_check();
    test_log_package_runtime_is_generated();
    test_unused_log_runtime_is_omitted();
    test_log_package_rejects_unknown_function();
    test_log_package_keeps_the_case_of_its_functions();
    test_log_package_rejects_wrong_argument_type();
    test_log_package_rejects_wrong_argument_count();
    test_log_package_needs_an_import();
    test_log_package_keeps_the_case_of_its_name();
    test_only_the_used_log_runtime_is_generated();
    test_log_runtime_guards_against_a_null_string();
    test_log_runtime_keeps_the_streams_apart();
    test_integer_literal_range_is_checked();
    test_minimum_integer_literal_is_accepted();
    test_package_registry_is_consistent();

    printf("Package tests passed.\n");
    return EXIT_SUCCESS;
}
