#include "pebble_manifest/manifest.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PEBBLE_MANIFEST_MAX_DEPTH 64

typedef struct {
    PebbleManifestDiagnostics *diagnostics;
    const char *path;
} Reader;

void pebble_manifest_report(PebbleManifestDiagnostics *diagnostics,
                            const char *path,
                            int line,
                            int column,
                            const char *message)
{
    PebbleManifestDiagnostic *items;
    size_t capacity;
    char *copy;
    char *file;

    if (diagnostics == NULL || message == NULL) {
        return;
    }

    if (diagnostics->count == diagnostics->capacity) {
        capacity = diagnostics->capacity == 0U ? 8U : diagnostics->capacity * 2U;
        items = realloc(diagnostics->items, capacity * sizeof(*items));

        if (items == NULL) {
            return;
        }

        diagnostics->items = items;
        diagnostics->capacity = capacity;
    }

    copy = malloc(strlen(message) + 1U);

    if (copy == NULL) {
        return;
    }

    (void)strcpy(copy, message);

    file = NULL;

    if (path != NULL && path[0] != '\0') {
        file = malloc(strlen(path) + 1U);

        if (file == NULL) {
            free(copy);
            return;
        }

        (void)strcpy(file, path);
    }

    diagnostics->items[diagnostics->count].file = file;
    diagnostics->items[diagnostics->count].line = line;
    diagnostics->items[diagnostics->count].column = column;
    diagnostics->items[diagnostics->count].message = copy;
    diagnostics->count++;
}

static void report(PebbleManifestDiagnostics *diagnostics,
                   const char *path,
                   int line,
                   int column,
                   const char *message)
{
    pebble_manifest_report(diagnostics, path, line, column, message);
}

void pebble_manifest_diagnostics_destroy(PebbleManifestDiagnostics *diagnostics)
{
    size_t index;

    if (diagnostics == NULL) {
        return;
    }

    for (index = 0U; index < diagnostics->count; ++index) {
        free(diagnostics->items[index].message);
        free(diagnostics->items[index].file);
    }

    free(diagnostics->items);
    *diagnostics = (PebbleManifestDiagnostics){0};
}

void pebble_manifest_destroy(PebbleManifest *manifest)
{
    size_t index;

    if (manifest == NULL) {
        return;
    }

    free(manifest->name);
    free(manifest->version);
    free(manifest->description);
    free(manifest->license);
    free(manifest->toolchain);

    for (index = 0U; index < manifest->author_count; ++index) {
        free(manifest->authors[index]);
    }

    free(manifest->authors);

    for (index = 0U; index < manifest->dependency_count; ++index) {
        free(manifest->dependencies[index].name);
        free(manifest->dependencies[index].path);
        free(manifest->dependencies[index].constraint);
    }

    free(manifest->dependencies);
    free(manifest);
}

static char *copy_string(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1U);
    return copy;
}

static char *trim(char *text)
{
    size_t length;
    char *start = text;

    while (*start == ' ' || *start == '\t') {
        start++;
    }

    length = strlen(start);

    while (length > 0U) {
        const char last = start[length - 1U];

        if (last != ' ' && last != '\t' && last != '\r') {
            break;
        }

        start[length - 1U] = '\0';
        length--;
    }

    return start;
}

static void strip_comment(char *line)
{
    int in_string = 0;
    size_t index;

    for (index = 0U; line[index] != '\0'; ++index) {
        const char current = line[index];

        if (current == '\\' && in_string) {
            index++;
            continue;
        }

        if (current == '"') {
            in_string = !in_string;
            continue;
        }

        if (current == '#' && !in_string) {
            line[index] = '\0';
            return;
        }
    }
}

