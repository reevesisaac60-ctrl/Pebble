#include "pebble_manifest/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;
    char *directory;
} ResolverEntry;

typedef struct {
    PebbleProject *project;
    PebbleManifestDiagnostics *diagnostics;
    ResolverEntry *stack;
    size_t stack_count;
    size_t stack_capacity;
} Resolver;

static void report(PebbleManifestDiagnostics *diagnostics,
                   const char *path,
                   int line,
                   const char *message)
{
    pebble_manifest_report(diagnostics, path, line, 1, message);
}

void pebble_resolved_list_destroy(PebbleResolvedList *packages)
{
    size_t index;

    if (packages == NULL) {
        return;
    }

    for (index = 0U; index < packages->count; ++index) {
        free(packages->items[index].name);
        free(packages->items[index].version);
        free(packages->items[index].path);
        free(packages->items[index].directory);
    }

    free(packages->items);
    *packages = (PebbleResolvedList){0};
}

void pebble_project_destroy(PebbleProject *project)
{
    if (project == NULL) {
        return;
    }

    free(project->root);
    free(project->manifest_path);
    free(project->entry_point);
    free(project->lockfile_path);
    pebble_manifest_destroy(project->manifest);
    pebble_resolved_list_destroy(&project->packages);
    *project = (PebbleProject){0};
}

static char *join_path(const char *directory, const char *name)
{
    size_t length;
    size_t needed;
    char *result;

    if (name == NULL || name[0] == '\0') {
        length = strlen(directory == NULL ? "" : directory);
        result = malloc(length + 1U);

        if (result == NULL) {
            return NULL;
        }

        memcpy(result, directory == NULL ? "" : directory, length + 1U);
        return result;
    }

    if (directory == NULL || directory[0] == '\0') {
        length = strlen(name);
        result = malloc(length + 1U);

        if (result == NULL) {
            return NULL;
        }

        memcpy(result, name, length + 1U);
        return result;
    }

    length = strlen(directory);
    needed = length + strlen(name) + 2U;
    result = malloc(needed);

    if (result == NULL) {
        return NULL;
    }

    if (length > 0U && directory[length - 1U] == '/') {
        (void)snprintf(result, needed, "%s%s", directory, name);
    } else {
        (void)snprintf(result, needed, "%s/%s", directory, name);
    }

    return result;
}

static char *normalise_path(const char *path)
{
    size_t length = strlen(path);
    char *result = malloc(length + 1U);
    char **segments;
    size_t count = 0U;
    size_t capacity = 8U;
    size_t index = 0U;
    size_t position = 0U;
    int absolute = path[0] == '/';

    if (result == NULL) {
        return NULL;
    }

    segments = malloc(capacity * sizeof(*segments));

    if (segments == NULL) {
        free(result);
        return NULL;
    }

    while (index < length) {
        size_t start = index;
        size_t segment_length;

        while (index < length && path[index] != '/') {
            index++;
        }

        segment_length = index - start;

        if (segment_length == 0U ||
            (segment_length == 1U && path[start] == '.')) {
            if (index < length) {
                index++;
            }

            continue;
        }

        if (segment_length == 2U && path[start] == '.' &&
            path[start + 1U] == '.') {
            if (count > 0U) {
                count--;
                free(segments[count]);

                if (index < length) {
                    index++;
                }

                continue;
            }

            if (absolute) {
                if (index < length) {
                    index++;
                }

                continue;
            }
        }

        if (count == capacity) {
            char **grown;

            capacity *= 2U;
            grown = realloc(segments, capacity * sizeof(*grown));

            if (grown == NULL) {
                for (index = 0U; index < count; ++index) {
                    free(segments[index]);
                }

                free(segments);
                free(result);
                return NULL;
            }

            segments = grown;
        }

        {
            char *segment = malloc(segment_length + 1U);

            if (segment == NULL) {
                for (index = 0U; index < count; ++index) {
                    free(segments[index]);
                }

                free(segments);
                free(result);
                return NULL;
            }

            memcpy(segment, path + start, segment_length);
            segment[segment_length] = '\0';
            segments[count++] = segment;
        }

        if (index < length) {
            index++;
        }
    }

    if (absolute && position == 0U) {
        result[position++] = '/';
    }

    for (index = 0U; index < count; ++index) {
        const size_t segment_length = strlen(segments[index]);

        if (position > 0U && result[position - 1U] != '/') {
            result[position++] = '/';
        }

        memcpy(result + position, segments[index], segment_length);
        position += segment_length;
        free(segments[index]);
    }

    free(segments);

    if (position == 0U && length > 0U) {
        result[position++] = '.';
    }

    result[position] = '\0';
    return result;
}

