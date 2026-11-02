#include "cli.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#endif

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

static char *join_path(const char *directory, const char *name)
{
    size_t length;
    size_t needed;
    char *result;

    if (name == NULL || name[0] == '\0') {
        return copy_string(directory == NULL ? "" : directory);
    }

    if (directory == NULL || directory[0] == '\0') {
        return copy_string(name);
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

static int write_file(const char *path, const char *contents)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        return 0;
    }

    (void)fputs(contents, file);

    if (fclose(file) != 0) {
        return 0;
    }

    return 1;
}

static int copy_file(const char *source, const char *target)
{
    FILE *input = fopen(source, "rb");
    FILE *output;
    char buffer[4096];
    size_t read_bytes;

    if (input == NULL) {
        return 0;
    }

    output = fopen(target, "wb");

    if (output == NULL) {
        (void)fclose(input);
        return 0;
    }

    while ((read_bytes = fread(buffer, 1U, sizeof(buffer), input)) > 0U) {
        if (fwrite(buffer, 1U, read_bytes, output) != read_bytes) {
            (void)fclose(input);
            (void)fclose(output);
            return 0;
        }
    }

    (void)fclose(input);
    return fclose(output) == 0;
}

static int make_directories(const char *path)
{
    char *copy = copy_string(path);
    size_t index;
    int result = 1;

    if (copy == NULL) {
        return 0;
    }

    for (index = 1U; copy[index] != '\0'; ++index) {
        if (copy[index] != '/') {
            continue;
        }

        copy[index] = '\0';

        if (mkdir(copy, 0755) != 0) {
            struct stat information;

            if (stat(copy, &information) != 0 ||
                !S_ISDIR(information.st_mode)) {
                result = 0;
                break;
            }
        }

        copy[index] = '/';
    }

    if (result) {
        if (mkdir(copy, 0755) != 0) {
            struct stat information;

            if (stat(copy, &information) != 0 ||
                !S_ISDIR(information.st_mode)) {
                result = 0;
            }
        }
    }

    free(copy);
    return result;
}

static int file_exists(const char *path)
{
    struct stat information;

    return stat(path, &information) == 0;
}

static void describe_diagnostics(const PebbleManifestDiagnostics *diagnostics,
                                 char *message,
                                 size_t message_size)
{
    size_t index;
    size_t position = 0U;

    if (message == NULL || message_size == 0U) {
        return;
    }

    message[0] = '\0';

    for (index = 0U; index < diagnostics->count; ++index) {
        const PebbleManifestDiagnostic *diagnostic = &diagnostics->items[index];
        int written;

        if (position > 0U) {
            written = snprintf(message + position, message_size - position,
                               "\n");

            if (written < 0 || (size_t)written >= message_size - position) {
                return;
            }

            position += (size_t)written;
        }

        if (diagnostic->file != NULL && diagnostic->file[0] != '\0') {
            written = snprintf(message + position, message_size - position,
                               "%s: ", diagnostic->file);

            if (written < 0 || (size_t)written >= message_size - position) {
                return;
            }

            position += (size_t)written;
        }

        written = snprintf(message + position, message_size - position, "%s",
                           diagnostic->message);

        if (written < 0 || (size_t)written >= message_size - position) {
            return;
        }

        position += (size_t)written;
    }
}

void pebble_cli_project_destroy(PebbleCliProject *project)
{
    size_t index;

    if (project == NULL) {
        return;
    }

    free(project->root);
    free(project->manifest_path);
    free(project->entry_point);

    for (index = 0U; index < project->search_path_count; ++index) {
        free(project->search_paths[index]);
    }

    free(project->search_paths);
    *project = (PebbleCliProject){0};
}

