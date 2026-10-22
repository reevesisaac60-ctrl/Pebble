#include "pebble_c/module.h"
#include "pebble_c/lexer.h"
#include "pebble_c/package.h"
#include "pebble_c/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    PebbleCModuleResult *result;
    PebbleCNode *program;
    char **resolved;
    size_t resolved_count;
    size_t resolved_capacity;
    char **active;
    size_t active_count;
    size_t active_capacity;
    PebbleCNode **pending_imports;
    size_t pending_count;
    size_t pending_capacity;
    const char *const *search_paths;
    size_t search_path_count;
} Resolver;

static int report(PebbleCModuleResult *result,
                  PebbleCSourceLocation location,
                  const char *message)
{
    return pebble_c_add_diagnostic(
        &result->diagnostics, location, message
    );
}

static int report_missing(PebbleCModuleResult *result,
                          PebbleCSourceLocation location,
                          const char *name,
                          char (*candidates)[512],
                          size_t candidate_count)
{
    char message[2048];
    size_t position = 0U;
    size_t index;

    position = (size_t)snprintf(message, sizeof(message),
                                "Cannot find package '%s'. Looked for ", name);

    for (index = 0U; index < candidate_count && position < sizeof(message);
         ++index) {
        const int written = snprintf(
            message + position, sizeof(message) - position, "%s'%s'",
            index > 0U ? (index + 1U == candidate_count ? " and " : ", ") : "",
            candidates[index]);

        if (written < 0) {
            break;
        }

        position += (size_t)written;
    }

    if (position < sizeof(message)) {
        (void)snprintf(message + position, sizeof(message) - position, ".");
    }

    return report(result, location, message);
}

static int join_path(char *buffer,
                     size_t size,
                     const char *directory,
                     const char *name)
{
    int written;
    size_t length;

    if (directory == NULL || directory[0] == '\0') {
        written = snprintf(buffer, size, "%s", name);
    } else {
        length = strlen(directory);

        if (length > 0U && directory[length - 1U] == '/') {
            written = snprintf(buffer, size, "%s%s", directory, name);
        } else {
            written = snprintf(buffer, size, "%s/%s", directory, name);
        }
    }

    return written > 0 && (size_t)written < size;
}

static char *read_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *contents;

    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0 ||
        (size = ftell(file)) < 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }

    contents = malloc((size_t)size + 1U);

    if (contents == NULL) {
        (void)fclose(file);
        return NULL;
    }

    if (fread(contents, 1U, (size_t)size, file) != (size_t)size) {
        free(contents);
        (void)fclose(file);
        return NULL;
    }

    contents[size] = '\0';
    (void)fclose(file);
    return contents;
}

static void directory_of(const char *path, char *buffer, size_t size)
{
    const char *separator = strrchr(path, '/');
    size_t length;

    if (separator == NULL) {
        buffer[0] = '\0';
        return;
    }

    length = (size_t)(separator - path);

    if (length >= size) {
        buffer[0] = '\0';
        return;
    }

    memcpy(buffer, path, length);
    buffer[length] = '\0';
}

static PebbleCNode *clone_node(const PebbleCNode *node)
{
    PebbleCNode *copy = pebble_c_node_create(
        node->kind, node->location, node->text);
    size_t index;

    if (copy == NULL) {
        return NULL;
    }

    copy->exported = node->exported;
    copy->local = node->local;

    for (index = 0U; index < node->child_count; ++index) {
        PebbleCNode *child = clone_node(node->children[index]);

        if (child == NULL || !pebble_c_node_add(copy, child)) {
            pebble_c_node_destroy(child);
            pebble_c_node_destroy(copy);
            return NULL;
        }
    }

    return copy;
}

static int add_function(PebbleCModuleResult *result, PebbleCNode *function)
{
    PebbleCNode **grown;
    size_t capacity;

    if (result->function_count == result->function_capacity) {
        capacity = result->function_capacity == 0U ? 8U
            : result->function_capacity * 2U;
        grown = realloc(result->functions, capacity * sizeof(*grown));

        if (grown == NULL) {
            return 0;
        }

        result->functions = grown;
        result->function_capacity = capacity;
    }

    result->functions[result->function_count++] = function;
    return 1;
}

