#include "pebble_c/codegen.h"
#include "pebble_c/lexer.h"
#include "pebble_c/module.h"
#include "pebble_c/parser.h"
#include "pebble_c/type_checker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void write_file(const char *path, const char *contents)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        fprintf(stderr, "FAIL: unable to write %s\n", path);
        exit(EXIT_FAILURE);
    }

    (void)fputs(contents, file);
    (void)fclose(file);
}

static void make_directory(const char *path)
{
    if (mkdir(path, 0755) != 0) {
        fprintf(stderr, "FAIL: unable to create %s\n", path);
        exit(EXIT_FAILURE);
    }
}

static void remove_directory(const char *path)
{
    char command[512];

    (void)snprintf(command, sizeof(command), "rm -rf '%s'", path);
    (void)system(command);
}

typedef struct {
    PebbleCParseResult parsed;
    PebbleCModuleResult modules;
    PebbleCTypeCheckResult checked;
} Program;

static Program load(const char *path, const char *contents,
                    const char *directory)
{
    Program result;
    PebbleCSource source;
    PebbleCTokenList tokens;
    size_t index;

    memset(&result, 0, sizeof(result));
    source.name = path;
    source.text = contents;
    source.length = strlen(contents);
    tokens = pebble_c_lex(source);
    result.parsed = pebble_c_parse(&tokens);

    if (!pebble_c_parse_result_ok(&result.parsed)) {
        fprintf(stderr, "Test setup failed during parsing.\n");
        exit(EXIT_FAILURE);
    }

    result.modules = pebble_c_resolve_modules(result.parsed.root, directory,
                                               NULL, 0U);

    for (index = 0U; index < result.modules.function_count; ++index) {
        if (!pebble_c_node_add(result.parsed.root,
                               result.modules.functions[index])) {
            fprintf(stderr, "FAIL: unable to merge package function\n");
            exit(EXIT_FAILURE);
        }
        result.modules.functions[index] = NULL;
    }

    result.checked = pebble_c_type_check(result.parsed.root);
    pebble_c_tokens_destroy(&tokens);
    return result;
}

static void destroy_program(Program *program)
{
    pebble_c_type_check_result_destroy(&program->checked);
    pebble_c_parse_result_destroy(&program->parsed);
    pebble_c_module_result_destroy(&program->modules);
}

static int contains_message(const PebbleCDiagnosticList *diagnostics,
                            const char *fragment)
{
    size_t index;

    for (index = 0U; index < diagnostics->count; ++index) {
        if (strstr(diagnostics->items[index].message, fragment) != NULL) {
            return 1;
        }
    }

    return 0;
}

static void test_package_next_to_the_importing_file_is_resolved(void)
{
    const char *workspace = "/tmp/pebble-local-package-c-sibling";
    const char *package = "/tmp/pebble-local-package-c-sibling/mylib.peb";
    const char *main = "/tmp/pebble-local-package-c-sibling/main.peb";
    const char *contents =
        "import package mylib\n"
        "fn main() { print(mylib.add(1, 2)); }\n";
    Program result;

    remove_directory(workspace);
    make_directory(workspace);
    write_file(package,
               "export fn add(a: Int, b: Int) -> Int { return a + b; }\n");
    write_file(main, contents);

    result = load(main, contents, workspace);

    require(result.modules.diagnostics.count == 0U,
            "the package should resolve");
    require(result.checked.diagnostics.count == 0U,
            "the program should type check");

    destroy_program(&result);
}

static void test_package_in_the_packages_directory_is_resolved(void)
{
    const char *workspace = "/tmp/pebble-local-package-c-directory";
    const char *packages = "/tmp/pebble-local-package-c-directory/packages";
    const char *main = "/tmp/pebble-local-package-c-directory/main.peb";
    const char *contents =
        "import package tools\n"
        "fn main() { print(tools.triple(5)); }\n";
    Program result;

    remove_directory(workspace);
    make_directory(workspace);
    make_directory(packages);
    write_file("/tmp/pebble-local-package-c-directory/packages/tools.peb",
               "export fn triple(x: Int) -> Int { return x * 3; }\n");
    write_file(main, contents);

    result = load(main, contents, workspace);

    require(result.modules.diagnostics.count == 0U,
            "the package should resolve");
    require(result.checked.diagnostics.count == 0U,
            "the program should type check");

    destroy_program(&result);
}

static void test_missing_package_is_reported(void)
{
    const char *workspace = "/tmp/pebble-local-package-c-missing";
    const char *main = "/tmp/pebble-local-package-c-missing/main.peb";
    const char *contents =
        "import package nothere\n"
        "fn main() { print(1); }\n";
    Program result;

    remove_directory(workspace);
    make_directory(workspace);
    write_file(main, contents);

    result = load(main, contents, workspace);

    require(result.modules.diagnostics.count > 0U,
            "the missing package should be reported");
    require(contains_message(&result.modules.diagnostics,
                             "Cannot find package 'nothere'"),
            "the missing package should be named");

    destroy_program(&result);
}

static void test_private_function_is_not_accessible(void)
{
    const char *workspace = "/tmp/pebble-local-package-c-private";
    const char *package = "/tmp/pebble-local-package-c-private/mylib.peb";
    const char *main = "/tmp/pebble-local-package-c-private/main.peb";
    const char *contents =
        "import package mylib\n"
        "fn main() { print(mylib.helper(1)); }\n";
    Program result;

    remove_directory(workspace);
    make_directory(workspace);
    write_file(package,
               "fn helper(x: Int) -> Int { return x * 2; }\n"
               "export fn add(a: Int, b: Int) -> Int { return helper(a) + b; }\n");
    write_file(main, contents);

    result = load(main, contents, workspace);

    require(result.checked.diagnostics.count > 0U,
            "the private function should be rejected");
    require(contains_message(&result.checked.diagnostics,
                             "Package 'mylib' has no function 'helper'."),
            "the private function should be reported");

    destroy_program(&result);
}