static int is_identifier(const char *text)
{
    size_t index;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    for (index = 0U; text[index] != '\0'; ++index) {
        const char current = text[index];

        if (index == 0U) {
            if (!((current >= 'a' && current <= 'z') ||
                  (current >= 'A' && current <= 'Z') || current == '_')) {
                return 0;
            }

            continue;
        }

        if (!((current >= 'a' && current <= 'z') ||
              (current >= 'A' && current <= 'Z') ||
              (current >= '0' && current <= '9') || current == '_' ||
              current == '-')) {
            return 0;
        }
    }

    return 1;
}

static int is_version(const char *text)
{
    size_t index = 0U;
    size_t digits = 0U;

    if (text == NULL) {
        return 0;
    }

    while (text[index] >= '0' && text[index] <= '9') {
        index++;
        digits++;
    }

    if (digits == 0U || text[index] != '.') {
        return 0;
    }

    index++;
    digits = 0U;

    while (text[index] >= '0' && text[index] <= '9') {
        index++;
        digits++;
    }

    if (digits == 0U || text[index] != '.') {
        return 0;
    }

    index++;
    digits = 0U;

    while (text[index] >= '0' && text[index] <= '9') {
        index++;
        digits++;
    }

    if (digits == 0U) {
        return 0;
    }

    if (text[index] == '\0') {
        return 1;
    }

    return text[index] == '-' || text[index] == '+';
}

static int is_version_constraint(const char *text)
{
    const char *version = text;

    if (text == NULL || text[0] == '\0') {
        return 1;
    }

    if (*version == '^' || *version == '=') {
        version++;
    } else if (*version == '>' || *version == '<') {
        version++;

        if (*version == '=') {
            version++;
        }
    }

    return is_version(version);
}

static int read_string(char *value, size_t *length, size_t *consumed)
{
    size_t index = 1U;
    size_t written = 0U;

    while (value[index] != '\0' && value[index] != '"') {
        if (value[index] == '\\' && value[index + 1U] != '\0') {
            index++;
        }

        value[written] = value[index];
        written++;
        index++;
    }

    if (value[index] != '"') {
        return 0;
    }

    value[written] = '\0';
    *length = written;
    *consumed = index + 1U;
    return 1;
}

static int add_author(PebbleManifest *manifest, const char *author)
{
    char **grown;
    size_t capacity;
    char *copy = copy_string(author);

    if (copy == NULL) {
        return 0;
    }

    if (manifest->author_count == 0U) {
        manifest->authors = NULL;
    }

    capacity = manifest->author_count + 1U;
    grown = realloc(manifest->authors, capacity * sizeof(*grown));

    if (grown == NULL) {
        free(copy);
        return 0;
    }

    manifest->authors = grown;
    manifest->authors[manifest->author_count++] = copy;
    return 1;
}

static int add_dependency(PebbleManifest *manifest,
                          const char *name,
                          const char *path,
                          const char *constraint)
{
    PebbleManifestDependency *grown;
    size_t capacity;

    capacity = manifest->dependency_count + 1U;
    grown = realloc(manifest->dependencies, capacity * sizeof(*grown));

    if (grown == NULL) {
        return 0;
    }

    manifest->dependencies = grown;
    manifest->dependencies[manifest->dependency_count].name =
        copy_string(name);
    manifest->dependencies[manifest->dependency_count].path = copy_string(path);
    manifest->dependencies[manifest->dependency_count].constraint =
        copy_string(constraint == NULL ? "" : constraint);
    manifest->dependency_count++;
    return 1;
}

const PebbleManifestDependency *pebble_manifest_find_dependency(
    const PebbleManifest *manifest,
    const char *name
)
{
    size_t index;

    if (manifest == NULL || name == NULL) {
        return NULL;
    }

    for (index = 0U; index < manifest->dependency_count; ++index) {
        if (strcmp(manifest->dependencies[index].name, name) == 0) {
            return &manifest->dependencies[index];
        }
    }

    return NULL;
}

