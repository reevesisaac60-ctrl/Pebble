#include "pebble_c/codegen.h"
#include "pebble_c/package.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    int failed;
} Buffer;

typedef struct {
    const char *name;
    const char *type;
    size_t depth;
    size_t id;
} Symbol;

typedef enum {
    PEBBLE_C_PRINT_SEPARATOR = 1,
    PEBBLE_C_PRINT_END = 2,
    PEBBLE_C_PRINT_INT = 4,
    PEBBLE_C_PRINT_FLOAT = 8,
    PEBBLE_C_PRINT_BOOL = 16,
    PEBBLE_C_PRINT_STRING = 32,
    PEBBLE_C_PRINT_NULL = 64
} PebbleCPrintHelper;

#define PEBBLE_C_STRING_CONCAT 128

typedef struct {
    const PebbleCNode *program;
    Buffer *output;
    Symbol *symbols;
    size_t symbol_count;
    size_t symbol_capacity;
    size_t next_symbol_id;
    size_t depth;
    int in_main;
    int print_helpers;
    int string_concat;
    char current_package[256];
    const PebbleCPackage **imports;
    size_t import_count;
    size_t import_capacity;
    const PebbleCPackageFunction **package_functions;
    size_t package_function_count;
    size_t package_function_capacity;
} Generator;

static int reserve(Buffer *buffer, size_t extra)
{
    size_t required;
    size_t capacity;
    char *grown;

    if (buffer->failed || extra > (size_t)-1 - buffer->length - 1U) {
        buffer->failed = 1;
        return 0;
    }

    required = buffer->length + extra + 1U;
    if (required <= buffer->capacity) {
        return 1;
    }

    capacity = buffer->capacity == 0U ? 1024U : buffer->capacity;
    while (capacity < required) {
        if (capacity > (size_t)-1 / 2U) {
            capacity = required;
            break;
        }
        capacity *= 2U;
    }

    grown = realloc(buffer->data, capacity);
    if (grown == NULL) {
        buffer->failed = 1;
        return 0;
    }

    buffer->data = grown;
    buffer->capacity = capacity;
    return 1;
}

static void append_n(Buffer *buffer, const char *text, size_t length)
{
    if (text == NULL || !reserve(buffer, length)) {
        buffer->failed = 1;
        return;
    }

    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
}

static void append(Buffer *buffer, const char *text)
{
    if (text != NULL) {
        append_n(buffer, text, strlen(text));
    }
}

static void appendf(Buffer *buffer, const char *format, ...)
{
    va_list arguments;
    va_list copy;
    int needed;

    va_start(arguments, format);
    va_copy(copy, arguments);
    needed = vsnprintf(NULL, 0U, format, copy);
    va_end(copy);

    if (needed < 0 || !reserve(buffer, (size_t)needed)) {
        buffer->failed = 1;
        va_end(arguments);
        return;
    }

    (void)vsnprintf(
        buffer->data + buffer->length,
        buffer->capacity - buffer->length,
        format,
        arguments
    );
    buffer->length += (size_t)needed;
    va_end(arguments);
}

static void indent(Generator *generator)
{
    size_t index;

    for (index = 0U; index < generator->depth; ++index) {
        append(generator->output, "    ");
    }
}

static int equal(const char *left, const char *right)
{
    return left != NULL && right != NULL &&
        strcmp(left, right) == 0;
}

static int package_of(const char *name, char *buffer, size_t size)
{
    size_t index;

    if (name == NULL) {
        return 0;
    }

    for (index = 0U; name[index] != '\0'; ++index) {
        if (name[index] == '.') {
            if (index >= size) {
                return 0;
            }
            memcpy(buffer, name, index);
            buffer[index] = '\0';
            return 1;
        }
    }

    return 0;
}

static const PebbleCNode *find_child(
    const PebbleCNode *node,
    PebbleCNodeKind kind
)
{
    size_t index;

    if (node == NULL) {
        return NULL;
    }

    for (index = 0U; index < node->child_count; ++index) {
        if (node->children[index]->kind == kind) {
            return node->children[index];
        }
    }

    return NULL;
}

static const PebbleCNode *initializer(const PebbleCNode *node)
{
    size_t index;

    for (index = 0U; index < node->child_count; ++index) {
        if (node->children[index]->kind != PEBBLE_C_NODE_TYPE_NAME) {
            return node->children[index];
        }
    }

    return NULL;
}

static const char *type_name(const PebbleCNode *node)
{
    const PebbleCNode *type = find_child(node, PEBBLE_C_NODE_TYPE_NAME);
    return type == NULL ? NULL : type->text;
}

static const char *return_type(const PebbleCNode *function)
{
    const char *type = type_name(function);
    return type == NULL ? "Void" : type;
}

static const char *c_type(const char *type)
{
    if (equal(type, "Int")) {
        return "int64_t";
    }
    if (equal(type, "Float")) {
        return "double";
    }
    if (equal(type, "Bool")) {
        return "bool";
    }
    if (equal(type, "String") || equal(type, "Null")) {
        return "const char *";
    }
    if (equal(type, "Void")) {
        return "void";
    }

    return NULL;
}

static int c_type_is_pointer(const char *type)
{
    return equal(type, "String") || equal(type, "Null");
}

static const char *function_c_name(const char *name)
{
    return equal(name, "main") ? "main" : NULL;
}

static void append_function_name(Buffer *output, const char *name)
{
    size_t index;

    if (function_c_name(name) != NULL) {
        append(output, "main");
        return;
    }

    append(output, "pebble_fn_");

    for (index = 0U; name[index] != '\0'; ++index) {
        if (name[index] == '.') {
            append(output, "_");
        } else {
            char character[2];

            character[0] = name[index];
            character[1] = '\0';
            append(output, character);
        }
    }
}

static void append_variable_name(Buffer *output, const char *name)
{
    append(output, "pebble_var_");
    append(output, name);
}

static void append_variable_gen_name(
    Buffer *output,
    const char *name,
    size_t id
)
{
    append(output, "pebble_var_");
    append(output, name);
    if (id != 0U) {
        appendf(output, "_%zu", id);
    }
}

static const PebbleCNode *find_function(
    const Generator *generator,
    const char *name
)
{
    size_t index;

    for (index = 0U; index < generator->program->child_count; ++index) {
        const PebbleCNode *child = generator->program->children[index];

        if (child->kind == PEBBLE_C_NODE_FUNCTION &&
            equal(child->text, name)) {
            return child;
        }
    }

    return NULL;
}

static int add_import(Generator *generator, const PebbleCPackage *package)
{
    const PebbleCPackage **grown;
    size_t capacity;

    if (generator->import_count == generator->import_capacity) {
        capacity = generator->import_capacity == 0U ? 4U
            : generator->import_capacity * 2U;
        grown = realloc(
            generator->imports,
            capacity * sizeof(*generator->imports)
        );

        if (grown == NULL) {
            return 0;
        }

        generator->imports = grown;
        generator->import_capacity = capacity;
    }

    generator->imports[generator->import_count++] = package;
    return 1;
}

static int add_package_function(
    Generator *generator,
    const PebbleCPackageFunction *function
)
{
    const PebbleCPackageFunction **grown;
    size_t capacity;
    size_t index;

    for (index = 0U; index < generator->package_function_count; ++index) {
        if (equal(generator->package_functions[index]->runtime_symbol,
                  function->runtime_symbol)) {
            return 1;
        }
    }

    if (generator->package_function_count ==
        generator->package_function_capacity) {
        capacity = generator->package_function_capacity == 0U ? 4U
            : generator->package_function_capacity * 2U;
        grown = realloc(
            generator->package_functions,
            capacity * sizeof(*generator->package_functions)
        );

        if (grown == NULL) {
            return 0;
        }

        generator->package_functions = grown;
        generator->package_function_capacity = capacity;
    }

    generator->package_functions[generator->package_function_count++] =
        function;
    return 1;
}

static const PebbleCPackage *find_imported_package(
    const Generator *generator,
    const char *name
)
{
    size_t index;

    if (name == NULL) {
        return NULL;
    }

    for (index = 0U; index < generator->import_count; ++index) {
        if (equal(generator->imports[index]->name, name)) {
            return generator->imports[index];
        }
    }

    return NULL;
}

static const PebbleCPackageFunction *resolve_package_function(
    const Generator *generator,
    const PebbleCNode *callee
)
{
    const PebbleCPackage *package;

    if (callee == NULL || callee->child_count != 1U ||
        callee->children[0]->kind != PEBBLE_C_NODE_NAME_EXPRESSION) {
        return NULL;
    }

    package = find_imported_package(generator, callee->children[0]->text);

    if (package == NULL) {
        return NULL;
    }

    return pebble_c_find_package_function(package, callee->text);
}

static void collect_imports(Generator *generator, const PebbleCNode *program)
{
    size_t index;

    for (index = 0U; index < program->child_count; ++index) {
        const PebbleCNode *child = program->children[index];
        const PebbleCPackage *package;

        if (child->kind != PEBBLE_C_NODE_IMPORT_DECLARATION) {
            continue;
        }

        if (child->local) {
            continue;
        }

        package = pebble_c_find_builtin_package(child->text);

        if (package != NULL && !add_import(generator, package)) {
            generator->output->failed = 1;
        }
    }
}

static const PebbleCNode *find_program_function(
    const Generator *generator,
    const char *name
)
{
    size_t index;

    if (name == NULL) {
        return NULL;
    }

    for (index = 0U; index < generator->program->child_count; ++index) {
        const PebbleCNode *child = generator->program->children[index];

        if (child->kind == PEBBLE_C_NODE_FUNCTION &&
            equal(child->text, name)) {
            return child;
        }
    }

    return NULL;
}

static const PebbleCNode *find_package_function_node(
    const Generator *generator,
    const PebbleCNode *callee
)
{
    char qualified[256];

    if (callee == NULL || callee->child_count != 1U ||
        callee->children[0]->kind != PEBBLE_C_NODE_NAME_EXPRESSION) {
        return NULL;
    }

    if (snprintf(qualified, sizeof(qualified), "%s.%s",
                 callee->children[0]->text, callee->text) >=
        (int)sizeof(qualified)) {
        return NULL;
    }

    return find_program_function(generator, qualified);
}