static char *resolve_dependency_path(const char *base, const char *path)
{
    char *joined;
    char *normalised;

    if (path[0] == '/') {
        return normalise_path(path);
    }

    joined = join_path(base, path);

    if (joined == NULL) {
        return NULL;
    }

    normalised = normalise_path(joined);
    free(joined);
    return normalised;
}

int pebble_project_load(const char *root,
                        PebbleProject *project,
                        PebbleManifestDiagnostics *diagnostics)
{
    char *manifest_path;
    PebbleManifest *manifest;

    if (project == NULL) {
        return 0;
    }

    memset(project, 0, sizeof(*project));

    project->root = join_path(root == NULL ? "" : root, "");

    if (project->root == NULL) {
        report(diagnostics, root, 0, "Out of memory while loading the "
                                     "project.");
        return 0;
    }

    manifest_path = join_path(project->root, PEBBLE_MANIFEST_NAME);

    if (manifest_path == NULL) {
        report(diagnostics, root, 0, "Out of memory while loading the "
                                     "project.");
        return 0;
    }

    project->manifest_path = manifest_path;
    project->lockfile_path = join_path(project->root, PEBBLE_LOCKFILE_NAME);

    manifest = pebble_manifest_load(manifest_path, diagnostics);

    if (manifest == NULL) {
        return 0;
    }

    project->manifest = manifest;

    {
        char *candidate = join_path(project->root, "src/main.peb");

        if (candidate != NULL) {
            FILE *file = fopen(candidate, "rb");

            if (file != NULL) {
                (void)fclose(file);
                project->entry_point = candidate;
            } else {
                free(candidate);
                candidate = join_path(project->root, "main.peb");

                if (candidate != NULL) {
                    file = fopen(candidate, "rb");

                    if (file != NULL) {
                        (void)fclose(file);
                        project->entry_point = candidate;
                    } else {
                        free(candidate);
                    }
                }
            }
        }
    }

    return 1;
}

static PebbleResolvedPackage *find_resolved(PebbleResolvedList *packages,
                                             const char *name)
{
    size_t index;

    for (index = 0U; index < packages->count; ++index) {
        if (strcmp(packages->items[index].name, name) == 0) {
            return &packages->items[index];
        }
    }

    return NULL;
}

static int add_resolved(PebbleResolvedList *packages,
                        const char *name,
                        const char *version,
                        const char *path,
                        const char *directory)
{
    PebbleResolvedPackage *grown;
    size_t capacity;

    if (packages->count == packages->capacity) {
        capacity = packages->capacity == 0U ? 8U : packages->capacity * 2U;
        grown = realloc(packages->items, capacity * sizeof(*grown));

        if (grown == NULL) {
            return 0;
        }

        packages->items = grown;
        packages->capacity = capacity;
    }

    packages->items[packages->count].name = NULL;
    packages->items[packages->count].version = NULL;
    packages->items[packages->count].path = NULL;
    packages->items[packages->count].directory = NULL;
    packages->count++;

    {
        PebbleResolvedPackage *package = &packages->items[packages->count - 1U];
        const size_t name_length = strlen(name);
        const size_t version_length = strlen(version);
        const size_t path_length = strlen(path);
        const size_t directory_length = strlen(directory);

        package->name = malloc(name_length + 1U);
        package->version = malloc(version_length + 1U);
        package->path = malloc(path_length + 1U);
        package->directory = malloc(directory_length + 1U);

        if (package->name == NULL || package->version == NULL ||
            package->path == NULL || package->directory == NULL) {
            return 0;
        }

        (void)snprintf(package->name, name_length + 1U, "%s", name);
        (void)snprintf(package->version, version_length + 1U, "%s", version);
        (void)snprintf(package->path, path_length + 1U, "%s", path);
        (void)snprintf(package->directory, directory_length + 1U, "%s",
                       directory);
    }

    return 1;
}

