#include "cli.h"
#include "pebble_c/ast.h"
#include "pebble_c/codegen.h"
#include "pebble_c/lexer.h"
#include "pebble_c/module.h"
#include "pebble_c/parser.h"
#include "pebble_c/type_checker.h"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <inttypes.h>

static void usage(const char *name) {
    (void)fprintf(stderr,
        "Pebble Native C 0.1.0\n"
        "Usage:\n"
        "  %s lex <file.peb>\n"
        "  %s parse <file.peb>\n"
        "  %s check [<file.peb>]\n"
        "  %s build [<file.peb>]\n"
        "  %s run [<file.peb>]\n"
        "  %s init [<name>]\n"
        "  %s add <path>\n"
        "  %s remove <package>\n"
        "  %s install\n"
        "  %s update\n"
        "  %s tree\n"
        "  %s package\n",
        name, name, name, name, name, name, name, name, name, name, name,
        name);
}

static char *copy_text(const char *text)
{
    const size_t length = strlen(text);
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1U);
    return copy;
}

static char *read_file(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    long file_size;
    char *buffer;

    if (file == NULL) {
        (void)fprintf(stderr, "error: cannot open '%s': %s\n",
            path, strerror(errno));
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0 ||
        (file_size = ftell(file)) < 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        (void)fprintf(stderr, "error: cannot determine source size\n");
        (void)fclose(file);
        return NULL;
    }

    if ((uintmax_t)file_size >= (uintmax_t)SIZE_MAX) {
        (void)fprintf(stderr, "error: source file is too large\n");
        (void)fclose(file);
        return NULL;
    }

    buffer = malloc((size_t)file_size + 1U);
    if (buffer == NULL) {
        (void)fclose(file);
        (void)fprintf(stderr, "error: out of memory\n");
        return NULL;
    }

    *length = fread(buffer, 1U, (size_t)file_size, file);
    if (*length != (size_t)file_size || ferror(file) != 0) {
        (void)fprintf(stderr, "error: failed reading '%s'\n", path);
        free(buffer);
        (void)fclose(file);
        return NULL;
    }

    buffer[*length] = '\0';
    if (fclose(file) != 0) {
        free(buffer);
        (void)fprintf(stderr, "error: failed closing '%s'\n", path);
        return NULL;
    }

    return buffer;
}

static void diagnostics(const PebbleCDiagnosticList *list,
    const char *fallback) {
    size_t i;

    for (i = 0U; i < list->count; ++i) {
        const PebbleCDiagnostic *item = &list->items[i];
        const char *file = item->location.file != NULL
            ? item->location.file : fallback;

        (void)fprintf(stderr, "%s:%zu:%zu: error: %s\n",
            file, item->location.line, item->location.column,
            item->message != NULL ? item->message : "unknown error");
    }
}

static void print_ast(const PebbleCNode *node, size_t depth) {
    size_t i;

    for (i = 0U; i < depth; ++i) {
        (void)fputs("  ", stdout);
    }

    (void)printf("%s", pebble_c_node_kind_name(node->kind));
    if (node->text != NULL && node->text[0] != '\0') {
        (void)printf(" `%s`", node->text);
    }
    if (node->kind == PEBBLE_C_NODE_FUNCTION && node->exported) {
        (void)printf(" exported");
    }
    (void)printf(" [%zu:%zu]\n",
        node->location.line, node->location.column);

    for (i = 0U; i < node->child_count; ++i) {
        print_ast(node->children[i], depth + 1U);
    }
}

static char *quote_argument(const char *value) {
    size_t length = strlen(value);
    size_t extra = 0U;
    size_t i;
    size_t position = 0U;
    char *result;

    for (i = 0U; i < length; ++i) {
        if (value[i] == '\'') {
            if (extra > (size_t)-1 - 3U) {
                return NULL;
            }
            extra += 3U;
        }
    }

    if (length > (size_t)-1 - extra - 3U) {
        return NULL;
    }

    result = malloc(length + extra + 3U);
    if (result == NULL) {
        return NULL;
    }

    result[position++] = '\'';
    for (i = 0U; i < length; ++i) {
        if (value[i] == '\'') {
            result[position++] = '\'';
            result[position++] = '\\';
            result[position++] = '\'';
            result[position++] = '\'';
        } else {
            result[position++] = value[i];
        }
    }
    result[position++] = '\'';
    result[position] = '\0';
    return result;
}