static int is_resolved(Resolver *resolver, const char *name)
{
    size_t index;

    for (index = 0U; index < resolver->resolved_count; ++index) {
        if (strcmp(resolver->resolved[index], name) == 0) {
            return 1;
        }
    }

    return 0;
}

static int is_active(Resolver *resolver, const char *name)
{
    size_t index;

    for (index = 0U; index < resolver->active_count; ++index) {
        if (strcmp(resolver->active[index], name) == 0) {
            return 1;
        }
    }

    return 0;
}

static int add_active(Resolver *resolver, const char *name)
{
    char **grown;
    size_t capacity;
    char *copy;

    if (resolver->active_count == resolver->active_capacity) {
        capacity = resolver->active_capacity == 0U ? 8U
            : resolver->active_capacity * 2U;
        grown = realloc(resolver->active, capacity * sizeof(*grown));

        if (grown == NULL) {
            return 0;
        }

        resolver->active = grown;
        resolver->active_capacity = capacity;
    }

    copy = malloc(strlen(name) + 1U);

    if (copy == NULL) {
        return 0;
    }

    (void)strcpy(copy, name);
    resolver->active[resolver->active_count++] = copy;
    return 1;
}

static void remove_active(Resolver *resolver, const char *name)
{
    size_t index;

    for (index = 0U; index < resolver->active_count; ++index) {
        if (strcmp(resolver->active[index], name) == 0) {
            free(resolver->active[index]);
            resolver->active[index] = resolver->active[resolver->active_count - 1U];
            resolver->active_count--;
            return;
        }
    }
}

static void cycle_description(Resolver *resolver,
                              const char *name,
                              char *buffer,
                              size_t size)
{
    size_t index;
    size_t position = 0U;
    int started = 0;

    buffer[0] = '\0';

    for (index = 0U; index < resolver->active_count; ++index) {
        const char *entry = resolver->active[index];

        if (strcmp(entry, name) == 0) {
            started = 1;
        }

        if (!started) {
            continue;
        }

        {
            const int written = snprintf(buffer + position, size - position,
                                         "%s%s", position > 0U ? " -> " : "",
                                         entry);

            if (written < 0 || (size_t)written >= size - position) {
                return;
            }

            position += (size_t)written;
        }
    }

    (void)snprintf(buffer + position, size - position, "%s -> %s",
                   position > 0U ? "" : "", name);
}

static int add_resolved(Resolver *resolver, const char *name)
{
    char **grown;
    size_t capacity;
    char *copy;

    if (resolver->resolved_count == resolver->resolved_capacity) {
        capacity = resolver->resolved_capacity == 0U ? 8U
            : resolver->resolved_capacity * 2U;
        grown = realloc(resolver->resolved, capacity * sizeof(*grown));

        if (grown == NULL) {
            return 0;
        }

        resolver->resolved = grown;
        resolver->resolved_capacity = capacity;
    }

    copy = malloc(strlen(name) + 1U);

    if (copy == NULL) {
        return 0;
    }

    (void)strcpy(copy, name);
    resolver->resolved[resolver->resolved_count++] = copy;
    return 1;
}

static int program_imports(Resolver *resolver, const char *name)
{
    size_t index;

    for (index = 0U; index < resolver->program->child_count; ++index) {
        const PebbleCNode *child = resolver->program->children[index];

        if (child->kind == PEBBLE_C_NODE_IMPORT_DECLARATION &&
            strcmp(child->text, name) == 0) {
            return 1;
        }
    }

    return 0;
}

static int add_pending_import(Resolver *resolver, PebbleCNode *declaration)
{
    PebbleCNode **grown;
    size_t capacity;

    if (resolver->pending_count == resolver->pending_capacity) {
        capacity = resolver->pending_capacity == 0U ? 8U
            : resolver->pending_capacity * 2U;
        grown = realloc(resolver->pending_imports, capacity * sizeof(*grown));

        if (grown == NULL) {
            return 0;
        }

        resolver->pending_imports = grown;
        resolver->pending_capacity = capacity;
    }

    resolver->pending_imports[resolver->pending_count++] = declaration;
    return 1;
}