static void collect_package_usage(Generator *generator,
                                  const PebbleCNode *node)
{
    size_t index;

    if (node->kind == PEBBLE_C_NODE_CALL_EXPRESSION &&
        node->child_count > 0U &&
        node->children[0]->kind == PEBBLE_C_NODE_MEMBER_EXPRESSION) {
        const PebbleCPackageFunction *function =
            resolve_package_function(generator, node->children[0]);

        if (function != NULL && !add_package_function(generator, function)) {
            generator->output->failed = 1;
        }
    }

    for (index = 0U; index < node->child_count; ++index) {
        collect_package_usage(generator, node->children[index]);
    }
}

static int uses_runtime_symbol(const Generator *generator,
                               const char *symbol)
{
    size_t index;

    for (index = 0U; index < generator->package_function_count; ++index) {
        if (equal(generator->package_functions[index]->runtime_symbol,
                  symbol)) {
            return 1;
        }
    }

    return 0;
}

static int is_string_symbol(const char *symbol)
{
    return strncmp(symbol, "pebble_string_", 14U) == 0;
}

static int uses_time_clock_symbol(const Generator *generator)
{
    return uses_runtime_symbol(generator, "pebble_time_now") ||
           uses_runtime_symbol(generator, "pebble_time_ticks") ||
           uses_runtime_symbol(generator, "pebble_time_delta");
}

static int uses_any_time_symbol(const Generator *generator)
{
    return uses_time_clock_symbol(generator) ||
           uses_runtime_symbol(generator, "pebble_time_sleep") ||
           uses_runtime_symbol(generator, "pebble_time_unix");
}

static int uses_filesystem_failure_symbol(const Generator *generator)
{
    return uses_runtime_symbol(generator, "pebble_filesystem_read") ||
           uses_runtime_symbol(generator, "pebble_filesystem_write") ||
           uses_runtime_symbol(generator, "pebble_filesystem_list") ||
           uses_runtime_symbol(generator, "pebble_filesystem_mkdir");
}

static int uses_any_filesystem_symbol(const Generator *generator)
{
    return uses_filesystem_failure_symbol(generator) ||
           uses_runtime_symbol(generator, "pebble_filesystem_exists");
}

static int uses_any_string_symbol(const Generator *generator)
{
    size_t index;

    for (index = 0U; index < generator->package_function_count; ++index) {
        if (is_string_symbol(generator->package_functions[index]->runtime_symbol)) {
            return 1;
        }
    }

    return 0;
}