static int push_stack(Resolver *resolver,
                      const char *name,
                      const char *directory)
{
    ResolverEntry *grown;
    size_t capacity;
    char *name_copy;
    char *directory_copy;

    if (resolver->stack_count == resolver->stack_capacity) {
        capacity = resolver->stack_capacity == 0U ? 8U
            : resolver->stack_capacity * 2U;
        grown = realloc(resolver->stack, capacity * sizeof(*grown));

        if (grown == NULL) {
            return 0;
        }

        resolver->stack = grown;
        resolver->stack_capacity = capacity;
    }

    name_copy = malloc(strlen(name) + 1U);
    directory_copy = malloc(strlen(directory) + 1U);

    if (name_copy == NULL || directory_copy == NULL) {
        free(name_copy);
        free(directory_copy);
        return 0;
    }

    (void)strcpy(name_copy, name);
    (void)strcpy(directory_copy, directory);
    resolver->stack[resolver->stack_count].name = name_copy;
    resolver->stack[resolver->stack_count].directory = directory_copy;
    resolver->stack_count++;
    return 1;
}

static void pop_stack(Resolver *resolver)
{
    if (resolver->stack_count == 0U) {
        return;
    }

    resolver->stack_count--;
    free(resolver->stack[resolver->stack_count].name);
    free(resolver->stack[resolver->stack_count].directory);
}

static int resolve_manifest(Resolver *resolver,
                            const PebbleManifest *manifest,
                            const char *directory,
                            int depth);

static int report_cycle(Resolver *resolver, const char *name)
{
    char message[512];
    size_t position = 0U;
    size_t index;
    int first = 1;
    int written;

    written = snprintf(message, sizeof(message),
                       "Circular dependency detected: ");

    if (written < 0 || (size_t)written >= sizeof(message)) {
        report(resolver->diagnostics, resolver->project->manifest_path, 0,
               "Circular dependency detected.");
        return 0;
    }

    position = (size_t)written;

    for (index = 0U; index < resolver->stack_count; ++index) {
        const int written_chars = snprintf(
            message + position, sizeof(message) - position, "%s%s",
            first ? "" : " -> ", resolver->stack[index].name);

        if (written_chars < 0 ||
            (size_t)written_chars >= sizeof(message) - position) {
            break;
        }

        position += (size_t)written_chars;
        first = 0;
    }

    (void)snprintf(message + position, sizeof(message) - position, " -> %s.",
                   name);

    report(resolver->diagnostics, resolver->project->manifest_path, 0, message);
    return 0;
}

static int resolve_dependency(Resolver *resolver,
                              const PebbleManifestDependency *dependency,
                              const char *directory,
                              int depth)
{
    char *path;
    char *manifest_path;
    PebbleManifest *child;
    size_t index;

    if (dependency->path[0] == '\0') {
        char message[320];

        (void)snprintf(message, sizeof(message),
                       "The dependency '%s' needs a path.", dependency->name);
        report(resolver->diagnostics, resolver->project->manifest_path, 0,
               message);
        return 0;
    }

    for (index = 0U; index < resolver->stack_count; ++index) {
        if (strcmp(resolver->stack[index].name, dependency->name) == 0) {
            return report_cycle(resolver, dependency->name);
        }
    }

    path = resolve_dependency_path(directory, dependency->path);

    if (path == NULL) {
        report(resolver->diagnostics, resolver->project->manifest_path, 0,
               "Out of memory while resolving the dependencies.");
        return 0;
    }

    for (index = 0U; index < resolver->stack_count; ++index) {
        if (strcmp(resolver->stack[index].directory, path) == 0) {
            free(path);
            return report_cycle(resolver, dependency->name);
        }
    }

    manifest_path = join_path(path, PEBBLE_MANIFEST_NAME);

    if (manifest_path == NULL) {
        free(path);
        report(resolver->diagnostics, resolver->project->manifest_path, 0,
               "Out of memory while resolving the dependencies.");
        return 0;
    }

    child = pebble_manifest_load(manifest_path, resolver->diagnostics);

    if (child == NULL) {
        char message[640];

        (void)snprintf(message, sizeof(message),
                       "The dependency '%s' cannot be resolved from '%s'.",
                       dependency->name, path);
        report(resolver->diagnostics, resolver->project->manifest_path, 0,
               message);
        free(manifest_path);
        free(path);
        return 0;
    }

    free(manifest_path);

    if (dependency->constraint[0] != '\0' &&
        !pebble_version_satisfies(child->version, dependency->constraint)) {
        char message[640];

        (void)snprintf(message, sizeof(message),
                       "The dependency '%s' requires version '%s' but "
                       "%s is installed.",
                       dependency->name, dependency->constraint,
                       child->version);
        report(resolver->diagnostics, resolver->project->manifest_path, 0,
               message);
        pebble_manifest_destroy(child);
        free(path);
        return 0;
    }

    {
        PebbleResolvedPackage *existing =
            find_resolved(&resolver->project->packages, dependency->name);

        if (existing == NULL) {
            if (!add_resolved(&resolver->project->packages, dependency->name,
                               child->version, dependency->path, path)) {
                report(resolver->diagnostics,
                       resolver->project->manifest_path, 0,
                       "Out of memory while resolving the dependencies.");
                pebble_manifest_destroy(child);
                free(path);
                return 0;
            }
        } else if (strcmp(existing->directory, path) != 0) {
            char message[640];

            (void)snprintf(message, sizeof(message),
                           "The dependency '%s' is required from two different "
                           "locations.",
                           dependency->name);
            report(resolver->diagnostics, resolver->project->manifest_path, 0,
                   message);
            pebble_manifest_destroy(child);
            free(path);
            return 0;
        }
    }

    if (!push_stack(resolver, dependency->name, path)) {
        report(resolver->diagnostics, resolver->project->manifest_path, 0,
               "Out of memory while resolving the dependencies.");
        pebble_manifest_destroy(child);
        free(path);
        return 0;
    }

    {
        const int ok = resolve_manifest(resolver, child, path, depth + 1);

        pop_stack(resolver);
        pebble_manifest_destroy(child);
        free(path);

        return ok;
    }
}