static char *output_path(const char *filename, const char *suffix) {
    const char *base = strrchr(filename, '/');
    const char *dot;
    size_t stem_length;
    size_t suffix_length = strlen(suffix);
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;
    char tag[17];
    int tag_length;
    size_t path_length;
    char *path;

    base = base != NULL ? base + 1 : filename;
    dot = strrchr(base, '.');
    stem_length = dot != NULL ? (size_t)(dot - base) : strlen(base);

    if (stem_length == 0U) {
        return NULL;
    }

    for (index = 0U; filename[index] != '\0'; ++index) {
        hash ^= (uint64_t)(unsigned char)filename[index];
        hash *= UINT64_C(1099511628211);
    }

    tag_length = snprintf(tag, sizeof(tag), "%016" PRIx64, hash);
    if (tag_length != 16) {
        return NULL;
    }

    if (stem_length > SIZE_MAX - suffix_length - 24U) {
        return NULL;
    }

    path_length = 6U + stem_length + 1U + 16U + suffix_length + 1U;
    path = malloc(path_length);
    if (path == NULL) {
        return NULL;
    }

    (void)memcpy(path, "build/", 6U);
    (void)memcpy(path + 6U, base, stem_length);
    path[6U + stem_length] = '-';
    (void)memcpy(path + 7U + stem_length, tag, 16U);
    (void)memcpy(
        path + 23U + stem_length,
        suffix,
        suffix_length + 1U
    );

    return path;
}

static int compile_c(const char *source_path, const char *executable_path) {
    const char *configured = getenv("CC");
    const char *compiler = configured != NULL && configured[0] != '\0'
        ? configured : "cc";
    char *q_compiler = quote_argument(compiler);
    char *q_source = quote_argument(source_path);
    char *q_executable = quote_argument(executable_path);
    const char *format =
        "%s -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror %s -o %s -lm";
    int needed;
    char *command;
    int status;

    if (q_compiler == NULL || q_source == NULL || q_executable == NULL) {
        free(q_compiler);
        free(q_source);
        free(q_executable);
        (void)fprintf(stderr, "error: out of memory preparing compiler command\n");
        return 0;
    }

    needed = snprintf(NULL, 0, format, q_compiler, q_source, q_executable);
    if (needed < 0) {
        free(q_compiler);
        free(q_source);
        free(q_executable);
        return 0;
    }

    command = malloc((size_t)needed + 1U);
    if (command == NULL) {
        free(q_compiler);
        free(q_source);
        free(q_executable);
        return 0;
    }

    (void)snprintf(command, (size_t)needed + 1U,
        format, q_compiler, q_source, q_executable);

    (void)printf("Compiling %s...\n", source_path);
    (void)fflush(stdout);

    status = system(command);

    free(command);
    free(q_compiler);
    free(q_source);
    free(q_executable);

    if (status != 0) {
        (void)fprintf(stderr, "error: native C compilation failed\n");
        return 0;
    }
    return 1;
}