static void append_package_runtime(Generator *generator, Buffer *prologue)
{
    size_t index;

    if (uses_runtime_symbol(generator, "pebble_math_randint") ||
        uses_runtime_symbol(generator, "pebble_math_random") ||
        uses_runtime_symbol(generator, "pebble_math_randomseed")) {
        append(prologue,
            "static uint64_t pebble_rng_state = 0;\n"
            "static int pebble_rng_ready = 0;\n"
            "\n"
            "static uint64_t pebble_rng_seed(void)\n"
            "{\n"
            "    uint64_t seed = (uint64_t)time(NULL);\n"
            "    seed ^= (uint64_t)clock() * UINT64_C(0x9E3779B97F4A7C15);\n"
            "    seed ^= (uint64_t)(uintptr_t)&pebble_rng_state;\n"
            "    return seed;\n"
            "}\n"
            "\n"
        );
    }

    if (uses_runtime_symbol(generator, "pebble_math_randint") ||
        uses_runtime_symbol(generator, "pebble_math_random")) {
        append(prologue,
            "static inline uint64_t pebble_rng_next(void)\n"
            "{\n"
            "    uint64_t z;\n"
            "\n"
            "    if (!pebble_rng_ready) {\n"
            "        pebble_rng_state = pebble_rng_seed();\n"
            "        pebble_rng_ready = 1;\n"
            "    }\n"
            "\n"
            "    z = (pebble_rng_state += UINT64_C(0x9E3779B97F4A7C15));\n"
            "    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);\n"
            "    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);\n"
            "    return z ^ (z >> 31);\n"
            "}\n"
            "\n"
        );
    }

    if (uses_runtime_symbol(generator, "pebble_math_randint")) {
        append(prologue,
            "static inline int64_t pebble_i64_from_u64(uint64_t value)\n"
            "{\n"
            "    int64_t result;\n"
            "    memcpy(&result, &value, sizeof(result));\n"
            "    return result;\n"
            "}\n"
            "\n"
        );
    }

    if (uses_any_string_symbol(generator)) {
        append(prologue,
            "static const char *pebble_string_text(const char *text)\n"
            "{\n"
            "    return text == NULL ? \"\" : text;\n"
            "}\n"
            "\n"
        );
    }

    if (uses_runtime_symbol(generator, "pebble_string_substring") ||
        uses_runtime_symbol(generator, "pebble_string_left") ||
        uses_runtime_symbol(generator, "pebble_string_right") ||
        uses_runtime_symbol(generator, "pebble_string_upper") ||
        uses_runtime_symbol(generator, "pebble_string_lower") ||
        uses_runtime_symbol(generator, "pebble_string_trim") ||
        uses_runtime_symbol(generator, "pebble_string_replace")) {
        append(prologue,
            "static char *pebble_string_copy(const char *text, size_t length)\n"
            "{\n"
            "    char *result = (char *)malloc(length + 1U);\n"
            "\n"
            "    if (result == NULL) {\n"
            "        (void)fputs(\"Pebble runtime error: out of memory\\n\", "
            "stderr);\n"
            "        exit(1);\n"
            "    }\n"
            "\n"
            "    if (length > 0U) {\n"
            "        (void)memcpy(result, text, length);\n"
            "    }\n"
            "\n"
            "    result[length] = '\\0';\n"
            "    return result;\n"
            "}\n"
            "\n"
        );
    }

    if (uses_time_clock_symbol(generator)) {
        append(prologue,
            "static struct timespec pebble_time_origin;\n"
            "static int pebble_time_started = 0;\n"
            "\n"
            "static void pebble_time_monotonic(struct timespec *value)\n"
            "{\n"
            "#if defined(CLOCK_MONOTONIC)\n"
            "    (void)clock_gettime(CLOCK_MONOTONIC, value);\n"
            "#else\n"
            "    (void)timespec_get(value, TIME_UTC);\n"
            "#endif\n"
            "}\n"
            "\n"
            "static void pebble_time_start(void)\n"
            "{\n"
            "    if (!pebble_time_started) {\n"
            "        pebble_time_monotonic(&pebble_time_origin);\n"
            "        pebble_time_started = 1;\n"
            "    }\n"
            "}\n"
            "\n"
        );
    }

    if (uses_runtime_symbol(generator, "pebble_time_now") ||
        uses_runtime_symbol(generator, "pebble_time_delta")) {
        append(prologue,
            "static double pebble_time_difference(\n"
            "    const struct timespec *from,\n"
            "    const struct timespec *to)\n"
            "{\n"
            "    return (double)(to->tv_sec - from->tv_sec) +\n"
            "           (double)(to->tv_nsec - from->tv_nsec) / 1000000000.0;\n"
            "}\n"
            "\n"
        );
    }

    if (uses_any_filesystem_symbol(generator)) {
        append(prologue,
            "static void pebble_filesystem_needs_path(const char *function)\n"
            "{\n"
            "    (void)fprintf(stderr,\n"
            "        \"Pebble runtime error: %s needs a path\\n\",\n"
            "        function);\n"
            "    exit(1);\n"
            "}\n"
            "\n"
        );
    }

    if (uses_filesystem_failure_symbol(generator)) {
        append(prologue,
            "static void pebble_filesystem_failed(\n"
            "    const char *function,\n"
            "    const char *action,\n"
            "    const char *path)\n"
            "{\n"
            "    (void)fprintf(stderr,\n"
            "        \"Pebble runtime error: %s %s '%s'\\n\",\n"
            "        function, action, path);\n"
            "    exit(1);\n"
            "}\n"
            "\n"
        );
    }

    for (index = 0U; index < generator->package_function_count; ++index) {
        const char *symbol =
            generator->package_functions[index]->runtime_symbol;

        if (equal(symbol, "pebble_math_randint")) {
            append(prologue,
                "static int64_t pebble_math_randint("
                "int64_t minimum, int64_t maximum)\n"
                "{\n"
                "    uint64_t range;\n"
                "    uint64_t limit;\n"
                "    uint64_t threshold;\n"
                "    uint64_t candidate;\n"
                "\n"
                "    if (minimum > maximum) {\n"
                "        (void)fprintf(stderr,\n"
                "            \"Pebble runtime error: math.randint requires "
                "min <= max (min=%lld, max=%lld)\\n\",\n"
                "            (long long)minimum, (long long)maximum);\n"
                "        exit(1);\n"
                "    }\n"
                "\n"
                "    range = (uint64_t)maximum - (uint64_t)minimum;\n"
                "\n"
                "    if (range == UINT64_MAX) {\n"
                "        candidate = pebble_rng_next();\n"
                "        return pebble_i64_from_u64("
                "(uint64_t)minimum + candidate);\n"
                "    }\n"
                "\n"
                "    limit = range + UINT64_C(1);\n"
                "    threshold = (UINT64_C(0) - limit) % limit;\n"
                "    candidate = pebble_rng_next();\n"
                "\n"
                "    while (candidate < threshold) {\n"
                "        candidate = pebble_rng_next();\n"
                "    }\n"
                "\n"
                "    return pebble_i64_from_u64("
                "(uint64_t)minimum + (candidate % limit));\n"
                "}\n"
                "\n"
            );
        } else if (equal(symbol, "pebble_math_abs")) {
            append(prologue,
                "static int64_t pebble_math_abs(int64_t value)\n"
                "{\n"
                "    if (value == INT64_MIN) {\n"
                "        (void)fputs(\"Pebble runtime error: math.abs is "
                "undefined for -9223372036854775808\\n\", stderr);\n"
                "        exit(1);\n"
                "    }\n"
                "\n"
                "    return value < 0 ? -value : value;\n"
                "}\n"
                "\n"
            );
        } else if (equal(symbol, "pebble_math_min")) {
            append(prologue,
                "static int64_t pebble_math_min(int64_t left, int64_t right)\n"
                "{\n"
                "    return left < right ? left : right;\n"
                "}\n"
                "\n"
            );
        } else if (equal(symbol, "pebble_math_max")) {
            append(prologue,
                "static int64_t pebble_math_max(int64_t left, int64_t right)\n"
                "{\n"
                "    return left > right ? left : right;\n"
                "}\n"
                "\n"
            );
        } else if (equal(symbol, "pebble_math_randomseed")) {
            append(prologue,
                "static void pebble_math_randomseed(int64_t seed)\n"
                "{\n"
                "    pebble_rng_state = (uint64_t)seed + "
                "UINT64_C(0x9E3779B97F4A7C15);\n"
                "    pebble_rng_ready = 1;\n"
                "}\n"
                "\n"
            );
        } else if (equal(symbol, "pebble_math_random")) {
            append(prologue,
                "static double pebble_math_random(void)\n"
                "{\n"
                "    return (double)(pebble_rng_next() >> 11) *\n"
                "        (1.0 / 9007199254740992.0);\n"
                "}\n"
                "\n"
            );
        } else if (equal(symbol, "pebble_math_sqrt")) {
            append(prologue,
                "static double pebble_math_sqrt(double value)\n"
                "{\n"
                "    if (value < 0.0) {\n"
                "        (void)fputs(\"Pebble runtime error: math.sqrt is "
                "undefined for negative values\\n\", stderr);\n"
                "        exit(1);\n"
                "    }\n"
                "\n"
                "    return sqrt(value);\n"
                "}\n"
                "\n"
            );
        } else if (equal(symbol, "pebble_math_floor")) {
            append(prologue,
                "static double pebble_math_floor(double value)\n"
                "{\n"
                "    return floor(value);\n"
                "}\n"
                "\n"
            );
        } else if (equal(symbol, "pebble_math_ceil")) {
            append(prologue,
                "static double pebble_math_ceil(double value)\n"
                "{\n"
                "    return ceil(value);\n"
                "}\n"
                "\n"
            );
        } else if (equal(symbol, "pebble_math_round")) {
            append(prologue,
                "static double pebble_math_round(double value)\n"
                "{\n"
                "    return round(value);\n"
                "}\n"
                "\n"
            );
        } else if (equal(symbol, "pebble_math_pi")) {
            append(prologue,
                "static double pebble_math_pi(void)\n"
                "{\n"
                "    return 3.14159265358979323846;\n"
                "}\n"
                "\n"
            );
        } else if (equal(symbol, "pebble_string_length")) {
            append(prologue,
            "static int64_t pebble_string_length(const char *text)\n"
            "{\n"
            "    return (int64_t)strlen(pebble_string_text(text));\n"
            "}");
        } else if (equal(symbol, "pebble_string_substring")) {
            append(prologue,
            "static char *pebble_string_substring(\n"
            "    const char *text, int64_t start, int64_t count)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    const size_t length = strlen(value);\n"
            "    size_t available;\n"
            "    size_t wanted;\n"
            "\n"
            "    if (start < 0 || (uint64_t)start > (uint64_t)length) {\n"
            "        (void)fprintf(stderr,\n"
            "            \"Pebble runtime error: string.substring is out of range \"\n"
            "            \"(start=%lld, length=%llu)\\n"
            "\",\n"
            "            (long long)start, (unsigned long long)length);\n"
            "        exit(1);\n"
            "    }\n"
            "\n"
            "    if (count < 0) {\n"
            "        (void)fprintf(stderr,\n"
            "            \"Pebble runtime error: string.substring needs a non \"\n"
            "            \"negative count (count=%lld)\\n"
            "\", (long long)count);\n"
            "        exit(1);\n"
            "    }\n"
            "\n"
            "    available = length - (size_t)start;\n"
            "    wanted = (size_t)count;\n"
            "\n"
            "    if (wanted > available) {\n"
            "        wanted = available;\n"
            "    }\n"
            "\n"
            "    return pebble_string_copy(value + (size_t)start, wanted);\n"
            "}");
        } else if (equal(symbol, "pebble_string_left")) {
            append(prologue,
            "static char *pebble_string_left(const char *text, int64_t count)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    const size_t length = strlen(value);\n"
            "    size_t wanted;\n"
            "\n"
            "    if (count < 0) {\n"
            "        (void)fprintf(stderr,\n"
            "            \"Pebble runtime error: string.left needs a non negative \"\n"
            "            \"count (count=%lld)\\n"
            "\", (long long)count);\n"
            "        exit(1);\n"
            "    }\n"
            "\n"
            "    wanted = (size_t)count;\n"
            "\n"
            "    if (wanted > length) {\n"
            "        wanted = length;\n"
            "    }\n"
            "\n"
            "    return pebble_string_copy(value, wanted);\n"
            "}");
        } else if (equal(symbol, "pebble_string_right")) {
            append(prologue,
            "static char *pebble_string_right(const char *text, int64_t count)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    const size_t length = strlen(value);\n"
            "    size_t wanted;\n"
            "\n"
            "    if (count < 0) {\n"
            "        (void)fprintf(stderr,\n"
            "            \"Pebble runtime error: string.right needs a non negative \"\n"
            "            \"count (count=%lld)\\n"
            "\", (long long)count);\n"
            "        exit(1);\n"
            "    }\n"
            "\n"
            "    wanted = (size_t)count;\n"
            "\n"
            "    if (wanted > length) {\n"
            "        wanted = length;\n"
            "    }\n"
            "\n"
            "    return pebble_string_copy(value + (length - wanted), wanted);\n"
            "}");
        } else if (equal(symbol, "pebble_string_upper")) {
            append(prologue,
            "static char *pebble_string_upper(const char *text)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    const size_t length = strlen(value);\n"
            "    char *result = pebble_string_copy(value, length);\n"
            "    size_t index;\n"
            "\n"
            "    for (index = 0U; index < length; ++index) {\n"
            "        result[index] = (char)toupper((unsigned char)result[index]);\n"
            "    }\n"
            "\n"
            "    return result;\n"
            "}");
        } else if (equal(symbol, "pebble_string_lower")) {
            append(prologue,
            "static char *pebble_string_lower(const char *text)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    const size_t length = strlen(value);\n"
            "    char *result = pebble_string_copy(value, length);\n"
            "    size_t index;\n"
            "\n"
            "    for (index = 0U; index < length; ++index) {\n"
            "        result[index] = (char)tolower((unsigned char)result[index]);\n"
            "    }\n"
            "\n"
            "    return result;\n"
            "}");
        } else if (equal(symbol, "pebble_string_trim")) {
            append(prologue,
            "static int pebble_string_is_space(char character)\n"
            "{\n"
            "    return character == ' ' || character == '\\t' ||\n"
            "           character == '\\n' || character == '\\r' ||\n"
            "           character == '\\f' || character == '\\v';\n"
            "}\n"
            "\n"
            "static char *pebble_string_trim(const char *text)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    const size_t length = strlen(value);\n"
            "    size_t start = 0U;\n"
            "    size_t end = length;\n"
            "\n"
            "    while (start < end && pebble_string_is_space(value[start])) {\n"
            "        ++start;\n"
            "    }\n"
            "\n"
            "    while (end > start && pebble_string_is_space(value[end - 1U])) {\n"
            "        --end;\n"
            "    }\n"
            "\n"
            "    return pebble_string_copy(value + start, end - start);\n"
            "}");
        } else if (equal(symbol, "pebble_string_contains")) {
            append(prologue,
            "static bool pebble_string_contains(const char *text, const char *needle)\n"
            "{\n"
            "    return strstr(pebble_string_text(text),\n"
            "                  pebble_string_text(needle)) != NULL;\n"
            "}");
        } else if (equal(symbol, "pebble_string_startswith")) {
            append(prologue,
            "static bool pebble_string_startswith(const char *text, const char *prefix)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    const char *start = pebble_string_text(prefix);\n"
            "\n"
            "    return strncmp(value, start, strlen(start)) == 0;\n"
            "}");
        } else if (equal(symbol, "pebble_string_endswith")) {
            append(prologue,
            "static bool pebble_string_endswith(const char *text, const char *suffix)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    const char *end = pebble_string_text(suffix);\n"
            "    const size_t value_length = strlen(value);\n"
            "    const size_t end_length = strlen(end);\n"
            "\n"
            "    if (end_length > value_length) {\n"
            "        return false;\n"
            "    }\n"
            "\n"
            "    return strncmp(value + (value_length - end_length), end,\n"
            "                  end_length) == 0;\n"
            "}");
        } else if (equal(symbol, "pebble_string_indexof")) {
            append(prologue,
            "static int64_t pebble_string_indexof(const char *text, const char *needle)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    const char *found = strstr(value, pebble_string_text(needle));\n"
            "\n"
            "    if (found == NULL) {\n"
            "        return -1;\n"
            "    }\n"
            "\n"
            "    return (int64_t)(found - value);\n"
            "}");
        } else if (equal(symbol, "pebble_string_replace")) {
            append(prologue,
            "static char *pebble_string_replace(\n"
            "    const char *text, const char *needle, const char *replacement)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    const char *search = pebble_string_text(needle);\n"
            "    const char *substitute = pebble_string_text(replacement);\n"
            "    const size_t search_length = strlen(search);\n"
            "    const size_t substitute_length = strlen(substitute);\n"
            "    size_t occurrences = 0U;\n"
            "    size_t position = 0U;\n"
            "    size_t offset = 0U;\n"
            "    size_t text_length = strlen(value);\n"
            "    size_t result_length;\n"
            "    char *result;\n"
            "\n"
            "    if (search_length == 0U) {\n"
            "        return pebble_string_copy(value, text_length);\n"
            "    }\n"
            "\n"
            "    for (;;) {\n"
            "        const char *found = strstr(value + position, search);\n"
            "\n"
            "        if (found == NULL) {\n"
            "            break;\n"
            "        }\n"
            "\n"
            "        ++occurrences;\n"
            "        position = (size_t)(found - value) + search_length;\n"
            "    }\n"
            "\n"
            "    result_length = text_length - occurrences * search_length +\n"
            "        occurrences * substitute_length;\n"
            "    result = (char *)malloc(result_length + 1U);\n"
            "\n"
            "    if (result == NULL) {\n"
            "        (void)fputs(\"Pebble runtime error: out of memory\\n"
            "\", stderr);\n"
            "        exit(1);\n"
            "    }\n"
            "\n"
            "    position = 0U;\n"
            "\n"
            "    for (;;) {\n"
            "        const char *found = strstr(value + position, search);\n"
            "        size_t chunk;\n"
            "\n"
            "        if (found == NULL) {\n"
            "            break;\n"
            "        }\n"
            "\n"
            "        chunk = (size_t)(found - value) - position;\n"
            "\n"
            "        if (chunk > 0U) {\n"
            "            (void)memcpy(result + offset, value + position, chunk);\n"
            "        }\n"
            "\n"
            "        if (substitute_length > 0U) {\n"
            "            (void)memcpy(result + offset + chunk, substitute,\n"
            "                         substitute_length);\n"
            "        }\n"
            "\n"
            "        offset += chunk + substitute_length;\n"
            "        position = (size_t)(found - value) + search_length;\n"
            "    }\n"
            "\n"
            "    if (text_length > position) {\n"
            "        (void)memcpy(result + offset, value + position, text_length - position);\n"
            "        offset += text_length - position;\n"
            "    }\n"
            "\n"
            "    result[offset] = '\\0';\n"
            "    return result;\n"
            "}");
        } else if (equal(symbol, "pebble_string_repeat")) {
            append(prologue,
            "static char *pebble_string_repeat(const char *text, int64_t count)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    const size_t length = strlen(value);\n"
            "    size_t total;\n"
            "    char *result;\n"
            "    int64_t index;\n"
            "\n"
            "    if (count < 0) {\n"
            "        (void)fprintf(stderr,\n"
            "            \"Pebble runtime error: string.repeat needs a non negative \"\n"
            "            \"count (count=%lld)\\n"
            "\", (long long)count);\n"
            "        exit(1);\n"
            "    }\n"
            "\n"
            "    total = length * (size_t)count;\n"
            "    result = (char *)malloc(total + 1U);\n"
            "\n"
            "    if (result == NULL) {\n"
            "        (void)fputs(\"Pebble runtime error: out of memory\\n"
            "\", stderr);\n"
            "        exit(1);\n"
            "    }\n"
            "\n"
            "    for (index = 0; index < count; ++index) {\n"
            "        (void)memcpy(result + (size_t)index * length, value, length);\n"
            "    }\n"
            "\n"
            "    result[total] = '\\0';\n"
            "    return result;\n"
            "}");
        } else if (equal(symbol, "pebble_string_toint")) {
            append(prologue,
            "static int64_t pebble_string_toint(const char *text)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    char *end = NULL;\n"
            "    long long parsed;\n"
            "\n"
            "    errno = 0;\n"
            "    parsed = strtoll(value, &end, 10);\n"
            "\n"
            "    if (end == value || *end != '\\0' || errno == ERANGE) {\n"
            "        (void)fprintf(stderr,\n"
            "            \"Pebble runtime error: string.toint cannot parse '%s'\\n"
            "\",\n"
            "            value);\n"
            "        exit(1);\n"
            "    }\n"
            "\n"
            "    return (int64_t)parsed;\n"
            "}");
        } else if (equal(symbol, "pebble_string_tofloat")) {
            append(prologue,
            "static double pebble_string_tofloat(const char *text)\n"
            "{\n"
            "    const char *value = pebble_string_text(text);\n"
            "    char *end = NULL;\n"
            "    double parsed;\n"
            "\n"
            "    errno = 0;\n"
            "    parsed = strtod(value, &end);\n"
            "\n"
            "    if (end == value || *end != '\\0' || errno == ERANGE) {\n"
            "        (void)fprintf(stderr,\n"
            "            \"Pebble runtime error: string.tofloat cannot parse '%s'\\n"
            "\",\n"
            "            value);\n"
            "        exit(1);\n"
            "    }\n"
            "\n"
            "    return parsed;\n"
            "}");
        } else if (equal(symbol, "pebble_time_now")) {
            append(prologue,
                "static double pebble_time_now(void)\n"
                "{\n"
                "    struct timespec current;\n"
                "\n"
                "    pebble_time_start();\n"
                "    pebble_time_monotonic(&current);\n"
                "    return pebble_time_difference(&pebble_time_origin, &current);\n"
                "}\n"
            );
        } else if (equal(symbol, "pebble_time_ticks")) {
            append(prologue,
                "static int64_t pebble_time_ticks(void)\n"
                "{\n"
                "    struct timespec current;\n"
                "    int64_t seconds;\n"
                "    int64_t nanoseconds;\n"
                "\n"
                "    pebble_time_start();\n"
                "    pebble_time_monotonic(&current);\n"
                "    seconds = (int64_t)(current.tv_sec - pebble_time_origin.tv_sec);\n"
                "    nanoseconds =\n"
                "        (int64_t)(current.tv_nsec - pebble_time_origin.tv_nsec);\n"
                "    return seconds * INT64_C(1000000000) + nanoseconds;\n"
                "}\n"
            );
        } else if (equal(symbol, "pebble_time_delta")) {
            append(prologue,
                "static double pebble_time_delta(void)\n"
                "{\n"
                "    static struct timespec last;\n"
                "    static int seen = 0;\n"
                "    struct timespec current;\n"
                "    double seconds = 0.0;\n"
                "\n"
                "    pebble_time_start();\n"
                "    pebble_time_monotonic(&current);\n"
                "\n"
                "    if (seen) {\n"
                "        seconds = pebble_time_difference(&last, &current);\n"
                "    }\n"
                "\n"
                "    last = current;\n"
                "    seen = 1;\n"
                "    return seconds;\n"
                "}\n"
            );
        } else if (equal(symbol, "pebble_time_sleep")) {
            append(prologue,
                "static void pebble_time_sleep(double seconds)\n"
                "{\n"
                "    struct timespec request;\n"
                "\n"
                "    if (seconds < 0.0) {\n"
                "        (void)fprintf(stderr,\n"
                "            \"Pebble runtime error: time.sleep needs a non negative "
                "duration (duration=%g)\\n\",\n"
                "            seconds);\n"
                "        exit(1);\n"
                "    }\n"
                "\n"
                "    request.tv_sec = (time_t)seconds;\n"
                "    request.tv_nsec =\n"
                "        (long)((seconds - (double)request.tv_sec) * 1000000000.0);\n"
                "\n"
                "    if (request.tv_nsec >= 1000000000L) {\n"
                "        request.tv_nsec -= 1000000000L;\n"
                "        request.tv_sec += 1;\n"
                "    }\n"
                "\n"
                "#if defined(__unix__) || defined(__APPLE__)\n"
                "    while (nanosleep(&request, &request) == -1 && errno == EINTR) {\n"
                "    }\n"
                "#else\n"
                "    {\n"
                "        struct timespec current;\n"
                "        struct timespec target;\n"
                "\n"
                "        (void)timespec_get(&current, TIME_UTC);\n"
                "        target.tv_sec = current.tv_sec + request.tv_sec;\n"
                "        target.tv_nsec = current.tv_nsec + request.tv_nsec;\n"
                "\n"
                "        if (target.tv_nsec >= 1000000000L) {\n"
                "            target.tv_nsec -= 1000000000L;\n"
                "            target.tv_sec += 1;\n"
                "        }\n"
                "\n"
                "        while ((double)(target.tv_sec - current.tv_sec) +\n"
                "               (double)(target.tv_nsec - current.tv_nsec) /\n"
                "                   1000000000.0 > 0.0) {\n"
                "            (void)timespec_get(&current, TIME_UTC);\n"
                "        }\n"
                "    }\n"
                "#endif\n"
                "}\n"
            );
        } else if (equal(symbol, "pebble_time_unix")) {
            append(prologue,
                "static int64_t pebble_time_unix(void)\n"
                "{\n"
                "    struct timespec current;\n"
                "\n"
                "    (void)timespec_get(&current, TIME_UTC);\n"
                "    return (int64_t)current.tv_sec;\n"
                "}\n"
            );
        } else if (equal(symbol, "pebble_filesystem_read")) {
            append(prologue,
                "static char *pebble_filesystem_read(const char *path)\n"
                "{\n"
                "    FILE *file;\n"
                "    struct stat information;\n"
                "    char *contents;\n"
                "    long size;\n"
                "    size_t read_bytes;\n"
                "\n"
                "    if (path == NULL || path[0] == '\\0') {\n"
                "        pebble_filesystem_needs_path(\"filesystem.read\");\n"
                "    }\n"
                "\n"
                "    if (stat(path, &information) == 0 &&\n"
                "        S_ISDIR(information.st_mode)) {\n"
                "        pebble_filesystem_failed(\n"
                "            \"filesystem.read\", \"cannot read\", path);\n"
                "    }\n"
                "\n"
                "    file = fopen(path, \"rb\");\n"
                "\n"
                "    if (file == NULL) {\n"
                "        pebble_filesystem_failed(\n"
                "            \"filesystem.read\", \"cannot read\", path);\n"
                "    }\n"
                "\n"
                "    if (fseek(file, 0L, SEEK_END) != 0) {\n"
                "        (void)fclose(file);\n"
                "        pebble_filesystem_failed(\n"
                "            \"filesystem.read\", \"cannot read\", path);\n"
                "    }\n"
                "\n"
                "    size = ftell(file);\n"
                "\n"
                "    if (size < 0) {\n"
                "        (void)fclose(file);\n"
                "        pebble_filesystem_failed(\n"
                "            \"filesystem.read\", \"cannot read\", path);\n"
                "    }\n"
                "\n"
                "    (void)rewind(file);\n"
                "\n"
                "    contents = (char *)malloc((size_t)size + 1U);\n"
                "\n"
                "    if (contents == NULL) {\n"
                "        (void)fclose(file);\n"
                "        (void)fputs(\"Pebble runtime error: out of memory\\n\", "
                "stderr);\n"
                "        exit(1);\n"
                "    }\n"
                "\n"
                "    read_bytes = fread(contents, 1U, (size_t)size, file);\n"
                "    (void)fclose(file);\n"
                "\n"
                "    if (read_bytes != (size_t)size) {\n"
                "        free(contents);\n"
                "        pebble_filesystem_failed(\n"
                "            \"filesystem.read\", \"cannot read\", path);\n"
                "    }\n"
                "\n"
                "    contents[(size_t)size] = '\\0';\n"
                "    return contents;\n"
                "}\n"
            );
        } else if (equal(symbol, "pebble_filesystem_write")) {
            append(prologue,
                "static void pebble_filesystem_write(\n"
                "    const char *path,\n"
                "    const char *contents)\n"
                "{\n"
                "    FILE *file;\n"
                "    size_t length;\n"
                "    size_t written;\n"
                "    int failed = 0;\n"
                "\n"
                "    if (path == NULL || path[0] == '\\0') {\n"
                "        pebble_filesystem_needs_path(\"filesystem.write\");\n"
                "    }\n"
                "\n"
                "    file = fopen(path, \"wb\");\n"
                "\n"
                "    if (file == NULL) {\n"
                "        pebble_filesystem_failed(\n"
                "            \"filesystem.write\", \"cannot write\", path);\n"
                "    }\n"
                "\n"
                "    length = contents == NULL ? 0U : strlen(contents);\n"
                "    written = length == 0U ? 0U : fwrite(contents, 1U, length, file);\n"
                "\n"
                "    if (written != length) {\n"
                "        failed = 1;\n"
                "    }\n"
                "\n"
                "    if (fclose(file) != 0) {\n"
                "        failed = 1;\n"
                "    }\n"
                "\n"
                "    if (failed != 0) {\n"
                "        pebble_filesystem_failed(\n"
                "            \"filesystem.write\", \"cannot write\", path);\n"
                "    }\n"
                "}\n"
            );
        } else if (equal(symbol, "pebble_filesystem_exists")) {
            append(prologue,
                "static bool pebble_filesystem_exists(const char *path)\n"
                "{\n"
                "    struct stat information;\n"
                "\n"
                "    if (path == NULL || path[0] == '\\0') {\n"
                "        pebble_filesystem_needs_path(\"filesystem.exists\");\n"
                "    }\n"
                "\n"
                "    return stat(path, &information) == 0 ? true : false;\n"
                "}\n"
            );
        } else if (equal(symbol, "pebble_filesystem_list")) {
            append(prologue,
                "static int pebble_filesystem_compare(\n"
                "    const void *left,\n"
                "    const void *right)\n"
                "{\n"
                "    const char *const *first = (const char *const *)left;\n"
                "    const char *const *second = (const char *const *)right;\n"
                "    return strcmp(*first, *second);\n"
                "}\n"
                "\n"
                "static char *pebble_filesystem_list(const char *path)\n"
                "{\n"
                "    char **names = NULL;\n"
                "    size_t count = 0U;\n"
                "    size_t capacity = 0U;\n"
                "    size_t total = 0U;\n"
                "    size_t offset = 0U;\n"
                "    size_t index;\n"
                "    char *listing;\n"
                "\n"
                "    if (path == NULL || path[0] == '\\0') {\n"
                "        pebble_filesystem_needs_path(\"filesystem.list\");\n"
                "    }\n"
                "\n"
#if defined(_WIN32)
                "    char search[8192];\n"
                "    WIN32_FIND_DATAA fd;\n"
                "    HANDLE h;\n"
                "    int have_entry = 0;\n"
                "    snprintf(search, sizeof(search), \"%s/*\", path);\n"
                "    h = FindFirstFileA(search, &fd);\n"
                "    if (h == INVALID_HANDLE_VALUE) {\n"
                "        if (GetLastError() != ERROR_FILE_NOT_FOUND) {\n"
                "            pebble_filesystem_failed(\n"
                "                \"filesystem.list\", \"cannot list\", path);\n"
                "        }\n"
                "    } else {\n"
                "        have_entry = 1;\n"
                "        while (have_entry) {\n"
                "        if (strcmp(fd.cFileName, \".\") == 0 ||\n"
                "            strcmp(fd.cFileName, \"..\") == 0) {\n"
                "            have_entry = FindNextFileA(h, &fd);\n"
                "            if (!have_entry) {\n"
                "                DWORD list_error = GetLastError();\n"
                "                FindClose(h);\n"
                "                if (list_error != ERROR_NO_MORE_FILES) {\n"
                "                    pebble_filesystem_failed(\n"
                "                        \"filesystem.list\", \"cannot list\", path);\n"
                "                }\n"
                "                continue;\n"
                "            }\n"
            "            continue;\n"
                "        }\n"
                "        if (count == capacity) {\n"
                "            size_t grown_capacity =\n"
                "                capacity == 0U ? 16U : capacity * 2U;\n"
                "            char **grown = (char **)realloc(\n"
                "                names, grown_capacity * sizeof(char *));\n"
                "            if (grown == NULL) {\n"
                "                FindClose(h);\n"
                "                (void)fputs(\"Pebble runtime error: out of memory\\n\", stderr);\n"
                "                exit(1);\n"
                "            }\n"
                "            names = grown;\n"
                "            capacity = grown_capacity;\n"
                "        }\n"
                "        {\n"
                "            size_t length = strlen(fd.cFileName);\n"
                "            names[count] = (char *)malloc(length + 1U);\n"
                "            if (names[count] == NULL) {\n"
                "                FindClose(h);\n"
                "                (void)fputs(\"Pebble runtime error: out of memory\\n\", stderr);\n"
                "                exit(1);\n"
                "            }\n"
                "            memcpy(names[count], fd.cFileName, length + 1U);\n"
                "            count++;\n"
                "        }\n"
                "            have_entry = FindNextFileA(h, &fd);\n"
                "            if (!have_entry) {\n"
                "                DWORD list_error = GetLastError();\n"
                "                FindClose(h);\n"
                "                if (list_error != ERROR_NO_MORE_FILES) {\n"
                "                    pebble_filesystem_failed(\n"
                "                        \"filesystem.list\", \"cannot list\", path);\n"
                "                }\n"
                "                continue;\n"
                "            }\n"
                "        }\n"
                "    }\n"
#else
                "    DIR *directory;\n"
                "    struct dirent *entry;\n"
                "    directory = opendir(path);\n"
                "    if (directory == NULL) {\n"
                "        pebble_filesystem_failed(\n"
                "            \"filesystem.list\", \"cannot list\", path);\n"
                "    }\n"
                "    while ((entry = readdir(directory)) != NULL) {\n"
                "        size_t length;\n"
                "        if (strcmp(entry->d_name, \".\") == 0 ||\n"
                "            strcmp(entry->d_name, \"..\") == 0) {\n"
                "            continue;\n"
                "        }\n"
                "        if (count == capacity) {\n"
                "            size_t grown_capacity =\n"
                "                capacity == 0U ? 16U : capacity * 2U;\n"
                "            char **grown = (char **)realloc(\n"
                "                names, grown_capacity * sizeof(char *));\n"
                "            if (grown == NULL) {\n"
                "                (void)closedir(directory);\n"
                "                (void)fputs(\"Pebble runtime error: out of memory\\n\", stderr);\n"
                "                exit(1);\n"
                "            }\n"
                "            names = grown;\n"
                "            capacity = grown_capacity;\n"
                "        }\n"
                "        length = strlen(entry->d_name);\n"
                "        names[count] = (char *)malloc(length + 1U);\n"
                "        if (names[count] == NULL) {\n"
                "            (void)closedir(directory);\n"
                "            (void)fputs(\"Pebble runtime error: out of memory\\n\", stderr);\n"
                "            exit(1);\n"
                "        }\n"
                "        memcpy(names[count], entry->d_name, length + 1U);\n"
                "        count++;\n"
                "    }\n"
                "    (void)closedir(directory);\n"
#endif
                "\n"
                "    if (count > 1U) {\n"
                "        qsort(names, count, sizeof(char *),\n"
                "              pebble_filesystem_compare);\n"
                "    }\n"
                "\n"
                "    for (index = 0U; index < count; index++) {\n"
                "        total += strlen(names[index]) + 1U;\n"
                "    }\n"
                "\n"
                "    listing = (char *)malloc(total + 1U);\n"
                "\n"
                "    if (listing == NULL) {\n"
                "        (void)fputs(\"Pebble runtime error: out of memory\\n\", "
                "stderr);\n"
                "        exit(1);\n"
                "    }\n"
                "\n"
                "    for (index = 0U; index < count; index++) {\n"
                "        size_t length = strlen(names[index]);\n"
                "\n"
                "        memcpy(listing + offset, names[index], length);\n"
                "        offset += length;\n"
                "        listing[offset] = '\\n';\n"
                "        offset++;\n"
                "        free(names[index]);\n"
                "    }\n"
                "\n"
                "    listing[offset] = '\\0';\n"
                "    free(names);\n"
                "    return listing;\n"
                "}\n"
            );
        } else if (equal(symbol, "pebble_filesystem_mkdir")) {
            append(prologue,
                "static void pebble_filesystem_mkdir(const char *path)\n"
                "{\n"
                "    struct stat information;\n"
                "    size_t length;\n"
                "    size_t index;\n"
                "    char *buffer;\n"
                "\n"
                "    if (path == NULL || path[0] == '\\0') {\n"
                "        pebble_filesystem_needs_path(\"filesystem.mkdir\");\n"
                "    }\n"
                "\n"
                "    if (stat(path, &information) == 0) {\n"
                "        pebble_filesystem_failed(\n"
                "            \"filesystem.mkdir\", \"cannot create\", path);\n"
                "    }\n"
                "\n"
                "    length = strlen(path);\n"
                "    buffer = (char *)malloc(length + 1U);\n"
                "\n"
                "    if (buffer == NULL) {\n"
                "        (void)fputs(\"Pebble runtime error: out of memory\\n\", "
                "stderr);\n"
                "        exit(1);\n"
                "    }\n"
                "\n"
                "    memcpy(buffer, path, length + 1U);\n"
                "\n"
#if defined(_WIN32)
                "    while (length > 1U && (buffer[length - 1U] == '/' || buffer[length - 1U] == '\\\\')) {\n"
                "        buffer[--length] = '\\0';\n"
                "    }\n"
#else
                "    while (length > 1U && buffer[length - 1U] == '/') {\n"
                "        buffer[--length] = '\\0';\n"
                "    }\n"
#endif
                "\n"
#if defined(_WIN32)
                "    for (index = 1U; index < length; index++) {\n"
                "        if (buffer[index] == '/' || buffer[index] == '\\\\') {\n"
#else
                "    for (index = 1U; index < length; index++) {\n"
                "        if (buffer[index] == '/') {\n"
#endif
                "            buffer[index] = '\\0';\n"
                "\n"
                "            if (mkdir(buffer, 0777) != 0 && errno != EEXIST) {\n"
                "                free(buffer);\n"
                "                pebble_filesystem_failed(\n"
                "                    \"filesystem.mkdir\", \"cannot create\", path);\n"
                "            }\n"
                "\n"
                "            buffer[index] = '/';\n"
                "        }\n"
                "    }\n"
                "\n"
                "    if (mkdir(buffer, 0777) != 0) {\n"
                "        free(buffer);\n"
                "        pebble_filesystem_failed(\n"
                "            \"filesystem.mkdir\", \"cannot create\", path);\n"
                "    }\n"
                "\n"
                "    free(buffer);\n"
                "}\n"
            );
        } else if (equal(symbol, "pebble_log_print")) {
            append(prologue,
                "static void pebble_log_print(const char *value)\n"
                "{\n"
                "    (void)fputs(value == NULL ? \"null\" : value, stdout);\n"
                "    (void)fputc('\\n', stdout);\n"
                "}\n"
            );
        } else if (equal(symbol, "pebble_log_warning")) {
            append(prologue,
                "static void pebble_log_warning(const char *value)\n"
                "{\n"
                "    (void)fflush(stdout);\n"
                "    (void)fputs(\"Pebble warning: \", stderr);\n"
                "    (void)fputs(value == NULL ? \"null\" : value, stderr);\n"
                "    (void)fputc('\\n', stderr);\n"
                "}\n"
            );
        } else if (equal(symbol, "pebble_log_error")) {
            append(prologue,
                "static void pebble_log_error(const char *value)\n"
                "{\n"
                "    (void)fflush(stdout);\n"
                "    (void)fputs(\"Pebble error: \", stderr);\n"
                "    (void)fputs(value == NULL ? \"null\" : value, stderr);\n"
                "    (void)fputc('\\n', stderr);\n"
                "}\n"
            );
        } else {
            generator->output->failed = 1;
        }
    }
}