static int parse_inline_table(Reader *reader,
                               PebbleManifest *manifest,
                               const char *section,
                               const char *name,
                               char *value,
                               int line)
{
    char *cursor = value;
    char *key;
    char *entry;
    char *dependency_path = NULL;
    char *constraint = NULL;
    int ok = 0;

    (void)section;

    while (*cursor != '\0') {
        char *entry_start;

        cursor = trim(cursor);

        if (*cursor == '{') {
            cursor++;
            continue;
        }

        if (*cursor == '\0' || *cursor == '}') {
            break;
        }

        if (*cursor == ',') {
            cursor++;
            continue;
        }

        entry_start = cursor;

        while (*cursor != '\0' && *cursor != '=' && *cursor != ',' &&
               *cursor != '}') {
            cursor++;
        }

        if (*cursor != '=') {
            report(reader->diagnostics, reader->path, line, 1,
                   "Expected 'key = value' inside the dependency table.");
            goto cleanup;
        }

        *cursor = '\0';
        key = trim(entry_start);
        cursor++;
        cursor = trim(cursor);

        if (*cursor == '"') {
            size_t length = 0U;
            size_t consumed = 0U;
            char *text;

            if (!read_string(cursor, &length, &consumed)) {
                report(reader->diagnostics, reader->path, line, 1,
                       "Unterminated string in the dependency table.");
                goto cleanup;
            }

            text = copy_string(cursor);

            if (text == NULL) {
                report(reader->diagnostics, reader->path, line, 1,
                       "Out of memory while reading the manifest.");
                goto cleanup;
            }

            cursor += consumed;

            if (strcmp(key, "path") == 0) {
                free(dependency_path);
                dependency_path = text;
            } else if (strcmp(key, "version") == 0) {
                free(constraint);
                constraint = text;
            } else {
                free(text);
                report(reader->diagnostics, reader->path, line, 1,
                       "Unknown key in a dependency table. Expected 'path' or "
                       "'version'.");
                goto cleanup;
            }
        } else {
            entry = cursor;

            while (*cursor != '\0' && *cursor != ',' && *cursor != '}') {
                cursor++;
            }

            *cursor = '\0';

            if (strcmp(key, "path") == 0) {
                free(dependency_path);
                dependency_path = copy_string(trim(entry));

                if (dependency_path == NULL) {
                    report(reader->diagnostics, reader->path, line, 1,
                           "Out of memory while reading the manifest.");
                    goto cleanup;
                }
            } else {
                report(reader->diagnostics, reader->path, line, 1,
                       "Unknown key in a dependency table. Expected 'path' or "
                       "'version'.");
                goto cleanup;
            }
        }
    }

    if (dependency_path == NULL || dependency_path[0] == '\0') {
        report(reader->diagnostics, reader->path, line, 1,
               "A dependency needs a 'path'.");
        goto cleanup;
    }

    if (constraint != NULL && constraint[0] != '\0' &&
        !is_version_constraint(constraint)) {
        report(reader->diagnostics, reader->path, line, 1,
               "The dependency version constraint is not a valid version.");
        goto cleanup;
    }

    if (pebble_manifest_find_dependency(manifest, name) != NULL) {
        char message[320];

        (void)snprintf(message, sizeof(message),
                       "The dependency '%s' is declared more than once.", name);
        report(reader->diagnostics, reader->path, line, 1, message);
        goto cleanup;
    }

    if (!add_dependency(manifest, name, dependency_path, constraint)) {
        report(reader->diagnostics, reader->path, line, 1,
               "Out of memory while reading the manifest.");
        goto cleanup;
    }

    ok = 1;

cleanup:
    free(dependency_path);
    free(constraint);
    return ok;
}