static int compile_program(const char *command, const char *filename,
                           const char *const *search_paths,
                           size_t search_path_count)
{
    char *text = NULL;
    size_t length = 0U;
    PebbleCTokenList tokens = {0};
    PebbleCParseResult parsed = {0};
    PebbleCTypeCheckResult checked = {0};
    PebbleCModuleResult modules = {0};
    char *generated = NULL;
    char *generated_path = NULL;
    char *executable_path = NULL;
    FILE *file = NULL;
    int result = 1;

    text = read_file(filename, &length);
    if (text == NULL) {
        goto cleanup;
    }

    {
        PebbleCSource source = {filename, text, length};
        tokens = pebble_c_lex(source);
    }

    if (strcmp(command, "lex") == 0) {
        size_t i;

        for (i = 0U; i < tokens.count; ++i) {
            const PebbleCToken *token = &tokens.items[i];

            (void)printf("%zu:%zu  %s",
                token->location.line, token->location.column,
                pebble_c_token_kind_name(token->kind));
            if (token->lexeme != NULL && token->lexeme[0] != '\0') {
                (void)printf("  `%s`", token->lexeme);
            }
            (void)putchar('\n');
        }

        diagnostics(&tokens.diagnostics, filename);
        result = tokens.diagnostics.count == 0U ? 0 : 1;
        goto cleanup;
    }

    diagnostics(&tokens.diagnostics, filename);
    if (tokens.diagnostics.count != 0U) {
        goto cleanup;
    }

    parsed = pebble_c_parse(&tokens);

    if (strcmp(command, "parse") == 0) {
        if (parsed.root != NULL) {
            print_ast(parsed.root, 0U);
        }
        diagnostics(&parsed.diagnostics, filename);
        result = pebble_c_parse_result_ok(&parsed) != 0 ? 0 : 1;
        goto cleanup;
    }

    diagnostics(&parsed.diagnostics, filename);
    if (pebble_c_parse_result_ok(&parsed) == 0 || parsed.root == NULL) {
        goto cleanup;
    }

    {
        char directory[1024];
        const char *separator = strrchr(filename, '/');
        size_t directory_length;
        size_t index;

        if (separator == NULL) {
            directory[0] = '\0';
        } else {
            directory_length = (size_t)(separator - filename);
            if (directory_length >= sizeof(directory)) {
                directory_length = sizeof(directory) - 1U;
            }
            memcpy(directory, filename, directory_length);
            directory[directory_length] = '\0';
        }

        modules = (PebbleCModuleResult){0};

        modules = pebble_c_resolve_modules(parsed.root, directory,
                                           search_paths, search_path_count);

        if (!pebble_c_module_result_ok(&modules)) {
            diagnostics(&modules.diagnostics, filename);
            goto cleanup;
        }

        for (index = 0U; index < modules.function_count; ++index) {
            if (!pebble_c_node_add(parsed.root, modules.functions[index])) {
                (void)fprintf(stderr, "error: unable to merge package\n");
                goto cleanup;
            }
            modules.functions[index] = NULL;
        }
    }

    checked = pebble_c_type_check(parsed.root);
    diagnostics(&checked.diagnostics, filename);
    if (pebble_c_type_check_result_ok(&checked) == 0) {
        goto cleanup;
    }
    if (strcmp(command, "check") == 0) {
        (void)printf("Type check passed: %s\n", filename);
        result = 0;
        goto cleanup;
    }

    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        (void)fprintf(stderr, "error: cannot create build directory: %s\n",
            strerror(errno));
        goto cleanup;
    }

    generated_path = output_path(filename, ".generated.c");
    executable_path = output_path(filename, "");
    if (generated_path == NULL || executable_path == NULL) {
        (void)fprintf(stderr, "error: invalid source filename or out of memory\n");
        goto cleanup;
    }

    generated = pebble_c_generate(parsed.root);
    if (generated == NULL) {
        (void)fprintf(stderr, "error: C code generation failed\n");
        goto cleanup;
    }

    file = fopen(generated_path, "wb");
    if (file == NULL) {
        (void)fprintf(stderr, "error: cannot write '%s': %s\n",
            generated_path, strerror(errno));
        goto cleanup;
    }

    if (fputs(generated, file) == EOF) {
        (void)fprintf(stderr, "error: failed writing '%s'\n", generated_path);
        goto cleanup;
    }
    if (fclose(file) != 0) {
        file = NULL;
        (void)fprintf(stderr, "error: failed closing '%s'\n", generated_path);
        goto cleanup;
    }
    file = NULL;

    if (!compile_c(generated_path, executable_path)) {
        goto cleanup;
    }

    if (strcmp(command, "build") == 0) {
        (void)printf("Built native executable: %s\n", executable_path);
        result = 0;
        goto cleanup;
    }

    {
        size_t path_length = strlen(executable_path);
        char *relative = malloc(path_length + 3U);
        char *quoted;
        int status;

        if (relative == NULL) {
            (void)fprintf(stderr, "error: out of memory preparing executable path\n");
            goto cleanup;
        }

        (void)snprintf(relative, path_length + 3U, "./%s", executable_path);
        quoted = quote_argument(relative);
        free(relative);

        if (quoted == NULL) {
            (void)fprintf(stderr, "error: out of memory preparing executable command\n");
            goto cleanup;
        }

        status = system(quoted);
        free(quoted);
        result = status == 0 ? 0 : 1;
    }