static const char *parameter_type(
    const PebbleCNode *function,
    size_t index
)
{
    size_t seen = 0U;
    size_t child_index;

    if (function == NULL) {
        return NULL;
    }

    for (child_index = 0U;
         child_index < function->child_count;
         ++child_index) {
        const PebbleCNode *child = function->children[child_index];

        if (child->kind != PEBBLE_C_NODE_PARAMETER) {
            continue;
        }

        if (seen == index) {
            return type_name(child);
        }

        ++seen;
    }

    return NULL;
}

static const Symbol *find_symbol(
    const Generator *generator,
    const char *name
)
{
    size_t index = generator->symbol_count;

    while (index > 0U) {
        const Symbol *symbol = &generator->symbols[--index];

        if (equal(symbol->name, name)) {
            return symbol;
        }
    }

    return NULL;
}

static int add_symbol(
    Generator *generator,
    const char *name,
    const char *type,
    size_t id
)
{
    Symbol *grown;
    size_t capacity;

    if (generator->symbol_count == generator->symbol_capacity) {
        capacity = generator->symbol_capacity == 0U
            ? 16U
            : generator->symbol_capacity * 2U;

        if (capacity < generator->symbol_capacity ||
            capacity > (size_t)-1 / sizeof(*generator->symbols)) {
            return 0;
        }

        grown = realloc(
            generator->symbols,
            capacity * sizeof(*generator->symbols)
        );
        if (grown == NULL) {
            return 0;
        }

        generator->symbols = grown;
        generator->symbol_capacity = capacity;
    }

    generator->symbols[generator->symbol_count].name = name;
    generator->symbols[generator->symbol_count].type = type;
    generator->symbols[generator->symbol_count].depth = generator->depth;
    generator->symbols[generator->symbol_count].id = id;
    ++generator->symbol_count;
    return 1;
}