static int parse_author_list(Reader *reader,
                              PebbleManifest *manifest,
                              char *value,
                              int line)
{
    char *cursor = trim(value);
    int bracketed = 0;
    int closed = 0;

    if (*cursor == '[') {
        bracketed = 1;
        cursor++;
    }

    while (*cursor != '\0') {
        char *entry;

        cursor = trim(cursor);

        if (*cursor == '\0') {
            break;
        }

        if (*cursor == ']') {
            cursor++;
            closed = 1;
            break;
        }

        if (*cursor == ',') {
            cursor++;
            continue;
        }

        if (*cursor != '"') {
            report(reader->diagnostics, reader->path, line, 1,
                   "Expected a quoted author name.");
            return 0;
        }

        entry = cursor;

        {
            size_t length = 0U;
            size_t consumed = 0U;

            if (!read_string(entry, &length, &consumed)) {
                report(reader->diagnostics, reader->path, line, 1,
                       "Unterminated author name.");
                return 0;
            }

            cursor = entry + consumed;

            if (!add_author(manifest, entry)) {
                report(reader->diagnostics, reader->path, line, 1,
                       "Out of memory while reading the manifest.");
                return 0;
            }
        }
    }

    if (bracketed) {
        cursor = trim(cursor);

        if (!closed || *cursor != '\0') {
            report(reader->diagnostics, reader->path, line, 1,
                   "Expected ']' to close the author list.");
            return 0;
        }
    }

    return 1;
}

static int assign(Reader *reader,
                  PebbleManifest *manifest,
                  const char *section,
                  const char *key,
                  char *value,
                  int line)
{
    if (strcmp(section, "package") == 0) {
        if (strcmp(key, "name") == 0) {
            manifest->name = copy_string(value);
            return manifest->name != NULL;
        }

        if (strcmp(key, "version") == 0) {
            manifest->version = copy_string(value);
            return manifest->version != NULL;
        }

        if (strcmp(key, "description") == 0) {
            manifest->description = copy_string(value);
            return manifest->description != NULL;
        }

        if (strcmp(key, "license") == 0) {
            manifest->license = copy_string(value);
            return manifest->license != NULL;
        }

        if (strcmp(key, "pebble") == 0) {
            manifest->toolchain = copy_string(value);
            return manifest->toolchain != NULL;
        }

        if (strcmp(key, "authors") == 0) {
            return parse_author_list(reader, manifest, value, line);
        }

        report(reader->diagnostics, reader->path, line, 1,
               "Unknown key in the [package] section.");
        return 0;
    }

    if (strcmp(section, "dependencies") == 0) {
        if (value[0] != '{') {
            report(reader->diagnostics, reader->path, line, 1,
                   "A dependency needs a table such as { path = \"../other\" }.");
            return 0;
        }

        return parse_inline_table(reader, manifest, section, key, value, line);
    }

    report(reader->diagnostics, reader->path, line, 1,
           "Unknown section in the manifest.");
    return 0;
}