int pebble_cli_load_project(const char *root,
                            PebbleCliProject *project,
                            char *message,
                            size_t message_size)
{
    PebbleManifestDiagnostics diagnostics = {0};
    PebbleProject loaded = {0};
    size_t index;

    memset(project, 0, sizeof(*project));

    if (!pebble_project_load(root, &loaded, &diagnostics)) {
        describe_diagnostics(&diagnostics, message, message_size);
        pebble_manifest_diagnostics_destroy(&diagnostics);
        pebble_project_destroy(&loaded);
        return 0;
    }

    if (!pebble_project_resolve(&loaded, &diagnostics)) {
        describe_diagnostics(&diagnostics, message, message_size);
        pebble_manifest_diagnostics_destroy(&diagnostics);
        pebble_project_destroy(&loaded);
        return 0;
    }

    pebble_manifest_diagnostics_destroy(&diagnostics);

    project->root = copy_string(loaded.root);
    project->manifest_path = copy_string(loaded.manifest_path);
    project->entry_point = copy_string(loaded.entry_point);

    if (loaded.packages.count > 0U) {
        project->search_paths = malloc(loaded.packages.count *
                                       sizeof(*project->search_paths));

        if (project->search_paths == NULL) {
            pebble_project_destroy(&loaded);
            pebble_cli_project_destroy(project);
            return 0;
        }
    }

    for (index = 0U; index < loaded.packages.count; ++index) {
        project->search_paths[index] =
            copy_string(loaded.packages.items[index].directory);
        project->search_path_count++;
    }

    pebble_project_destroy(&loaded);
    return 1;
}

static int write_manifest(const char *path, const PebbleManifest *manifest)
{
    char *const rendered = pebble_manifest_write(manifest);
    int written;

    if (rendered == NULL) {
        return 0;
    }

    written = write_file(path, rendered);
    free(rendered);
    return written;
}

static char *package_source(void)
{
    char *source = malloc(256U);

    if (source == NULL) {
        return NULL;
    }

    (void)snprintf(source, 256U,
        "export fn greet(who: String) -> String {\n"
        "    return \"Hello, \" + who + \"!\"\n"
        "}\n");
    return source;
}

static char *example_source(const char *name)
{
    const size_t length = strlen(name);
    char *source = malloc(length + 256U);

    if (source == NULL) {
        return NULL;
    }

    (void)snprintf(source, length + 256U,
        "import package %s\n"
        "\n"
        "fn main() {\n"
        "    print(%s.greet(\"Pebble\"))\n"
        "}\n", name, name);
    return source;
}