static const char *expression_type(
    const Generator *generator,
    const PebbleCNode *expression
)
{
    const char *left;
    const char *right;
    const PebbleCNode *callee;
    const PebbleCNode *function;
    if (expression == NULL) {
        return NULL;
    }

    switch (expression->kind) {
    case PEBBLE_C_NODE_LITERAL_EXPRESSION:
        if (equal(expression->text, "true") ||
            equal(expression->text, "false")) {
            return "Bool";
        }
        if (equal(expression->text, "null")) {
            return "Null";
        }
        if (expression->text != NULL &&
            expression->text[0] == '"') {
            return "String";
        }
        if (expression->text != NULL &&
            (strchr(expression->text, '.') != NULL ||
             strchr(expression->text, 'e') != NULL ||
             strchr(expression->text, 'E') != NULL)) {
            return "Float";
        }
        return "Int";

    case PEBBLE_C_NODE_NAME_EXPRESSION: {
        const Symbol *symbol = find_symbol(generator, expression->text);
        return symbol != NULL ? symbol->type : NULL;
    }

    case PEBBLE_C_NODE_UNARY_EXPRESSION:
        if (equal(expression->text, "!")) {
            return "Bool";
        }
        return expression->child_count == 1U
            ? expression_type(generator, expression->children[0])
            : NULL;

    case PEBBLE_C_NODE_BINARY_EXPRESSION:
        if (equal(expression->text, "==") ||
            equal(expression->text, "!=") ||
            equal(expression->text, "<") ||
            equal(expression->text, "<=") ||
            equal(expression->text, ">") ||
            equal(expression->text, ">=") ||
            equal(expression->text, "&&") ||
            equal(expression->text, "||")) {
            return "Bool";
        }

        if (expression->child_count != 2U) {
            return NULL;
        }

        left = expression_type(generator, expression->children[0]);
        right = expression_type(generator, expression->children[1]);

        if (equal(expression->text, "+") && equal(left, "String") &&
            equal(right, "String")) {
            return "String";
        }

        if (equal(left, "Float") || equal(right, "Float")) {
            return "Float";
        }
        return left;

    case PEBBLE_C_NODE_CALL_EXPRESSION:
        if (expression->child_count == 0U) {
            return NULL;
        }

        callee = expression->children[0];
        if (callee->kind == PEBBLE_C_NODE_MEMBER_EXPRESSION) {
            const PebbleCPackage *package =
                find_imported_package(generator, callee->children[0]->text);

            if (package != NULL) {
                const PebbleCPackageFunction *package_function =
                    pebble_c_find_package_function(package, callee->text);

                return package_function == NULL
                    ? NULL
                    : package_function->return_type;
            }

            {
                const PebbleCNode *local = find_package_function_node(
                    generator, callee);

                return local == NULL ? NULL : return_type(local);
            }
        }
        if (callee->kind != PEBBLE_C_NODE_NAME_EXPRESSION) {
            return NULL;
        }
        if (equal(callee->text, "print")) {
            return "Void";
        }

        function = find_function(generator, callee->text);
        return function == NULL ? NULL : return_type(function);

    default:
        return NULL;
    }
}