static void propagate_imports(Resolver *resolver, const PebbleCNode *root)
{
    size_t index;

    for (index = 0U; index < root->child_count; ++index) {
        const PebbleCNode *child = root->children[index];
        PebbleCNode *declaration;

        if (child->kind != PEBBLE_C_NODE_IMPORT_DECLARATION ||
            child->local == 0) {
            continue;
        }

        if (program_imports(resolver, child->text)) {
            continue;
        }

        declaration = pebble_c_node_create(
            PEBBLE_C_NODE_IMPORT_DECLARATION, child->location, child->text);

        if (declaration == NULL) {
            continue;
        }

        declaration->local = 1;

        if (!add_pending_import(resolver, declaration)) {
            pebble_c_node_destroy(declaration);
        }
    }
}

static int add_path(PebbleCModuleResult *result, const char *path)
{
    char **grown;
    size_t capacity;
    char *copy;

    if (result->path_count == result->path_capacity) {
        capacity = result->path_capacity == 0U ? 8U : result->path_capacity * 2U;
        grown = realloc(result->paths, capacity * sizeof(*grown));

        if (grown == NULL) {
            return 0;
        }

        result->paths = grown;
        result->path_capacity = capacity;
    }

    copy = malloc(strlen(path) + 1U);

    if (copy == NULL) {
        return 0;
    }

    (void)strcpy(copy, path);
    result->paths[result->path_count++] = copy;
    return 1;
}

static int resolve_package(Resolver *resolver,
                           PebbleCNode *declaration,
                           const char *directory);

static int resolve_imports(Resolver *resolver,
                           const PebbleCNode *program,
                           const char *directory)
{
    size_t index;

    for (index = 0U; index < program->child_count; ++index) {
        const PebbleCNode *child = program->children[index];

        if (child->kind != PEBBLE_C_NODE_IMPORT_DECLARATION) {
            continue;
        }

        if (pebble_c_find_builtin_package(child->text) != NULL) {
            continue;
        }

        if (!resolve_package(resolver, program->children[index], directory)) {
            continue;
        }
    }

    return 1;
}