PebbleManifest *pebble_manifest_parse(const char *text,
                                       size_t length,
                                       const char *path,
                                       PebbleManifestDiagnostics *diagnostics)
{
    PebbleManifest *manifest;
    Reader reader;
    char *buffer;
    char *line;
    char *cursor;
    int line_number = 0;
    char section[64];
    int ok = 1;

    if (text == NULL) {
        report(diagnostics, path, 0, 0, "The manifest is empty.");
        return NULL;
    }

    manifest = calloc(1U, sizeof(*manifest));

    if (manifest == NULL) {
        report(diagnostics, path, 0, 0, "Out of memory while reading the "
                                         "manifest.");
        return NULL;
    }

    buffer = malloc(length + 1U);

    if (buffer == NULL) {
        report(diagnostics, path, 0, 0, "Out of memory while reading the "
                                         "manifest.");
        pebble_manifest_destroy(manifest);
        return NULL;
    }

    memcpy(buffer, text, length);
    buffer[length] = '\0';

    reader.diagnostics = diagnostics;
    reader.path = path;
    section[0] = '\0';

    cursor = buffer;

    while (cursor != NULL && *cursor != '\0') {
        char *content;
        char *equals;
        char *key;
        char *value;

        line = cursor;
        cursor = strchr(cursor, '\n');

        if (cursor != NULL) {
            *cursor = '\0';
            cursor++;
        }

        line_number++;
        strip_comment(line);
        content = trim(line);

        if (content[0] == '\0') {
            continue;
        }

        if (content[0] == '[') {
            char *close = strchr(content, ']');

            if (close == NULL) {
                report(diagnostics, path, line_number, 1,
                       "Unterminated section header.");
                ok = 0;
                break;
            }

            *close = '\0';
            (void)snprintf(section, sizeof(section), "%s", trim(content + 1));

            if (strcmp(section, "package") != 0 &&
                strcmp(section, "dependencies") != 0) {
                report(diagnostics, path, line_number, 1,
                       "Unknown section in the manifest.");
                ok = 0;
                break;
            }

            continue;
        }

        equals = strchr(content, '=');

        if (equals == NULL) {
            report(diagnostics, path, line_number, 1,
                   "Expected 'key = value'.");
            ok = 0;
            continue;
        }

        *equals = '\0';
        key = trim(content);
        value = trim(equals + 1);

        if (section[0] == '\0') {
            report(diagnostics, path, line_number, 1,
                   "A key needs a section, for example [package].");
            ok = 0;
            continue;
        }

        if (!is_identifier(key)) {
            report(diagnostics, path, line_number, 1,
                   "Invalid key name in the manifest.");
            ok = 0;
            continue;
        }

        if (value[0] == '"') {
            size_t string_length = 0U;
            size_t string_consumed = 0U;

            if (!read_string(value, &string_length, &string_consumed)) {
                report(diagnostics, path, line_number, 1,
                       "Unterminated string in the manifest.");
                ok = 0;
                continue;
            }
        }

        if (!assign(&reader, manifest, section, key, value, line_number)) {
            ok = 0;
        }
    }

    free(buffer);

    if (ok) {
        if (manifest->name == NULL || manifest->name[0] == '\0') {
            report(diagnostics, path, 0, 0,
                   "The manifest needs a package name.");
            ok = 0;
        } else if (!is_identifier(manifest->name)) {
            report(diagnostics, path, 0, 0,
                   "The package name is not a valid identifier.");
            ok = 0;
        }

        if (manifest->version == NULL || manifest->version[0] == '\0') {
            report(diagnostics, path, 0, 0,
                   "The manifest needs a package version.");
            ok = 0;
        } else if (!is_version(manifest->version)) {
            report(diagnostics, path, 0, 0,
                   "The package version is not a valid semantic version.");
            ok = 0;
        }

        if (manifest->toolchain != NULL && manifest->toolchain[0] != '\0') {
            const char *constraint = manifest->toolchain;

            if (*constraint == '>' || *constraint == '<' || *constraint == '^' ||
                *constraint == '=') {
                constraint++;
            }

            if (!is_version(constraint)) {
                report(diagnostics, path, 0, 0,
                       "The Pebble toolchain requirement is not a valid "
                       "version.");
                ok = 0;
            }
        }

        {
            size_t index;

            for (index = 0U; index < manifest->dependency_count; ++index) {
                const PebbleManifestDependency *dependency =
                    &manifest->dependencies[index];

                if (!is_identifier(dependency->name)) {
                    report(diagnostics, path, 0, 0,
                           "A dependency name is not a valid identifier.");
                    ok = 0;
                }

                if (dependency->path[0] == '\0') {
                    report(diagnostics, path, 0, 0,
                           "A dependency needs a path.");
                    ok = 0;
                }
            }
        }
    }

    if (!ok) {
        pebble_manifest_destroy(manifest);
        return NULL;
    }

    return manifest;
}