static const char *package_parameter_type(
    const PebbleCPackageFunction *function,
    size_t index
)
{
    if (function == NULL || index >= function->parameter_count) {
        return NULL;
    }

    return function->parameters[index];
}

static int is_minimum_integer_literal(const PebbleCNode *node)
{
    if (node == NULL || node->kind != PEBBLE_C_NODE_UNARY_EXPRESSION ||
        !equal(node->text, "-") || node->child_count != 1U) {
        return 0;
    }

    return node->children[0]->kind == PEBBLE_C_NODE_LITERAL_EXPRESSION &&
        equal(node->children[0]->text, "9223372036854775808");
}

static void emit_expression(Generator *, const PebbleCNode *);
static void emit_statement(Generator *, const PebbleCNode *);
static void emit_block(Generator *, const PebbleCNode *);

static void emit_converted_expression(
    Generator *generator,
    const PebbleCNode *node,
    const char *target_type
)
{
    Buffer *output = generator->output;
    const char *type = expression_type(generator, node);

    if (equal(target_type, "Float") && equal(type, "Int")) {
        append(output, "(double)(");
        emit_expression(generator, node);
        append(output, ")");
        return;
    }

    emit_expression(generator, node);
}

static void emit_print_value(
    Generator *generator,
    const PebbleCNode *argument
)
{
    Buffer *output = generator->output;
    const char *type = expression_type(generator, argument);

    if (equal(type, "Int")) {
        append(output, "pebble_print_int(");
        emit_expression(generator, argument);
        append(output, ")");
        generator->print_helpers |= PEBBLE_C_PRINT_INT;
        return;
    }

    if (equal(type, "Float")) {
        append(output, "pebble_print_float(");
        emit_expression(generator, argument);
        append(output, ")");
        generator->print_helpers |= PEBBLE_C_PRINT_FLOAT;
        return;
    }

    if (equal(type, "Bool")) {
        append(output, "pebble_print_bool(");
        emit_expression(generator, argument);
        append(output, ")");
        generator->print_helpers |= PEBBLE_C_PRINT_BOOL;
        return;
    }

    if (equal(type, "String")) {
        append(output, "pebble_print_string(");
        emit_expression(generator, argument);
        append(output, ")");
        generator->print_helpers |= PEBBLE_C_PRINT_STRING;
        return;
    }

    if (equal(type, "Null")) {
        append(output, "pebble_print_null()");
        generator->print_helpers |= PEBBLE_C_PRINT_NULL;
        return;
    }

    output->failed = 1;
}

static void emit_print_statement(
    Generator *generator,
    const PebbleCNode *call
)
{
    Buffer *output = generator->output;
    size_t index;

    for (index = 1U; index < call->child_count; ++index) {
        if (index > 1U) {
            indent(generator);
            append(output, "pebble_print_separator();\n");
            generator->print_helpers |= PEBBLE_C_PRINT_SEPARATOR;
        }

        indent(generator);
        emit_print_value(generator, call->children[index]);
        append(output, ";\n");
    }

    indent(generator);
    append(output, "pebble_print_end();\n");
    generator->print_helpers |= PEBBLE_C_PRINT_END;
}