static void test_package_calls_its_own_functions(void)
{
    const char *workspace = "/tmp/pebble-local-package-c-internal";
    const char *package = "/tmp/pebble-local-package-c-internal/mylib.peb";
    const char *main = "/tmp/pebble-local-package-c-internal/main.peb";
    const char *contents =
        "import package mylib\n"
        "fn main() { print(mylib.add(1, 2)); }\n";
    Program result;

    remove_directory(workspace);
    make_directory(workspace);
    write_file(package,
               "fn helper(x: Int) -> Int { return x * 2; }\n"
               "export fn add(a: Int, b: Int) -> Int { return helper(a) + b; }\n");
    write_file(main, contents);

    result = load(main, contents, workspace);

    require(result.checked.diagnostics.count == 0U,
            "the package should call its own functions");

    destroy_program(&result);
}

static void test_nested_package_imports_are_resolved(void)
{
    const char *workspace = "/tmp/pebble-local-package-c-nested";
    const char *packages = "/tmp/pebble-local-package-c-nested/packages";
    const char *package = "/tmp/pebble-local-package-c-nested/outer.peb";
    const char *main = "/tmp/pebble-local-package-c-nested/main.peb";
    const char *contents =
        "import package outer\n"
        "fn main() { print(outer.compute(2)); }\n";
    Program result;

    remove_directory(workspace);
    make_directory(workspace);
    make_directory(packages);
    write_file(package,
               "import package tools\n"
               "fn helper(x: Int) -> Int { return tools.triple(x) + 1; }\n"
               "export fn compute(x: Int) -> Int { return helper(x); }\n");
    write_file("/tmp/pebble-local-package-c-nested/packages/tools.peb",
               "export fn triple(x: Int) -> Int { return x * 3; }\n");
    write_file(main, contents);

    result = load(main, contents, workspace);

    require(result.modules.diagnostics.count == 0U,
            "the nested package should resolve");
    require(result.checked.diagnostics.count == 0U,
            "the nested package should type check");

    destroy_program(&result);
}

static void test_package_functions_are_mangled(void)
{
    const char *workspace = "/tmp/pebble-local-package-c-mangling";
    const char *package = "/tmp/pebble-local-package-c-mangling/mylib.peb";
    const char *main = "/tmp/pebble-local-package-c-mangling/main.peb";
    const char *contents =
        "import package mylib\n"
        "fn main() { print(mylib.add(1, 2)); }\n";
    Program result;
    char *generated;

    remove_directory(workspace);
    make_directory(workspace);
    write_file(package,
               "export fn add(a: Int, b: Int) -> Int { return a + b; }\n");
    write_file(main, contents);

    result = load(main, contents, workspace);

    require(result.checked.diagnostics.count == 0U,
            "the program should type check");

    generated = pebble_c_generate(result.parsed.root);
    require(generated != NULL, "the program should generate code");
    require(strstr(generated, "pebble_fn_mylib_add") != NULL,
            "the package function should be mangled");

    free(generated);
    destroy_program(&result);
}

static void test_package_function_shadows_a_global_function(void)
{
    const char *workspace = "/tmp/pebble-local-package-c-shadowing";
    const char *package = "/tmp/pebble-local-package-c-shadowing/mylib.peb";
    const char *main = "/tmp/pebble-local-package-c-shadowing/main.peb";
    const char *contents =
        "import package mylib\n"
        "fn helper(x: Int) -> Int { return x * 100; }\n"
        "fn main() { print(mylib.compute(5)); }\n";
    Program result;

    remove_directory(workspace);
    make_directory(workspace);
    write_file(package,
               "fn helper(x: Int) -> Int { return x + 1; }\n"
               "export fn compute(x: Int) -> Int { return helper(x); }\n");
    write_file(main, contents);

    result = load(main, contents, workspace);

    require(result.checked.diagnostics.count == 0U,
            "the package should resolve its own function");

    destroy_program(&result);
}

static void test_type_error_in_package_keeps_the_package_location(void)
{
    const char *workspace = "/tmp/pebble-local-package-c-location";
    const char *package = "/tmp/pebble-local-package-c-location/mylib.peb";
    const char *main = "/tmp/pebble-local-package-c-location/main.peb";
    const char *contents =
        "import package mylib\n"
        "fn main() { print(mylib.add(1, 2)); }\n";
    Program result;

    remove_directory(workspace);
    make_directory(workspace);
    write_file(package,
               "export fn add(a: Int, b: Int) -> Int { return a + \"text\"; }\n");
    write_file(main, contents);

    result = load(main, contents, workspace);

    require(result.checked.diagnostics.count > 0U,
            "the type error should be reported");
    require(strcmp(result.checked.diagnostics.items[0].location.file,
                   package) == 0,
            "the diagnostic should point at the package file");

    destroy_program(&result);
}

int main(void)
{
    test_package_next_to_the_importing_file_is_resolved();
    test_package_in_the_packages_directory_is_resolved();
    test_missing_package_is_reported();
    test_private_function_is_not_accessible();
    test_package_calls_its_own_functions();
    test_nested_package_imports_are_resolved();
    test_package_function_shadows_a_global_function();
    test_package_functions_are_mangled();
    test_type_error_in_package_keeps_the_package_location();

    printf("Local package tests passed.\n");
    return EXIT_SUCCESS;
}