PebbleManifest *pebble_manifest_load(const char *path,
                                      PebbleManifestDiagnostics *diagnostics)
{
    FILE *file;
    long size;
    char *contents;
    PebbleManifest *manifest;

    if (path == NULL) {
        report(diagnostics, NULL, 0, 0, "The manifest cannot be read.");
        return NULL;
    }

    file = fopen(path, "rb");

    if (file == NULL) {
        report(diagnostics, path, 0, 0, "The manifest cannot be read.");
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0 || (size = ftell(file)) < 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        report(diagnostics, path, 0, 0, "The manifest cannot be read.");
        return NULL;
    }

    contents = malloc((size_t)size + 1U);

    if (contents == NULL) {
        (void)fclose(file);
        report(diagnostics, path, 0, 0, "Out of memory while reading the "
                                         "manifest.");
        return NULL;
    }

    if (fread(contents, 1U, (size_t)size, file) != (size_t)size) {
        free(contents);
        (void)fclose(file);
        report(diagnostics, path, 0, 0, "The manifest cannot be read.");
        return NULL;
    }

    (void)fclose(file);
    manifest = pebble_manifest_parse(contents, (size_t)size, path, diagnostics);
    free(contents);
    return manifest;
}

PebbleManifest *pebble_manifest_create(const char *name, const char *version)
{
    PebbleManifest *manifest = calloc(1U, sizeof(*manifest));

    if (manifest == NULL) {
        return NULL;
    }

    if (name != NULL) {
        manifest->name = copy_string(name);
    }

    if (version != NULL) {
        manifest->version = copy_string(version);
    }

    return manifest;
}

void pebble_manifest_set_description(PebbleManifest *manifest,
                                      const char *description)
{
    if (manifest == NULL) {
        return;
    }

    free(manifest->description);
    manifest->description = copy_string(description);
}

void pebble_manifest_set_license(PebbleManifest *manifest, const char *license)
{
    if (manifest == NULL) {
        return;
    }

    free(manifest->license);
    manifest->license = copy_string(license);
}

void pebble_manifest_set_toolchain(PebbleManifest *manifest,
                                   const char *toolchain)
{
    if (manifest == NULL) {
        return;
    }

    free(manifest->toolchain);
    manifest->toolchain = copy_string(toolchain);
}

int pebble_manifest_add_author(PebbleManifest *manifest, const char *author)
{
    if (manifest == NULL || author == NULL) {
        return 0;
    }

    return add_author(manifest, author);
}

int pebble_manifest_add_dependency(PebbleManifest *manifest,
                                   const char *name,
                                   const char *path,
                                   const char *constraint)
{
    if (manifest == NULL || name == NULL || path == NULL) {
        return 0;
    }

    return add_dependency(manifest, name, path, constraint);
}

int pebble_manifest_remove_dependency(PebbleManifest *manifest, const char *name)
{
    size_t index;

    if (manifest == NULL || name == NULL) {
        return 0;
    }

    for (index = 0U; index < manifest->dependency_count; ++index) {
        if (strcmp(manifest->dependencies[index].name, name) != 0) {
            continue;
        }

        free(manifest->dependencies[index].name);
        free(manifest->dependencies[index].path);
        free(manifest->dependencies[index].constraint);

        for (size_t move = index + 1U; move < manifest->dependency_count;
             ++move) {
            manifest->dependencies[move - 1U] = manifest->dependencies[move];
        }

        manifest->dependency_count--;
        return 1;
    }

    return 0;
}

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Writer;

static int writer_reserve(Writer *writer, size_t extra)
{
    size_t capacity;
    char *grown;

    if (writer->length + extra + 1U <= writer->capacity) {
        return 1;
    }

    capacity = writer->capacity == 0U ? 512U : writer->capacity;

    while (capacity < writer->length + extra + 1U) {
        capacity *= 2U;
    }

    grown = realloc(writer->data, capacity);

    if (grown == NULL) {
        return 0;
    }

    writer->data = grown;
    writer->capacity = capacity;
    return 1;
}

static int writer_append(Writer *writer, const char *text)
{
    size_t length = strlen(text);

    if (!writer_reserve(writer, length)) {
        return 0;
    }

    memcpy(writer->data + writer->length, text, length);
    writer->length += length;
    writer->data[writer->length] = '\0';
    return 1;
}