static void emit_print_expression(
    Generator *generator,
    const PebbleCNode *call
)
{
    Buffer *output = generator->output;
    size_t index;

    for (index = 1U; index < call->child_count; ++index) {
        emit_print_value(generator, call->children[index]);
        append(output, ", ");

        if (index + 1U < call->child_count) {
            append(output, "pebble_print_separator(), ");
            generator->print_helpers |= PEBBLE_C_PRINT_SEPARATOR;
        }
    }

    append(output, "pebble_print_end()");
    generator->print_helpers |= PEBBLE_C_PRINT_END;
}

static void emit_expression(Generator *generator, const PebbleCNode *node)
{
    Buffer *output = generator->output;
    const char *left_type;
    const char *right_type;
    size_t index;

    if (node == NULL) {
        output->failed = 1;
        return;
    }

    switch (node->kind) {
    case PEBBLE_C_NODE_LITERAL_EXPRESSION:
        if (equal(node->text, "true") || equal(node->text, "false")) {
            append(output, node->text);
        } else if (equal(node->text, "null")) {
            append(output, "NULL");
        } else if (node->text != NULL && node->text[0] == '"') {
            append(output, node->text);
        } else if (equal(expression_type(generator, node), "Int")) {
            appendf(output, "INT64_C(%s)", node->text);
        } else {
            append(output, node->text);
        }
        return;

    case PEBBLE_C_NODE_NAME_EXPRESSION: {
        const Symbol *symbol = find_symbol(generator, node->text);
        append_variable_gen_name(
            output,
            node->text,
            symbol != NULL ? symbol->id : 0U
        );
        return;
    }

    case PEBBLE_C_NODE_UNARY_EXPRESSION:
        if (node->child_count != 1U) {
            output->failed = 1;
            return;
        }
        if (is_minimum_integer_literal(node)) {
            append(output, "(-INT64_C(9223372036854775807) - 1)");
            return;
        }
        append(output, "(");
        append(output, node->text);
        emit_expression(generator, node->children[0]);
        append(output, ")");
        return;

    case PEBBLE_C_NODE_BINARY_EXPRESSION:
        if (node->child_count != 2U) {
            output->failed = 1;
            return;
        }
        left_type = expression_type(generator, node->children[0]);
        right_type = expression_type(generator, node->children[1]);

        if (equal(node->text, "+") && equal(left_type, "String") &&
            equal(right_type, "String")) {
            generator->string_concat = 1;
            append(output, "pebble_string_concat(");
            emit_expression(generator, node->children[0]);
            append(output, ", ");
            emit_expression(generator, node->children[1]);
            append(output, ")");
            return;
        }

        append(output, "(");
        if (equal(right_type, "Float") && equal(left_type, "Int")) {
            append(output, "(double)(");
            emit_expression(generator, node->children[0]);
            append(output, ")");
        } else {
            emit_expression(generator, node->children[0]);
        }
        appendf(output, " %s ", node->text);
        if (equal(left_type, "Float") && equal(right_type, "Int")) {
            append(output, "(double)(");
            emit_expression(generator, node->children[1]);
            append(output, ")");
        } else {
            emit_expression(generator, node->children[1]);
        }
        append(output, ")");
        return;

    case PEBBLE_C_NODE_CALL_EXPRESSION:
        if (node->child_count == 0U) {
            output->failed = 1;
            return;
        }

        if (node->children[0]->kind == PEBBLE_C_NODE_MEMBER_EXPRESSION) {
            const PebbleCPackage *package;
            const PebbleCPackageFunction *package_function = NULL;
            const PebbleCNode *local_function = NULL;

            if (node->children[0]->child_count != 1U ||
                node->children[0]->children[0]->kind !=
                    PEBBLE_C_NODE_NAME_EXPRESSION) {
                output->failed = 1;
                return;
            }

            package = find_imported_package(
                generator, node->children[0]->children[0]->text);

            if (package != NULL) {
                package_function = pebble_c_find_package_function(
                    package, node->children[0]->text);

                if (package_function == NULL) {
                    output->failed = 1;
                    return;
                }

                append(output, package_function->runtime_symbol);
            } else {
                local_function = find_package_function_node(
                    generator, node->children[0]);

                if (local_function == NULL) {
                    output->failed = 1;
                    return;
                }

                append_function_name(output, local_function->text);
            }

            append(output, "(");
            for (index = 1U; index < node->child_count; ++index) {
                if (index > 1U) {
                    append(output, ", ");
                }
                emit_converted_expression(
                    generator,
                    node->children[index],
                    local_function != NULL
                        ? parameter_type(local_function, index - 1U)
                        : package_parameter_type(package_function, index - 1U)
                );
            }
            append(output, ")");
            return;
        }

        if (node->children[0]->kind != PEBBLE_C_NODE_NAME_EXPRESSION) {
            output->failed = 1;
            return;
        }

        if (equal(node->children[0]->text, "print")) {
            emit_print_expression(generator, node);
            return;
        }

        {
            const PebbleCNode *function = NULL;
            const PebbleCNode *package_function = NULL;

            if (generator->current_package[0] != '\0') {
                char qualified[256];

                if (snprintf(qualified, sizeof(qualified), "%s.%s",
                             generator->current_package,
                             node->children[0]->text) <
                    (int)sizeof(qualified)) {
                    package_function =
                        find_function(generator, qualified);
                }
            }

            if (package_function != NULL) {
                function = package_function;
                append_function_name(output, package_function->text);
            } else {
                function = find_function(generator, node->children[0]->text);
                append_function_name(output, node->children[0]->text);
            }

            append(output, "(");
            for (index = 1U; index < node->child_count; ++index) {
                if (index > 1U) {
                    append(output, ", ");
                }
                emit_converted_expression(
                    generator,
                    node->children[index],
                    parameter_type(function, index - 1U)
                );
            }
            append(output, ")");
        }
        return;

    default:
        output->failed = 1;
        return;
    }
}

static void emit_if(Generator *generator, const PebbleCNode *node, int indent_first)
{
    Buffer *output = generator->output;

    if (node->child_count < 2U) {
        output->failed = 1;
        return;
    }

    if (indent_first) {
        indent(generator);
    }

    append(output, "if (");
    emit_expression(generator, node->children[0]);
    append(output, ") ");
    emit_block(generator, node->children[1]);

    if (node->child_count >= 3U) {
        const PebbleCNode *branch = node->children[2];
        append(output, " else ");

        if (branch->kind == PEBBLE_C_NODE_BLOCK) {
            emit_block(generator, branch);
        } else if (branch->kind == PEBBLE_C_NODE_IF_STATEMENT) {
            emit_if(generator, branch, 0);
            return;
        } else {
            output->failed = 1;
            return;
        }
    }

    append(output, "\n");
}

static void emit_block(Generator *generator, const PebbleCNode *block)
{
    size_t index;
    size_t previous_count = generator->symbol_count;

    append(generator->output, "{\n");
    ++generator->depth;

    for (index = 0U; index < block->child_count; ++index) {
        emit_statement(generator, block->children[index]);
    }

    --generator->depth;
    while (generator->symbol_count > previous_count) {
        --generator->symbol_count;
    }
    indent(generator);
    append(generator->output, "}");
}

static void emit_parameter_uses(Generator *generator,
                                const PebbleCNode *function)
{
    size_t index;

    if (function == NULL) {
        return;
    }

    for (index = 0U; index < function->child_count; ++index) {
        const PebbleCNode *parameter = function->children[index];

        if (parameter->kind != PEBBLE_C_NODE_PARAMETER) {
            continue;
        }

        indent(generator);
        append(generator->output, "(void)");
        append_variable_name(generator->output, parameter->text);
        append(generator->output, ";\n");
    }
}

static void emit_statement(Generator *generator, const PebbleCNode *node)
{
    Buffer *output = generator->output;
    const PebbleCNode *value;
    const char *pebble_type;
    const char *declaration;

    switch (node->kind) {
    case PEBBLE_C_NODE_BLOCK:
        indent(generator);
        emit_block(generator, node);
        append(output, "\n");
        return;

    case PEBBLE_C_NODE_LET_DECLARATION:
    case PEBBLE_C_NODE_VAR_DECLARATION:
        value = initializer(node);
        if (value == NULL) {
            output->failed = 1;
            return;
        }

        pebble_type = type_name(node);
        if (pebble_type == NULL) {
            pebble_type = expression_type(generator, value);
        }
        if (pebble_type == NULL) {
            output->failed = 1;
            return;
        }

        declaration = c_type(pebble_type);
        if (declaration == NULL) {
            output->failed = 1;
            return;
        }

        indent(generator);
        if (node->kind == PEBBLE_C_NODE_LET_DECLARATION &&
            !c_type_is_pointer(pebble_type)) {
            append(output, "const ");
        }
        append(output, declaration);
        append(output, " ");

        {
            const Symbol *outer = find_symbol(generator, node->text);
            size_t id = outer != NULL ? ++generator->next_symbol_id : 0U;

            append_variable_gen_name(output, node->text, id);
            append(output, " = ");
            emit_converted_expression(generator, value, pebble_type);
            append(output, ";\n");

            indent(generator);
            append(output, "(void)");
            append_variable_gen_name(output, node->text, id);
            append(output, ";\n");

            if (!add_symbol(generator, node->text, pebble_type, id)) {
                output->failed = 1;
            }
        }
        return;

    case PEBBLE_C_NODE_ASSIGNMENT:
        if (node->child_count != 1U) {
            output->failed = 1;
            return;
        }
        {
            const Symbol *symbol = find_symbol(generator, node->text);
            indent(generator);
            append_variable_gen_name(
                output,
                node->text,
                symbol != NULL ? symbol->id : 0U
            );
            append(output, " = ");
            emit_converted_expression(
                generator,
                node->children[0],
                symbol != NULL ? symbol->type : NULL
            );
            append(output, ";\n");
        }
        return;

    case PEBBLE_C_NODE_EXPRESSION_STATEMENT:
        if (node->child_count != 1U) {
            output->failed = 1;
            return;
        }
        if (node->children[0]->kind == PEBBLE_C_NODE_ASSIGNMENT) {
            emit_statement(generator, node->children[0]);
            return;
        }
        if (node->children[0]->kind == PEBBLE_C_NODE_CALL_EXPRESSION &&
            node->children[0]->child_count > 0U &&
            node->children[0]->children[0]->kind == PEBBLE_C_NODE_NAME_EXPRESSION &&
            equal(node->children[0]->children[0]->text, "print")) {
            emit_print_statement(generator, node->children[0]);
            return;
        }
        indent(generator);
        emit_expression(generator, node->children[0]);
        append(output, ";\n");
        return;

    case PEBBLE_C_NODE_RETURN_STATEMENT:
        indent(generator);
        append(output, "return");
        if (node->child_count != 0U) {
            append(output, " ");
            emit_expression(generator, node->children[0]);
        } else if (generator->in_main) {
            append(output, " 0");
        }
        append(output, ";\n");
        return;

    case PEBBLE_C_NODE_IF_STATEMENT:
        emit_if(generator, node, 1);
        return;

    case PEBBLE_C_NODE_WHILE_STATEMENT:
        if (node->child_count != 2U) {
            output->failed = 1;
            return;
        }
        indent(generator);
        append(output, "while (");
        emit_expression(generator, node->children[0]);
        append(output, ") ");
        emit_block(generator, node->children[1]);
        append(output, "\n");
        return;

    default:
        output->failed = 1;
        return;
    }
}