static int resolve_package(Resolver *resolver,
                           PebbleCNode *declaration,
                           const char *directory)
{
    const char *name = declaration->text;
    char candidates[16][512];
    size_t candidate_count = 0U;
    char path[512];
    char contents_directory[512];
    char *contents = NULL;
    size_t index;
    int found = 0;

    if (is_active(resolver, name)) {
        char cycle[512];
        char message[640];

        cycle_description(resolver, name, cycle, sizeof(cycle));
        (void)snprintf(message, sizeof(message),
                       "Circular dependency detected: %s.", cycle);
        (void)report(resolver->result, declaration->location, message);
        return 0;
    }

    if (is_resolved(resolver, name)) {
        return 1;
    }

    {
        char file_name[256];

        if (snprintf(file_name, sizeof(file_name), "%s.peb", name) < 0) {
            return 0;
        }

        if (!join_path(candidates[0], sizeof(candidates[0]), directory,
                       file_name)) {
            return 0;
        }

        candidate_count = 1U;

        if (!join_path(contents_directory, sizeof(contents_directory),
                       directory, "packages")) {
            return 0;
        }

        if (!join_path(candidates[1], sizeof(candidates[1]),
                       contents_directory, file_name)) {
            return 0;
        }

        candidate_count = 2U;

        for (index = 0U; index < resolver->search_path_count &&
                         candidate_count < 16U; ++index) {
            if (!join_path(candidates[candidate_count],
                           sizeof(candidates[candidate_count]),
                           resolver->search_paths[index], file_name)) {
                return 0;
            }

            candidate_count++;
        }
    }

    for (index = 0U; index < candidate_count; ++index) {
        contents = read_file(candidates[index]);

        if (contents != NULL) {
            size_t length = strlen(candidates[index]);

            if (length < sizeof(path)) {
                memcpy(path, candidates[index], length + 1U);
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        (void)report_missing(resolver->result, declaration->location, name,
                             candidates, candidate_count);
        return 0;
    }

    if (!add_path(resolver->result, path)) {
        free(contents);
        return 0;
    }

    {
        const char *owned_path =
            resolver->result->paths[resolver->result->path_count - 1U];
        PebbleCSource source = {owned_path, contents, strlen(contents)};
        PebbleCTokenList tokens = pebble_c_lex(source);
        PebbleCParseResult parsed = pebble_c_parse(&tokens);
        int ok = pebble_c_parse_result_ok(&parsed) && parsed.root != NULL;

        if (ok) {
            (void)add_resolved(resolver, name);
            declaration->local = 1;
            directory_of(path, contents_directory, sizeof(contents_directory));

            if (!add_active(resolver, name)) {
                ok = 0;
            } else {
                (void)resolve_imports(
                    resolver, parsed.root, contents_directory);
                remove_active(resolver, name);
            }

            propagate_imports(resolver, parsed.root);

            for (index = 0U; index < parsed.root->child_count; ++index) {
                const PebbleCNode *child = parsed.root->children[index];
                PebbleCNode *function;
                char qualified[512];
                size_t child_index;

                if (child->kind != PEBBLE_C_NODE_FUNCTION) {
                    continue;
                }

                if (snprintf(qualified, sizeof(qualified), "%s.%s", name,
                             child->text) >= (int)sizeof(qualified)) {
                    ok = 0;
                    break;
                }

                function = pebble_c_node_create(
                    PEBBLE_C_NODE_FUNCTION, child->location, qualified);

                if (function == NULL) {
                    ok = 0;
                    break;
                }

                function->exported = child->exported;

                for (child_index = 0U; child_index < child->child_count;
                     ++child_index) {
                    PebbleCNode *copy = clone_node(child->children[child_index]);

                    if (copy == NULL ||
                        !pebble_c_node_add(function, copy)) {
                        pebble_c_node_destroy(copy);
                        pebble_c_node_destroy(function);
                        ok = 0;
                        break;
                    }
                }

                if (!ok) {
                    break;
                }

                if (!add_function(resolver->result, function)) {
                    pebble_c_node_destroy(function);
                    ok = 0;
                    break;
                }
            }
        }

        if (!ok) {
            char message[256];

            (void)snprintf(message, sizeof(message),
                           "Cannot parse package '%s'.", name);
            (void)report(resolver->result, declaration->location, message);
        }

        pebble_c_parse_result_destroy(&parsed);
        pebble_c_tokens_destroy(&tokens);
    }

    free(contents);
    return 1;
}

void pebble_c_module_result_destroy(PebbleCModuleResult *result)
{
    size_t index;

    if (result == NULL) {
        return;
    }

    for (index = 0U; index < result->function_count; ++index) {
        pebble_c_node_destroy(result->functions[index]);
    }

    free(result->functions);

    for (index = 0U; index < result->path_count; ++index) {
        free(result->paths[index]);
    }

    free(result->paths);
    pebble_c_diagnostics_destroy(&result->diagnostics);
    *result = (PebbleCModuleResult){0};
}

int pebble_c_module_result_ok(const PebbleCModuleResult *result)
{
    if (result == NULL) {
        return 0;
    }

    return result->diagnostics.count == 0U;
}

PebbleCModuleResult pebble_c_resolve_modules(
    PebbleCNode *program,
    const char *base_directory,
    const char *const *search_paths,
    size_t search_path_count
)
{
    PebbleCModuleResult result = {0};
    Resolver resolver = {
        &result, program, NULL, 0U, 0U, NULL, 0U, 0U, NULL, 0U, 0U,
        search_paths, search_path_count
    };
    size_t index;

    if (program == NULL) {
        return result;
    }

    (void)resolve_imports(&resolver, program, base_directory);

    for (index = 0U; index < resolver.pending_count; ++index) {
        if (!pebble_c_node_add(program, resolver.pending_imports[index])) {
            pebble_c_node_destroy(resolver.pending_imports[index]);
            resolver.pending_imports[index] = NULL;
        }
    }

    free(resolver.pending_imports);

    for (index = 0U; index < resolver.resolved_count; ++index) {
        free(resolver.resolved[index]);
    }

    free(resolver.resolved);

    for (index = 0U; index < resolver.active_count; ++index) {
        free(resolver.active[index]);
    }

    free(resolver.active);
    return result;
}