static int resolve_manifest(Resolver *resolver,
                            const PebbleManifest *manifest,
                            const char *directory,
                            int depth)
{
    size_t index;

    if (depth > 64) {
        report(resolver->diagnostics, resolver->project->manifest_path, 0,
               "The dependency graph is too deep.");
        return 0;
    }

    for (index = 0U; index < manifest->dependency_count; ++index) {
        if (!resolve_dependency(resolver, &manifest->dependencies[index],
                                directory, depth)) {
            return 0;
        }
    }

    return 1;
}

int pebble_project_resolve(PebbleProject *project,
                           PebbleManifestDiagnostics *diagnostics)
{
    Resolver resolver;
    int ok;

    if (project == NULL) {
        return 0;
    }

    resolver.project = project;
    resolver.diagnostics = diagnostics;
    resolver.stack = NULL;
    resolver.stack_count = 0U;
    resolver.stack_capacity = 0U;

    if (project->manifest == NULL) {
        return 0;
    }

    if (!push_stack(&resolver, project->manifest->name, project->root)) {
        report(diagnostics, project->manifest_path, 0,
               "Out of memory while resolving the dependencies.");
        return 0;
    }

    ok = resolve_manifest(&resolver, project->manifest, project->root, 0);
    pop_stack(&resolver);
    free(resolver.stack);
    return ok;
}

static void write_lockfile_value(FILE *file, const char *value)
{
    const char *cursor = value;

    (void)fputc('"', file);

    while (*cursor != '\0') {
        if (*cursor == '"' || *cursor == '\\') {
            (void)fputc('\\', file);
        }

        (void)fputc(*cursor, file);
        cursor++;
    }

    (void)fputc('"', file);
}

static void unescape_lockfile_value(char *value)
{
    char *source = value;
    char *target = value;

    while (*source != '\0') {
        if (*source == '\\' && source[1] != '\0') {
            source++;
        }

        *target = *source;
        target++;
        source++;
    }

    *target = '\0';
}

int pebble_project_write_lockfile(const PebbleProject *project)
{
    FILE *file;
    size_t index;

    if (project == NULL || project->lockfile_path == NULL) {
        return 0;
    }

    file = fopen(project->lockfile_path, "wb");

    if (file == NULL) {
        return 0;
    }

    (void)fprintf(file,
                  "# Pebble lockfile. Generated by \"pebble install\", "
                  "do not edit it by hand.\n");
    (void)fprintf(file, "lockfile_version = 1\n");

    for (index = 0U; index < project->packages.count; ++index) {
        const PebbleResolvedPackage *package = &project->packages.items[index];

        (void)fprintf(file, "\n[[package]]\n");
        (void)fprintf(file, "name = ");
        write_lockfile_value(file, package->name);
        (void)fprintf(file, "\nversion = ");
        write_lockfile_value(file, package->version);
        (void)fprintf(file, "\npath = ");
        write_lockfile_value(file, package->path);
        (void)fprintf(file, "\n");
    }

    (void)fclose(file);
    return 1;
}