static int writer_appendf(Writer *writer, const char *format, ...)
{
    va_list arguments;
    va_list copy_arguments;
    int needed;

    va_start(arguments, format);
    va_copy(copy_arguments, arguments);
    needed = vsnprintf(NULL, 0, format, copy_arguments);
    va_end(copy_arguments);

    if (needed < 0) {
        va_end(arguments);
        return 0;
    }

    if (!writer_reserve(writer, (size_t)needed)) {
        va_end(arguments);
        return 0;
    }

    (void)vsnprintf(writer->data + writer->length,
                    writer->capacity - writer->length, format, arguments);
    va_end(arguments);
    writer->length += (size_t)needed;
    return 1;
}

static int writer_append_quoted(Writer *writer, const char *text)
{
    const char *cursor = text;

    if (!writer_append(writer, "\"")) {
        return 0;
    }

    while (*cursor != '\0') {
        if (*cursor == '"' || *cursor == '\\') {
            if (!writer_append(writer, "\\")) {
                return 0;
            }
        }

        if (!writer_appendf(writer, "%c", *cursor)) {
            return 0;
        }

        cursor++;
    }

    return writer_append(writer, "\"");
}

static int writer_append_entry(Writer *writer, const char *key, const char *value)
{
    if (!writer_appendf(writer, "%s = ", key) ||
        !writer_append_quoted(writer, value)) {
        return 0;
    }

    return writer_append(writer, "\n");
}

char *pebble_manifest_write(const PebbleManifest *manifest)
{
    Writer writer = {NULL, 0U, 0U};
    size_t index;

    if (manifest == NULL) {
        return NULL;
    }

    if (!writer_append(&writer, "[package]\n") ||
        !writer_append_entry(&writer, "name",
                             manifest->name == NULL ? "" : manifest->name) ||
        !writer_append_entry(&writer, "version",
                             manifest->version == NULL ? "" : manifest->version)) {
        free(writer.data);
        return NULL;
    }

    if (manifest->description != NULL && manifest->description[0] != '\0') {
        if (!writer_append_entry(&writer, "description",
                                 manifest->description)) {
            free(writer.data);
            return NULL;
        }
    }

    if (manifest->license != NULL && manifest->license[0] != '\0') {
        if (!writer_append_entry(&writer, "license", manifest->license)) {
            free(writer.data);
            return NULL;
        }
    }

    if (manifest->authors != NULL && manifest->author_count > 0U) {
        if (!writer_append(&writer, "authors = [")) {
            free(writer.data);
            return NULL;
        }

        for (index = 0U; index < manifest->author_count; ++index) {
            if (!writer_append(&writer, index > 0U ? ", " : "") ||
                !writer_append_quoted(&writer, manifest->authors[index])) {
                free(writer.data);
                return NULL;
            }
        }

        if (!writer_append(&writer, "]\n")) {
            free(writer.data);
            return NULL;
        }
    }

    if (manifest->toolchain != NULL && manifest->toolchain[0] != '\0') {
        if (!writer_append_entry(&writer, "pebble", manifest->toolchain)) {
            free(writer.data);
            return NULL;
        }
    }

    if (manifest->dependency_count > 0U) {
        if (!writer_append(&writer, "\n[dependencies]\n")) {
            free(writer.data);
            return NULL;
        }

        for (index = 0U; index < manifest->dependency_count; ++index) {
            const PebbleManifestDependency *dependency =
                &manifest->dependencies[index];

            if (!writer_appendf(&writer, "%s = { path = ", dependency->name) ||
                !writer_append_quoted(&writer, dependency->path)) {
                free(writer.data);
                return NULL;
            }

            if (dependency->constraint != NULL &&
                dependency->constraint[0] != '\0') {
                if (!writer_append(&writer, ", version = ") ||
                    !writer_append_quoted(&writer, dependency->constraint)) {
                    free(writer.data);
                    return NULL;
                }
            }

            if (!writer_append(&writer, " }\n")) {
                free(writer.data);
                return NULL;
            }
        }
    }

    return writer.data;
}