int pebble_cli_init(const char *root, const char *name)
{
    char *manifest_path = join_path(root, PEBBLE_MANIFEST_NAME);
    char *package_name = NULL;
    char *package_path = NULL;
    char *example_path = NULL;
    char *source = NULL;
    char *example = NULL;
    PebbleManifest *manifest = NULL;

    if (manifest_path == NULL) {
        (void)fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    if (file_exists(manifest_path)) {
        (void)fprintf(stderr, "error: '%s' already exists.\n", manifest_path);
        free(manifest_path);
        return 1;
    }

    if (name != NULL && name[0] != '\0') {
        package_name = copy_string(name);
    } else {
        const char *separator = strrchr(root, '/');
        const char *start = separator == NULL ? root : separator + 1;

        package_name = copy_string(start);
    }

    if (package_name == NULL || package_name[0] == '\0') {
        (void)fprintf(stderr, "error: the package needs a name.\n");
        free(manifest_path);
        free(package_name);
        return 1;
    }

    manifest = pebble_manifest_create(package_name, "0.1.0");

    if (manifest == NULL) {
        (void)fprintf(stderr, "error: out of memory\n");
        free(manifest_path);
        free(package_name);
        return 1;
    }

    if (!write_manifest(manifest_path, manifest)) {
        (void)fprintf(stderr, "error: cannot write '%s'\n", manifest_path);
        pebble_manifest_destroy(manifest);
        free(manifest_path);
        free(package_name);
        return 1;
    }

    package_path = join_path(root, "");
    free(package_path);
    package_path = malloc(strlen(root) + strlen(package_name) + 8U);

    if (package_path != NULL) {
        (void)snprintf(package_path, strlen(root) + strlen(package_name) + 8U,
                       "%s/%s.peb", root, package_name);
    }

    example_path = join_path(root, "main.peb");
    source = package_source();
    example = example_source(package_name);

    if (package_path == NULL || example_path == NULL || source == NULL ||
        example == NULL ||
        !write_file(package_path, source) ||
        !write_file(example_path, example)) {
        (void)fprintf(stderr, "error: cannot write the package sources.\n");
    } else {
        (void)printf("Created package '%s' in %s\n", package_name, root);
        (void)printf("  %s\n", PEBBLE_MANIFEST_NAME);
        (void)printf("  %s.peb\n", package_name);
        (void)printf("  main.peb\n");
    }

    free(source);
    free(example);
    free(package_path);
    free(example_path);
    free(package_name);
    free(manifest_path);
    pebble_manifest_destroy(manifest);
    return 0;
}

int pebble_cli_add(const char *root, const char *path)
{
    char *manifest_path = join_path(root, PEBBLE_MANIFEST_NAME);
    char *dependency_path = join_path(path, PEBBLE_MANIFEST_NAME);
    PebbleManifestDiagnostics diagnostics = {0};
    PebbleManifest *dependency = NULL;
    PebbleManifest *manifest = NULL;
    char *name = NULL;
    char *relative = NULL;
    int result = 1;

    if (manifest_path == NULL || dependency_path == NULL) {
        (void)fprintf(stderr, "error: out of memory\n");
        free(manifest_path);
        free(dependency_path);
        return 1;
    }

    if (!file_exists(manifest_path)) {
        (void)fprintf(stderr,
            "error: '%s' does not exist. Run 'pebble init' first.\n",
            manifest_path);
        goto cleanup;
    }

    if (!file_exists(dependency_path)) {
        (void)fprintf(stderr, "error: '%s' does not exist.\n", dependency_path);
        goto cleanup;
    }

    dependency = pebble_manifest_load(dependency_path, &diagnostics);

    if (dependency == NULL) {
        char message[1024];

        describe_diagnostics(&diagnostics, message, sizeof(message));
        (void)fprintf(stderr, "error: %s\n", message);
        goto cleanup;
    }

    name = copy_string(dependency->name);
    pebble_manifest_destroy(dependency);
    dependency = NULL;

    manifest = pebble_manifest_load(manifest_path, &diagnostics);

    if (manifest == NULL) {
        char message[1024];

        describe_diagnostics(&diagnostics, message, sizeof(message));
        (void)fprintf(stderr, "error: %s\n", message);
        goto cleanup;
    }

    if (pebble_manifest_find_dependency(manifest, name) != NULL) {
        (void)fprintf(stderr,
            "error: the dependency '%s' is already declared.\n", name);
        goto cleanup;
    }

    relative = copy_string(path);

    if (relative != NULL) {
        const size_t root_length = strlen(root);

        if (strncmp(relative, root, root_length) == 0 &&
            relative[root_length] == '/') {
            char *trimmed = copy_string(relative + root_length + 1U);

            if (trimmed != NULL) {
                free(relative);
                relative = trimmed;
            }
        }
    }

    if (!pebble_manifest_add_dependency(manifest, name, relative, "")) {
        (void)fprintf(stderr, "error: out of memory\n");
        goto cleanup;
    }

    if (!write_manifest(manifest_path, manifest)) {
        (void)fprintf(stderr, "error: cannot write '%s'\n", manifest_path);
        goto cleanup;
    }

    (void)printf("Added dependency '%s' from %s\n", name, relative);
    result = 0;

cleanup:
    pebble_manifest_diagnostics_destroy(&diagnostics);
    pebble_manifest_destroy(dependency);
    pebble_manifest_destroy(manifest);
    free(name);
    free(relative);
    free(manifest_path);
    free(dependency_path);
    return result;
}

int pebble_cli_remove(const char *root, const char *name)
{
    char *manifest_path = join_path(root, PEBBLE_MANIFEST_NAME);
    PebbleManifestDiagnostics diagnostics = {0};
    PebbleManifest *manifest = NULL;
    int result = 1;

    if (manifest_path == NULL) {
        (void)fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    if (!file_exists(manifest_path)) {
        (void)fprintf(stderr,
            "error: '%s' does not exist. Run 'pebble init' first.\n",
            manifest_path);
        goto cleanup;
    }

    manifest = pebble_manifest_load(manifest_path, &diagnostics);

    if (manifest == NULL) {
        char message[1024];

        describe_diagnostics(&diagnostics, message, sizeof(message));
        (void)fprintf(stderr, "error: %s\n", message);
        goto cleanup;
    }

    if (!pebble_manifest_remove_dependency(manifest, name)) {
        (void)fprintf(stderr,
            "error: the dependency '%s' is not declared.\n", name);
        goto cleanup;
    }

    if (!write_manifest(manifest_path, manifest)) {
        (void)fprintf(stderr, "error: cannot write '%s'\n", manifest_path);
        goto cleanup;
    }

    (void)printf("Removed dependency '%s'\n", name);
    result = 0;

cleanup:
    pebble_manifest_diagnostics_destroy(&diagnostics);
    pebble_manifest_destroy(manifest);
    free(manifest_path);
    return result;
}

static int install_project(const char *root)
{
    PebbleManifestDiagnostics diagnostics = {0};
    PebbleProject project = {0};
    size_t index;
    int result = 1;

    if (!pebble_project_load(root, &project, &diagnostics)) {
        char message[1024];

        describe_diagnostics(&diagnostics, message, sizeof(message));
        (void)fprintf(stderr, "error: %s\n", message);
        goto cleanup;
    }

    if (!pebble_project_resolve(&project, &diagnostics)) {
        char message[1024];

        describe_diagnostics(&diagnostics, message, sizeof(message));
        (void)fprintf(stderr, "error: %s\n", message);
        goto cleanup;
    }

    for (index = 0U; index < project.packages.count; ++index) {
        (void)printf("  %s %s (%s)\n",
            project.packages.items[index].name,
            project.packages.items[index].version,
            project.packages.items[index].path);
    }

    if (!pebble_project_write_lockfile(&project)) {
        (void)fprintf(stderr, "error: cannot write the lockfile.\n");
        goto cleanup;
    }

    (void)printf("Resolved %zu dependenc%s.\n", project.packages.count,
                 project.packages.count == 1U ? "y" : "ies");
    result = 0;

cleanup:
    pebble_manifest_diagnostics_destroy(&diagnostics);
    pebble_project_destroy(&project);
    return result;
}

int pebble_cli_install(const char *root)
{
    return install_project(root);
}

int pebble_cli_update(const char *root)
{
    return install_project(root);
}

int pebble_cli_tree(const char *root)
{
    PebbleCliProject project = {0};
    char message[1024];
    PebbleManifestDiagnostics diagnostics = {0};
    PebbleManifest *manifest = NULL;
    size_t index;
    int result = 1;

    if (!pebble_cli_load_project(root, &project, message, sizeof(message))) {
        (void)fprintf(stderr, "error: %s\n", message);
        goto cleanup;
    }

    manifest = pebble_manifest_load(project.manifest_path, &diagnostics);

    if (manifest == NULL) {
        describe_diagnostics(&diagnostics, message, sizeof(message));
        (void)fprintf(stderr, "error: %s\n", message);
        goto cleanup;
    }

    (void)printf("%s %s\n", manifest->name, manifest->version);

    for (index = 0U; index < project.search_path_count; ++index) {
        (void)printf("  %s\n", project.search_paths[index]);
    }

    result = 0;

cleanup:
    pebble_manifest_diagnostics_destroy(&diagnostics);
    pebble_manifest_destroy(manifest);
    pebble_cli_project_destroy(&project);
    return result;
}

int pebble_cli_package(const char *root)
{
    PebbleCliProject project = {0};
    char message[1024];
    PebbleManifestDiagnostics diagnostics = {0};
    PebbleManifest *manifest = NULL;
    char *distribution = NULL;
    char *manifest_copy = NULL;
#ifdef _WIN32
    HANDLE directory = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAA find_data;
    int have_entry = 0;
    char *search_pattern = NULL;
#else
    DIR *directory = NULL;
    struct dirent *entry = NULL;
#endif
    size_t copied = 0U;
    int result = 1;

    if (!pebble_cli_load_project(root, &project, message, sizeof(message))) {
        (void)fprintf(stderr, "error: %s\n", message);
        goto cleanup;
    }

    manifest = pebble_manifest_load(project.manifest_path, &diagnostics);

    if (manifest == NULL) {
        describe_diagnostics(&diagnostics, message, sizeof(message));
        (void)fprintf(stderr, "error: %s\n", message);
        goto cleanup;
    }

    {
        char *dist = join_path(root, "dist");
        char *label = NULL;
        size_t needed;

        if (dist != NULL) {
            needed = strlen(manifest->name) + strlen(manifest->version) + 3U;
            label = malloc(needed);

            if (label != NULL) {
                (void)snprintf(label, needed, "%s-%s", manifest->name,
                               manifest->version);
                distribution = join_path(dist, label);
            }
        }

        free(dist);
        free(label);
    }

    if (distribution == NULL) {
        (void)fprintf(stderr, "error: out of memory\n");
        goto cleanup;
    }

    if (!make_directories(distribution)) {
        (void)fprintf(stderr, "error: cannot create '%s'\n", distribution);
        goto cleanup;
    }

    manifest_copy = join_path(distribution, PEBBLE_MANIFEST_NAME);

    if (manifest_copy == NULL || !write_manifest(manifest_copy, manifest)) {
        (void)fprintf(stderr, "error: cannot write the package manifest.\n");
        goto cleanup;
    }

#ifdef _WIN32
    search_pattern = join_path(root, "*");
    if (search_pattern == NULL) {
        (void)fprintf(stderr, "error: out of memory\n");
        goto cleanup;
    }
    directory = FindFirstFileA(search_pattern, &find_data);
    free(search_pattern);
    search_pattern = NULL;
    if (directory == INVALID_HANDLE_VALUE) {
        const DWORD find_error = GetLastError();
        if (find_error != ERROR_FILE_NOT_FOUND) {
            (void)fprintf(stderr, "error: cannot read directory '%s'\n", root);
            goto cleanup;
        }
    } else {
        have_entry = 1;
        while (have_entry) {
                const char *name = find_data.cFileName;
                const size_t length = strlen(name);
                struct stat information;

                if (length >= 5U && strcmp(name + length - 4U, ".peb") == 0) {
                    char *source = join_path(root, name);
                    char *target = join_path(distribution, name);

                    if (source != NULL && target != NULL &&
                        stat(source, &information) == 0 &&
                        S_ISREG(information.st_mode) &&
                        copy_file(source, target)) {
                        copied++;
                    }

                    free(source);
                    free(target);
                }

                have_entry = FindNextFileA(directory, &find_data);
            }

            if (GetLastError() != ERROR_NO_MORE_FILES) {
                (void)fprintf(stderr, "error: cannot read directory '%s'\n", root);
                (void)FindClose(directory);
                directory = INVALID_HANDLE_VALUE;
                goto cleanup;
            }

            (void)FindClose(directory);
            directory = INVALID_HANDLE_VALUE;
        }
#else
    directory = opendir(root);

    if (directory != NULL) {
        while ((entry = readdir(directory)) != NULL) {
            const size_t length = strlen(entry->d_name);
            struct stat information;

            if (length < 5U || strcmp(entry->d_name + length - 4U, ".peb") != 0) {
                continue;
            }

            {
                char *source = join_path(root, entry->d_name);
                char *target = join_path(distribution, entry->d_name);

                if (source != NULL && target != NULL &&
                    stat(source, &information) == 0 &&
                    S_ISREG(information.st_mode) &&
                    copy_file(source, target)) {
                    copied++;
                }

                free(source);
                free(target);
            }
        }

        (void)closedir(directory);
        directory = NULL;
    }
#endif

    (void)printf("Packaged %s %s with %zu source file(s) into %s\n",
                 manifest->name, manifest->version, copied, distribution);
    result = 0;

cleanup:
#ifdef _WIN32
    if (directory != INVALID_HANDLE_VALUE) {
        (void)FindClose(directory);
    }
    free(search_pattern);
#else
    if (directory != NULL) {
        (void)closedir(directory);
    }
#endif

    pebble_manifest_diagnostics_destroy(&diagnostics);
    pebble_manifest_destroy(manifest);
    free(manifest_copy);
    free(distribution);
    pebble_cli_project_destroy(&project);
    return result;
}