cleanup:
    if (file != NULL) {
        (void)fclose(file);
    }
    free(executable_path);
    free(generated_path);
    free(generated);
    pebble_c_type_check_result_destroy(&checked);
    pebble_c_parse_result_destroy(&parsed);
    pebble_c_module_result_destroy(&modules);
    pebble_c_tokens_destroy(&tokens);
    free(text);
    return result;
}

static char *working_directory(void)
{
    return copy_text(".");
}

int main(int argc, char **argv) {
    const char *command;
    char *root;

    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    command = argv[1];
    root = working_directory();

    if (root == NULL) {
        (void)fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    if (strcmp(command, "init") == 0) {
        int result;

        if (argc > 3) {
            usage(argv[0]);
            free(root);
            return 2;
        }

        result = pebble_cli_init(root, argc == 3 ? argv[2] : NULL);
        free(root);
        return result;
    }

    if (strcmp(command, "add") == 0) {
        int result;

        if (argc != 3) {
            usage(argv[0]);
            free(root);
            return 2;
        }

        result = pebble_cli_add(root, argv[2]);
        free(root);
        return result;
    }

    if (strcmp(command, "remove") == 0) {
        int result;

        if (argc != 3) {
            usage(argv[0]);
            free(root);
            return 2;
        }

        result = pebble_cli_remove(root, argv[2]);
        free(root);
        return result;
    }

    if (strcmp(command, "install") == 0 || strcmp(command, "update") == 0) {
        int result;

        if (argc != 2) {
            usage(argv[0]);
            free(root);
            return 2;
        }

        result = strcmp(command, "install") == 0
            ? pebble_cli_install(root)
            : pebble_cli_update(root);
        free(root);
        return result;
    }

    if (strcmp(command, "tree") == 0) {
        int result;

        if (argc != 2) {
            usage(argv[0]);
            free(root);
            return 2;
        }

        result = pebble_cli_tree(root);
        free(root);
        return result;
    }

    if (strcmp(command, "package") == 0) {
        int result;

        if (argc != 2) {
            usage(argv[0]);
            free(root);
            return 2;
        }

        result = pebble_cli_package(root);
        free(root);
        return result;
    }

    if (strcmp(command, "lex") != 0 && strcmp(command, "parse") != 0 &&
        strcmp(command, "check") != 0 && strcmp(command, "build") != 0 &&
        strcmp(command, "run") != 0) {
        usage(argv[0]);
        free(root);
        return 2;
    }

    if (strcmp(command, "lex") == 0 || strcmp(command, "parse") == 0) {
        int result;

        if (argc != 3) {
            usage(argv[0]);
            free(root);
            return 2;
        }

        result = compile_program(command, argv[2], NULL, 0U);
        free(root);
        return result;
    }

    if (argc == 2) {
        PebbleCliProject project = {0};
        char message[1024];
        int result;

        if (!pebble_cli_load_project(root, &project, message,
                                     sizeof(message))) {
            (void)fprintf(stderr, "error: %s\n", message);
            free(root);
            return 1;
        }

        if (project.entry_point == NULL) {
            (void)fprintf(stderr,
                "error: the project has no entry point. Add 'src/main.peb' or "
                "'main.peb'.\n");
            pebble_cli_project_destroy(&project);
            free(root);
            return 1;
        }

        result = compile_program(command, project.entry_point,
                                 (const char *const *)project.search_paths,
                                 project.search_path_count);
        pebble_cli_project_destroy(&project);
        free(root);
        return result;
    }

    if (argc != 3) {
        usage(argv[0]);
        free(root);
        return 2;
    }

    {
        int result = compile_program(command, argv[2], NULL, 0U);

        free(root);
        return result;
    }
}