static void emit_function_signature(
    Generator *generator,
    const PebbleCNode *function,
    int prototype
)
{
    Buffer *output = generator->output;
    const char *type = c_type(return_type(function));
    size_t index;
    int first = 1;

    if (equal(function->text, "main")) {
        append(output, "int main(void)");
        if (prototype) {
            append(output, ";\n");
        }
        return;
    }

    if (type == NULL) {
        output->failed = 1;
        return;
    }

    append(output, type);
    append(output, " ");
    append_function_name(output, function->text);
    append(output, "(");

    for (index = 0U; index < function->child_count; ++index) {
        const PebbleCNode *parameter = function->children[index];
        const char *parameter_c_type;

        if (parameter->kind != PEBBLE_C_NODE_PARAMETER) {
            continue;
        }

        parameter_c_type = c_type(type_name(parameter));
        if (parameter_c_type == NULL) {
            output->failed = 1;
            return;
        }

        if (!first) {
            append(output, ", ");
        }
        first = 0;
        append(output, parameter_c_type);
        append(output, " ");
        append_variable_name(output, parameter->text);
    }

    if (first) {
        append(output, "void");
    }

    append(output, ")");
    if (prototype) {
        append(output, ";\n");
    }
}

static void emit_print_helpers(Generator *generator, Buffer *prologue)
{
    append(prologue,
        "#include <stdbool.h>\n"
        "#include <stdint.h>\n"
        "#include <stddef.h>\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n"
        "#include <ctype.h>\n"
        "#include <errno.h>\n"
        "#include <math.h>\n"
        "#include <time.h>\n"
        "\n"
    );

    if (uses_any_filesystem_symbol(generator)) {
        append(prologue,
            "#if defined(_WIN32)\n"
            "#ifndef WIN32_LEAN_AND_MEAN\n"
            "#define WIN32_LEAN_AND_MEAN\n"
            "#endif\n"
            "#ifndef NOMINMAX\n"
            "#define NOMINMAX\n"
            "#endif\n"
            "#include <windows.h>\n"
            "#include <direct.h>\n"
            "#include <io.h>\n"
            "#include <sys/stat.h>\n"
            "#include <errno.h>\n"
            "#ifndef S_ISDIR\n"
            "#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)\n"
            "#endif\n"
            "#ifndef S_ISREG\n"
            "#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)\n"
            "#endif\n"
            "#ifndef mkdir\n"
            "#define mkdir(path, mode) _mkdir(path)\n"
            "#endif\n"
            "#else\n"
            "#include <dirent.h>\n"
            "#include <sys/stat.h>\n"
            "#include <errno.h>\n"
            "#include <unistd.h>\n"
            "#endif\n"
            "\n"
        );
    }

    if (generator->print_helpers & PEBBLE_C_PRINT_SEPARATOR) {
        append(prologue,
            "static void pebble_print_separator(void) "
            "{ (void)putchar(' '); }\n"
        );
    }

    if (generator->print_helpers & PEBBLE_C_PRINT_END) {
        append(prologue,
            "static void pebble_print_end(void) "
            "{ (void)putchar('\\n'); }\n"
        );
    }

    if (generator->print_helpers & PEBBLE_C_PRINT_INT) {
        append(prologue,
            "static void pebble_print_int(int64_t value) "
            "{ (void)printf(\"%lld\", (long long)value); }\n"
        );
    }

    if (generator->print_helpers & PEBBLE_C_PRINT_FLOAT) {
        append(prologue,
            "static void pebble_print_float(double value) "
            "{ (void)printf(\"%g\", value); }\n"
        );
    }

    if (generator->print_helpers & PEBBLE_C_PRINT_BOOL) {
        append(prologue,
            "static void pebble_print_bool(bool value) "
            "{ (void)fputs(value ? \"true\" : \"false\", stdout); }\n"
        );
    }

    if (generator->print_helpers & PEBBLE_C_PRINT_STRING) {
        append(prologue,
            "static void pebble_print_string(const char *value) "
            "{ (void)fputs(value == NULL ? \"null\" : value, stdout); }\n"
        );
    }

    if (generator->print_helpers & PEBBLE_C_PRINT_NULL) {
        append(prologue,
            "static void pebble_print_null(void) "
            "{ (void)fputs(\"null\", stdout); }\n"
        );
    }

    if (generator->string_concat) {
        append(prologue,
            "static char *pebble_string_concat("
            "const char *left, const char *right)\n"
            "{\n"
            "    size_t left_length = left == NULL ? 0U : strlen(left);\n"
            "    size_t right_length = right == NULL ? 0U : strlen(right);\n"
            "    char *result = (char *)malloc(left_length + right_length + 1U);\n"
            "\n"
            "    if (result == NULL) {\n"
            "        (void)fputs(\"Pebble runtime error: out of memory\\n\", "
            "stderr);\n"
            "        exit(1);\n"
            "    }\n"
            "\n"
            "    if (left_length > 0U) {\n"
            "        (void)memcpy(result, left, left_length);\n"
            "    }\n"
            "\n"
            "    if (right_length > 0U) {\n"
            "        (void)memcpy(result + left_length, right, right_length);\n"
            "    }\n"
            "\n"
            "    result[left_length + right_length] = '\\0';\n"
            "    return result;\n"
            "}\n"
        );
    }
}

char *pebble_c_generate(const PebbleCNode *program)
{
    Generator generator = {0};
    Buffer body = {0};
    Buffer result = {0};
    char *generated;
    size_t index;

    if (program == NULL || program->kind != PEBBLE_C_NODE_PROGRAM) {
        return NULL;
    }

    generator.program = program;
    generator.output = &body;

    collect_imports(&generator, program);
    collect_package_usage(&generator, program);

    for (index = 0U; index < program->child_count; ++index) {
        const PebbleCNode *function = program->children[index];

        if (function->kind == PEBBLE_C_NODE_IMPORT_DECLARATION) {
            continue;
        }

        if (function->kind != PEBBLE_C_NODE_FUNCTION) {
            body.failed = 1;
            break;
        }
        emit_function_signature(&generator, function, 1);
    }

    append(&body, "\n");

    for (index = 0U; index < program->child_count; ++index) {
        const PebbleCNode *function = program->children[index];
        const PebbleCNode *body_block = find_child(function, PEBBLE_C_NODE_BLOCK);
        size_t parameter_index;

        if (function->kind != PEBBLE_C_NODE_FUNCTION) {
            continue;
        }

        if (body_block == NULL) {
            body.failed = 1;
            break;
        }

        generator.symbol_count = 0U;
        generator.next_symbol_id = 0U;
        generator.depth = 0U;
        generator.in_main = equal(function->text, "main");
        generator.current_package[0] = '\0';

        if (package_of(function->text, generator.current_package,
                       sizeof(generator.current_package))) {
        }

        for (parameter_index = 0U;
             parameter_index < function->child_count;
             ++parameter_index) {
            const PebbleCNode *parameter = function->children[parameter_index];

            if (parameter->kind == PEBBLE_C_NODE_PARAMETER &&
                !add_symbol(&generator, parameter->text, type_name(parameter),
                            0U)) {
                body.failed = 1;
                break;
            }
        }

        emit_function_signature(&generator, function, 0);
        append(&body, " ");
        append(&body, "{\n");
        ++generator.depth;

        if (generator.in_main && uses_time_clock_symbol(&generator)) {
            indent(&generator);
            append(&body, "pebble_time_start();\n");
        }

        emit_parameter_uses(&generator, function);

        for (parameter_index = 0U;
             parameter_index < body_block->child_count;
             ++parameter_index) {
            emit_statement(&generator, body_block->children[parameter_index]);
        }

        --generator.depth;
        indent(&generator);
        append(&body, "}");
        append(&body, "\n\n");
    }

    if (body.failed) {
        free(body.data);
        free(generator.symbols);
        free(generator.imports);
        free(generator.package_functions);
        return NULL;
    }

    if (uses_any_time_symbol(&generator) ||
        uses_any_filesystem_symbol(&generator)) {
        append(&result,
            "#ifndef _POSIX_C_SOURCE\n"
            "#define _POSIX_C_SOURCE 200809L\n"
            "#endif\n"
        );
    }

    emit_print_helpers(&generator, &result);
    append_package_runtime(&generator, &result);

    free(generator.symbols);
    free(generator.imports);
    free(generator.package_functions);

    if (body.length > 0U) {
        append_n(&result, body.data, body.length);
    }

    free(body.data);

    generated = result.data;
    if (generated == NULL) {
        generated = malloc(1U);
        if (generated != NULL) {
            generated[0] = '\0';
        }
    }
    return generated;
}