static int parse_version_parts(const char *text,
                               int parts[3],
                               const char **pre_release)
{
    size_t index = 0U;
    size_t part = 0U;
    int digits = 0;

    parts[0] = 0;
    parts[1] = 0;
    parts[2] = 0;

    if (pre_release != NULL) {
        *pre_release = NULL;
    }

    while (part < 3U) {
        digits = 0;

        while (text[index] >= '0' && text[index] <= '9') {
            parts[part] = parts[part] * 10 + (text[index] - '0');
            index++;
            digits++;
        }

        if (digits == 0) {
            return 0;
        }

        part++;

        if (part < 3U) {
            if (text[index] != '.') {
                return 0;
            }

            index++;
        }
    }

    if (text[index] == '-') {
        if (pre_release != NULL) {
            *pre_release = text + index + 1U;
        }

        return 1;
    }

    if (text[index] == '+') {
        return 1;
    }

    return text[index] == '\0';
}

static int compare_pre_release(const char *left, const char *right)
{
    if (left == NULL && right == NULL) {
        return 0;
    }

    if (left == NULL) {
        return 1;
    }

    if (right == NULL) {
        return -1;
    }

    return strcmp(left, right) < 0 ? -1 : 1;
}

int pebble_version_compare(const char *left, const char *right)
{
    int left_parts[3];
    int right_parts[3];
    const char *left_pre = NULL;
    const char *right_pre = NULL;
    size_t index;

    if (left == NULL || right == NULL) {
        return 0;
    }

    if (!parse_version_parts(left, left_parts, &left_pre) ||
        !parse_version_parts(right, right_parts, &right_pre)) {
        return strcmp(left, right) < 0 ? -1 : 1;
    }

    for (index = 0U; index < 3U; ++index) {
        if (left_parts[index] != right_parts[index]) {
            return left_parts[index] < right_parts[index] ? -1 : 1;
        }
    }

    return compare_pre_release(left_pre, right_pre);
}

int pebble_version_satisfies(const char *version, const char *constraint)
{
    char operator[3];
    const char *required;
    int comparison;

    if (version == NULL) {
        return 0;
    }

    if (constraint == NULL || constraint[0] == '\0') {
        return 1;
    }

    if (constraint[0] == '^') {
        operator[0] = '^';
        operator[1] = '\0';
        required = constraint + 1;
    } else if (constraint[0] == '>' || constraint[0] == '<' ||
               constraint[0] == '=') {
        operator[0] = constraint[0];
        operator[1] = '\0';
        required = constraint + 1;

        if (constraint[1] == '=') {
            operator[1] = '=';
            required = constraint + 2;
        }
    } else {
        operator[0] = '=';
        operator[1] = '\0';
        required = constraint;
    }

    if (required[0] == '=') {
        required++;
    }

    comparison = pebble_version_compare(version, required);

    switch (operator[0]) {
    case '=':
        return comparison == 0;
    case '>':
        return operator[1] == '=' ? comparison >= 0 : comparison > 0;
    case '<':
        return operator[1] == '=' ? comparison <= 0 : comparison < 0;
    case '^': {
        int parts[3];
        const char *pre_release = NULL;

        if (!parse_version_parts(required, parts, &pre_release)) {
            return 0;
        }

        if (comparison < 0) {
            return 0;
        }

        if (parts[0] > 0) {
            return version[0] == required[0] && comparison >= 0;
        }

        if (parts[1] > 0) {
            return parts[0] == 0 && comparison >= 0;
        }

        return parts[0] == 0 && parts[1] == 0 && comparison >= 0;
    }
    default:
        return 0;
    }
}