static int add_lockfile_package(PebbleResolvedList *packages,
                                 PebbleManifestDiagnostics *diagnostics,
                                 const char *path,
                                 const char *name,
                                 const char *version,
                                 const char *package_path)
{
    char message[320];

    if (name == NULL || name[0] == '\0') {
        report(diagnostics, path, 0,
               "A package in the lockfile needs a name.");
        return 0;
    }

    if (version == NULL || version[0] == '\0') {
        (void)snprintf(message, sizeof(message),
                       "The package '%s' in the lockfile needs a version.", name);
        report(diagnostics, path, 0, message);
        return 0;
    }

    if (package_path == NULL || package_path[0] == '\0') {
        (void)snprintf(message, sizeof(message),
                       "The package '%s' in the lockfile needs a path.", name);
        report(diagnostics, path, 0, message);
        return 0;
    }

    return add_resolved(packages, name, version, package_path, package_path);
}

static int set_lockfile_value(char **slot, const char *value)
{
    char *copy = malloc(strlen(value) + 1U);

    if (copy == NULL) {
        return 0;
    }

    (void)strcpy(copy, value);
    free(*slot);
    *slot = copy;
    return 1;
}

int pebble_project_read_lockfile(const char *path,
                                 PebbleResolvedList *packages,
                                 PebbleManifestDiagnostics *diagnostics)
{
    FILE *file;
    char line[1024];
    int in_package = 0;
    int supported = 1;
    char *name = NULL;
    char *version = NULL;
    char *package_path = NULL;
    int result = 1;

    if (packages == NULL || path == NULL) {
        return 0;
    }

    memset(packages, 0, sizeof(*packages));

    file = fopen(path, "rb");

    if (file == NULL) {
        return 1;
    }

    while (fgets(line, (int)sizeof(line), file) != NULL) {
        char *content = line;
        char *equals;
        char *key;
        char *value;
        size_t length;

        length = strlen(content);

        if (length > 0U && content[length - 1U] != '\n' &&
            !feof(file)) {
            report(diagnostics, path, 0,
                   "The lockfile has a line that is too long.");
            result = 0;
            break;
        }

        while (length > 0U && (content[length - 1U] == '\n' ||
                               content[length - 1U] == '\r')) {
            content[--length] = '\0';
        }

        if (content[0] == '#' || content[0] == '\0') {
            continue;
        }

        if (strcmp(content, "[[package]]") == 0) {
            if (in_package) {
                if (!add_lockfile_package(packages, diagnostics, path, name,
                                          version, package_path)) {
                    result = 0;
                    break;
                }
            }

            in_package = 1;
            free(name);
            free(version);
            free(package_path);
            name = NULL;
            version = NULL;
            package_path = NULL;
            continue;
        }

        equals = strchr(content, '=');

        if (equals == NULL) {
            continue;
        }

        *equals = '\0';
        key = content;
        value = equals + 1;

        length = strlen(key);

        while (length > 0U && (key[length - 1U] == ' ' ||
                               key[length - 1U] == '\t')) {
            key[--length] = '\0';
        }

        length = strlen(value);

        while (length > 0U && (value[0] == ' ' || value[0] == '\t')) {
            value++;
            length--;
        }

        while (length > 0U && (value[length - 1U] == ' ' ||
                               value[length - 1U] == '\t')) {
            value[--length] = '\0';
        }

        if (length >= 2U && value[0] == '"' && value[length - 1U] == '"') {
            value[length - 1U] = '\0';
            value++;
            unescape_lockfile_value(value);
        }

        if (strcmp(key, "name") == 0) {
            if (!set_lockfile_value(&name, value)) {
                result = 0;
                break;
            }
        } else if (strcmp(key, "version") == 0) {
            if (!set_lockfile_value(&version, value)) {
                result = 0;
                break;
            }
        } else if (strcmp(key, "path") == 0) {
            if (!set_lockfile_value(&package_path, value)) {
                result = 0;
                break;
            }
        } else if (strcmp(key, "lockfile_version") == 0) {
            if (strcmp(value, "1") != 0) {
                char message[320];

                (void)snprintf(message, sizeof(message),
                               "The lockfile version '%s' is not supported.",
                               value);
                report(diagnostics, path, 0, message);
                supported = 0;
            }
        }
    }

    if (result && in_package) {
        if (!add_lockfile_package(packages, diagnostics, path, name, version,
                                  package_path)) {
            result = 0;
        }
    }

    free(name);
    free(version);
    free(package_path);
    (void)fclose(file);
    return result && supported;
}
