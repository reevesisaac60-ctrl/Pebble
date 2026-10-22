#include "pebble_manifest/project.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define WORKSPACE "/tmp/pebble-project-tests"

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void write_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        fprintf(stderr, "FAIL: cannot write '%s'.\n", path);
        exit(EXIT_FAILURE);
    }

    fwrite(text, 1U, strlen(text), file);
    fclose(file);
}

static void make_directory(const char *path)
{
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "FAIL: cannot create '%s'.\n", path);
        exit(EXIT_FAILURE);
    }
}

static int has_diagnostic(const PebbleManifestDiagnostics *diagnostics,
                          const char *fragment)
{
    size_t index;

    for (index = 0U; index < diagnostics->count; index++) {
        if (strstr(diagnostics->items[index].message, fragment) != NULL) {
            return 1;
        }
    }

    return 0;
}

static const PebbleResolvedPackage *find_package(const PebbleProject *project,
                                                  const char *name)
{
    size_t index;

    for (index = 0U; index < project->packages.count; index++) {
        if (strcmp(project->packages.items[index].name, name) == 0) {
            return &project->packages.items[index];
        }
    }

    return NULL;
}

static const PebbleResolvedPackage *find_resolved(const PebbleResolvedList *list,
                                                   const char *name)
{
    size_t index;

    for (index = 0U; index < list->count; index++) {
        if (strcmp(list->items[index].name, name) == 0) {
            return &list->items[index];
        }
    }

    return NULL;
}

static char *read_file(const char *path)
{
    static char contents[8192];
    FILE *file = fopen(path, "rb");
    size_t length;

    if (file == NULL) {
        return NULL;
    }

    length = fread(contents, 1U, sizeof(contents) - 1U, file);
    contents[length] = '\0';
    fclose(file);
    return contents;
}

static void test_project_load_finds_the_manifest_and_the_entry_point(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/demo/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n");
    write_file(WORKSPACE "/demo/src/main.peb",
               "fn main() {\n"
               "    print(1);\n"
               "}\n");

    require(pebble_project_load(WORKSPACE "/demo", &project, &diagnostics) != 0,
            "the project should load");
    require(diagnostics.count == 0U,
            "a valid manifest should not report diagnostics");
    require(project.manifest != NULL, "the project should hold a manifest");
    require(strcmp(project.manifest->name, "demo") == 0,
            "the manifest should hold the package name");
    require(project.entry_point != NULL,
            "the project should find the entry point");
    require(strstr(project.entry_point, "src/main.peb") != NULL,
            "the entry point should be the main file of the source directory");
    require(project.lockfile_path != NULL,
            "the project should know the lockfile");
    require(strstr(project.lockfile_path, "pebble.lock") != NULL,
            "the lockfile should be called pebble.lock");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_project_resolves_a_dependency_in_a_parent_directory(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};
    const PebbleResolvedPackage *resolved;

    write_file(WORKSPACE "/parent/demo/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "dep = { path = \"../dep\" }\n");
    write_file(WORKSPACE "/parent/dep/pebble.toml",
               "[package]\n"
               "name = \"dep\"\n"
               "version = \"0.2.0\"\n");

    require(pebble_project_load(WORKSPACE "/parent/demo",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) != 0,
            "the dependency should resolve");
    require(diagnostics.count == 0U,
            "a resolvable dependency should not report diagnostics");
    require(project.packages.count == 1U,
            "the project should hold one resolved package");

    resolved = find_package(&project, "dep");
    require(resolved != NULL, "the dependency should be resolved");
    require(strcmp(resolved->version, "0.2.0") == 0,
            "the resolved package should hold the version of the dependency");
    require(strcmp(resolved->path, "../dep") == 0,
            "the resolved package should hold the declared path");
    require(strstr(resolved->directory, "..") == NULL,
            "the resolved directory should be normalised");
    require(strstr(resolved->directory, "/parent/dep") != NULL,
            "the resolved directory should point at the dependency");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_project_reports_a_missing_dependency(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/missing/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "dep = { path = \"../nothing\" }\n");

    require(pebble_project_load(WORKSPACE "/missing",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) == 0,
            "a missing dependency should be reported");
    require(has_diagnostic(&diagnostics, "cannot be resolved"),
            "the diagnostic should mention the unresolved dependency");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_project_reports_a_dependency_cycle(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/cycle/alpha/pebble.toml",
               "[package]\n"
               "name = \"alpha\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "beta = { path = \"../beta\" }\n");
    write_file(WORKSPACE "/cycle/beta/pebble.toml",
               "[package]\n"
               "name = \"beta\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "alpha = { path = \"../alpha\" }\n");

    require(pebble_project_load(WORKSPACE "/cycle/alpha",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) == 0,
            "a dependency cycle should be reported");
    require(has_diagnostic(&diagnostics, "Circular dependency detected"),
            "the diagnostic should mention the cycle");
    require(has_diagnostic(&diagnostics,
                           "Circular dependency detected: alpha -> beta -> alpha."),
            "the diagnostic should name the packages of the cycle");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_lockfile_round_trip(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};
    PebbleResolvedList packages = {NULL, 0U, 0U};

    (void)remove(WORKSPACE "/lockfile/demo/pebble.lock");

    write_file(WORKSPACE "/lockfile/demo/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "dep = { path = \"../dep\" }\n");
    write_file(WORKSPACE "/lockfile/dep/pebble.toml",
               "[package]\n"
               "name = \"dep\"\n"
               "version = \"0.2.0\"\n");

    require(pebble_project_load(WORKSPACE "/lockfile/demo",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) != 0,
            "the dependency should resolve");
    require(pebble_project_write_lockfile(&project) != 0,
            "the lockfile should be written");

    require(pebble_project_read_lockfile(project.lockfile_path,
                                         &packages,
                                         &diagnostics) != 0,
            "the lockfile should be read");
    require(packages.count == 1U, "the lockfile should hold one package");
    require(strcmp(packages.items[0].name, "dep") == 0,
            "the lockfile should hold the name of the dependency");
    require(strcmp(packages.items[0].version, "0.2.0") == 0,
            "the lockfile should hold the version of the dependency");

    pebble_resolved_list_destroy(&packages);
    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_missing_manifest_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    require(pebble_project_load(WORKSPACE "/nothing",
                                &project,
                                &diagnostics) == 0,
            "a missing manifest should be reported");
    require(has_diagnostic(&diagnostics, "The manifest cannot be read."),
            "the diagnostic should mention the missing manifest");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_malformed_manifest_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/broken/pebble.toml",
               "[package]\n"
               "name \"demo\"\n");

    require(pebble_project_load(WORKSPACE "/broken",
                                &project,
                                &diagnostics) == 0,
            "a malformed manifest should be reported");
    require(has_diagnostic(&diagnostics, "Expected 'key = value'."),
            "the diagnostic should mention the malformed line");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_manifest_without_a_name_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/nameless/pebble.toml",
               "[package]\n"
               "version = \"1.0.0\"\n");

    require(pebble_project_load(WORKSPACE "/nameless",
                                &project,
                                &diagnostics) == 0,
            "a manifest without a name should be reported");
    require(has_diagnostic(&diagnostics, "The manifest needs a package name."),
            "the diagnostic should mention the missing name");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_manifest_without_a_version_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/versionless/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n");

    require(pebble_project_load(WORKSPACE "/versionless",
                                &project,
                                &diagnostics) == 0,
            "a manifest without a version should be reported");
    require(has_diagnostic(&diagnostics,
                           "The manifest needs a package version."),
            "the diagnostic should mention the missing version");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_an_invalid_version_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/badversion/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"one\"\n");

    require(pebble_project_load(WORKSPACE "/badversion",
                                &project,
                                &diagnostics) == 0,
            "an invalid version should be reported");
    require(has_diagnostic(&diagnostics,
                           "The package version is not a valid semantic version."),
            "the diagnostic should mention the invalid version");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_dependency_without_a_path_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/nopath/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "dep = { version = \"1.0.0\" }\n");

    require(pebble_project_load(WORKSPACE "/nopath",
                                &project,
                                &diagnostics) == 0,
            "a dependency without a path should be reported");
    require(has_diagnostic(&diagnostics, "A dependency needs a 'path'."),
            "the diagnostic should mention the missing path");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_duplicate_dependency_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/duplicate/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "dep = { path = \"../dep\" }\n"
               "dep = { path = \"../other\" }\n");

    require(pebble_project_load(WORKSPACE "/duplicate",
                                &project,
                                &diagnostics) == 0,
            "a duplicate dependency should be reported");
    require(has_diagnostic(&diagnostics,
                           "The dependency 'dep' is declared more than once."),
            "the diagnostic should mention the duplicate dependency");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_an_invalid_version_constraint_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/constraint/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "dep = { path = \"../dep\", version = \"newest\" }\n");

    require(pebble_project_load(WORKSPACE "/constraint",
                                &project,
                                &diagnostics) == 0,
            "an invalid version constraint should be reported");
    require(has_diagnostic(&diagnostics,
                           "The dependency version constraint is not a valid "
                           "version."),
            "the diagnostic should mention the invalid constraint");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_version_constraint_is_honoured(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/satisfied/demo/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "dep = { path = \"../dep\", version = \">=0.1.0\" }\n");
    write_file(WORKSPACE "/satisfied/dep/pebble.toml",
               "[package]\n"
               "name = \"dep\"\n"
               "version = \"0.2.0\"\n");

    require(pebble_project_load(WORKSPACE "/satisfied/demo",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) != 0,
            "a satisfied version constraint should resolve");
    require(diagnostics.count == 0U,
            "a satisfied version constraint should not report diagnostics");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_an_unsatisfied_version_constraint_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/unsatisfied/demo/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "dep = { path = \"../dep\", version = \">=2.0.0\" }\n");
    write_file(WORKSPACE "/unsatisfied/dep/pebble.toml",
               "[package]\n"
               "name = \"dep\"\n"
               "version = \"0.2.0\"\n");

    require(pebble_project_load(WORKSPACE "/unsatisfied/demo",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) == 0,
            "an unsatisfied version constraint should be reported");
    require(has_diagnostic(&diagnostics,
                           "The dependency 'dep' requires version '>=2.0.0' "
                           "but 0.2.0 is installed."),
            "the diagnostic should mention the versions");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_transitive_dependencies_are_resolved(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};
    const PebbleResolvedPackage *middle;
    const PebbleResolvedPackage *last;

    write_file(WORKSPACE "/chain/demo/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "middle = { path = \"../middle\" }\n");
    write_file(WORKSPACE "/chain/middle/pebble.toml",
               "[package]\n"
               "name = \"middle\"\n"
               "version = \"1.1.0\"\n"
               "\n"
               "[dependencies]\n"
               "last = { path = \"../last\" }\n");
    write_file(WORKSPACE "/chain/last/pebble.toml",
               "[package]\n"
               "name = \"last\"\n"
               "version = \"1.2.0\"\n");

    require(pebble_project_load(WORKSPACE "/chain/demo",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) != 0,
            "a chain of dependencies should resolve");
    require(project.packages.count == 2U,
            "the project should hold both dependencies");

    middle = find_package(&project, "middle");
    last = find_package(&project, "last");

    require(middle != NULL && last != NULL,
            "the chain should be resolved");
    require(strcmp(middle->version, "1.1.0") == 0,
            "the middle package should hold its version");
    require(strcmp(last->version, "1.2.0") == 0,
            "the last package should hold its version");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_missing_transitive_dependency_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/brokenchain/demo/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "middle = { path = \"../middle\" }\n");
    write_file(WORKSPACE "/brokenchain/middle/pebble.toml",
               "[package]\n"
               "name = \"middle\"\n"
               "version = \"1.1.0\"\n"
               "\n"
               "[dependencies]\n"
               "last = { path = \"../nothing\" }\n");

    require(pebble_project_load(WORKSPACE "/brokenchain/demo",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) == 0,
            "a missing transitive dependency should be reported");
    require(has_diagnostic(&diagnostics, "cannot be resolved"),
            "the diagnostic should mention the unresolved dependency");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_cycle_of_three_packages_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/longcycle/alpha/pebble.toml",
               "[package]\n"
               "name = \"alpha\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "beta = { path = \"../beta\" }\n");
    write_file(WORKSPACE "/longcycle/beta/pebble.toml",
               "[package]\n"
               "name = \"beta\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "gamma = { path = \"../gamma\" }\n");
    write_file(WORKSPACE "/longcycle/gamma/pebble.toml",
               "[package]\n"
               "name = \"gamma\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "alpha = { path = \"../alpha\" }\n");

    require(pebble_project_load(WORKSPACE "/longcycle/alpha",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) == 0,
            "a cycle of three packages should be reported");
    require(has_diagnostic(&diagnostics,
                           "Circular dependency detected: alpha -> beta -> "
                           "gamma -> alpha."),
            "the diagnostic should name the packages of the cycle");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_package_that_depends_on_itself_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/selfcycle/demo/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "other = { path = \".\" }\n");

    require(pebble_project_load(WORKSPACE "/selfcycle/demo",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) == 0,
            "a package that depends on itself should be reported");
    require(has_diagnostic(&diagnostics,
                           "Circular dependency detected: demo -> other."),
            "the diagnostic should name the packages of the cycle");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_the_api_tolerates_null_arguments(void)
{
    PebbleManifest *manifest;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};
    PebbleResolvedList packages = {NULL, 0U, 0U};

    require(pebble_project_load(WORKSPACE, NULL, &diagnostics) == 0,
            "loading into a null project should fail");
    require(pebble_project_resolve(NULL, &diagnostics) == 0,
            "resolving a null project should fail");
    require(pebble_project_write_lockfile(NULL) == 0,
            "writing the lockfile of a null project should fail");
    require(pebble_project_read_lockfile(WORKSPACE "/pebble.lock", NULL,
                                         &diagnostics) == 0,
            "reading into a null list should fail");
    require(pebble_project_read_lockfile(NULL, &packages, &diagnostics) == 0,
            "reading a null lockfile should fail");

    pebble_project_destroy(NULL);
    pebble_resolved_list_destroy(NULL);
    pebble_manifest_destroy(NULL);
    pebble_manifest_diagnostics_destroy(NULL);

    manifest = pebble_manifest_parse(NULL, 0U, "pebble.toml", &diagnostics);

    require(manifest == NULL, "parsing a null manifest should fail");
    require(pebble_manifest_load(NULL, &diagnostics) == NULL,
            "loading a null manifest should fail");
    require(pebble_manifest_find_dependency(NULL, "dep") == NULL,
            "looking up a dependency in a null manifest should fail");
    require(pebble_manifest_write(NULL) == NULL,
            "writing a null manifest should fail");
    require(pebble_manifest_add_author(NULL, "Author") == 0,
            "adding an author to a null manifest should fail");
    require(pebble_manifest_add_dependency(NULL, "dep", "../dep", NULL) == 0,
            "adding a dependency to a null manifest should fail");
    require(pebble_manifest_remove_dependency(NULL, "dep") == 0,
            "removing a dependency from a null manifest should fail");
    require(pebble_version_compare(NULL, "1.0.0") == 0,
            "comparing a null version should return zero");
    require(pebble_version_compare("1.0.0", NULL) == 0,
            "comparing a null version should return zero");
    require(pebble_version_satisfies(NULL, "1.0.0") == 0,
            "a null version satisfies nothing");

    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_the_lockfile_keeps_a_long_path(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};
    PebbleResolvedList packages = {NULL, 0U, 0U};
    const PebbleResolvedPackage *dependency;
    const char *segment = "segmentsegmentsegmentsegmentsegmentsegmentsegmentseg";
    char relative[1024];
    char directory[1024];
    char manifest[2048];
    size_t index;

    relative[0] = '\0';
    directory[0] = '\0';

    make_directory(WORKSPACE "/long");
    make_directory(WORKSPACE "/long/demo");

    for (index = 0U; index < 10U; ++index) {
        (void)snprintf(directory + strlen(directory),
                       sizeof(directory) - strlen(directory), "%s/%s",
                       index == 0U ? WORKSPACE "/long" : "", segment);
        make_directory(directory);
        (void)snprintf(relative + strlen(relative),
                       sizeof(relative) - strlen(relative), "%s/%s",
                       index == 0U ? ".." : "", segment);
    }

    (void)snprintf(directory + strlen(directory),
                   sizeof(directory) - strlen(directory), "/dep");
    make_directory(directory);
    (void)snprintf(relative + strlen(relative),
                   sizeof(relative) - strlen(relative), "/dep");

    (void)snprintf(manifest, sizeof(manifest),
                   "[package]\n"
                   "name = \"demo\"\n"
                   "version = \"1.0.0\"\n"
                   "\n"
                   "[dependencies]\n"
                   "dep = { path = \"%s\" }\n",
                   relative);
    write_file(WORKSPACE "/long/demo/pebble.toml", manifest);

    {
        char full[1600];

        (void)snprintf(full, sizeof(full), "%s/pebble.toml", directory);
        write_file(full,
                   "[package]\n"
                   "name = \"dep\"\n"
                   "version = \"0.2.0\"\n");
    }

    require(pebble_project_load(WORKSPACE "/long/demo",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) != 0,
            "the dependency should resolve");
    require(pebble_project_write_lockfile(&project) != 0,
            "the lockfile should be written");
    require(pebble_project_read_lockfile(project.lockfile_path,
                                         &packages,
                                         &diagnostics) != 0,
            "the lockfile should be read");

    dependency = find_resolved(&packages, "dep");

    require(dependency != NULL, "the lockfile should hold the dependency");
    require(strcmp(dependency->path, relative) == 0,
            "the lockfile should keep the whole path");

    pebble_resolved_list_destroy(&packages);
    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_manifest_round_trips_special_characters(void)
{
    PebbleManifest *manifest;
    PebbleManifest *parsed;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};
    PebbleManifestDiagnostics parse_diagnostics = {NULL, 0U, 0U};
    const PebbleManifestDependency *dependency;
    char *text;

    manifest = pebble_manifest_create("demo", "1.0.0");

    require(manifest != NULL, "the manifest should be created");
    pebble_manifest_set_description(manifest, "a \"quoted\" # description");
    pebble_manifest_set_license(manifest, "a \\ license");
    require(pebble_manifest_add_author(manifest, "A \"quoted\" Author") != 0,
            "the author should be added");
    require(pebble_manifest_add_dependency(manifest, "dep",
                                            "../a\"b", ">=0.1.0") != 0,
            "the dependency should be added");

    text = pebble_manifest_write(manifest);

    require(text != NULL, "the manifest should be written");

    parsed = pebble_manifest_parse(text, strlen(text), "pebble.toml",
                                   &parse_diagnostics);

    require(parsed != NULL, "the manifest should be parsed");
    require(parse_diagnostics.count == 0U,
            "the manifest should not report diagnostics");
    require(strcmp(parsed->name, "demo") == 0, "the name should round trip");
    require(strcmp(parsed->version, "1.0.0") == 0,
            "the version should round trip");
    require(strcmp(parsed->description, "a \"quoted\" # description") == 0,
            "the description should round trip");
    require(strcmp(parsed->license, "a \\ license") == 0,
            "the licence should round trip");
    require(parsed->author_count == 1U, "the author should round trip");
    require(strcmp(parsed->authors[0], "A \"quoted\" Author") == 0,
            "the author name should round trip");

    dependency = pebble_manifest_find_dependency(parsed, "dep");

    require(dependency != NULL, "the dependency should round trip");
    require(strcmp(dependency->path, "../a\"b") == 0,
            "the dependency path should round trip");
    require(strcmp(dependency->constraint, ">=0.1.0") == 0,
            "the version constraint should round trip");

    free(text);
    pebble_manifest_destroy(parsed);
    pebble_manifest_destroy(manifest);
    pebble_manifest_diagnostics_destroy(&parse_diagnostics);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_manifest_is_rejected_when_a_dependency_path_is_malformed(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    make_directory(WORKSPACE "/malformed");
    write_file(WORKSPACE "/malformed/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "dep = { path = \"unterminated }\n");

    require(pebble_project_load(WORKSPACE "/malformed", &project, &diagnostics) == 0,
            "the project should not load");
    require(diagnostics.count > 0U, "the diagnostic should be reported");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_conflicting_dependency_is_reported(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/conflict/demo/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "first = { path = \"../first\" }\n"
               "second = { path = \"../second\" }\n");
    write_file(WORKSPACE "/conflict/first/pebble.toml",
               "[package]\n"
               "name = \"first\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "shared = { path = \"../../shared\" }\n");
    write_file(WORKSPACE "/conflict/second/pebble.toml",
               "[package]\n"
               "name = \"second\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "shared = { path = \"../../elsewhere\" }\n");
    write_file(WORKSPACE "/shared/pebble.toml",
               "[package]\n"
               "name = \"shared\"\n"
               "version = \"1.0.0\"\n");
    write_file(WORKSPACE "/elsewhere/pebble.toml",
               "[package]\n"
               "name = \"shared\"\n"
               "version = \"1.0.0\"\n");

    require(pebble_project_load(WORKSPACE "/conflict/demo",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) == 0,
            "a dependency from two locations should be reported");
    require(has_diagnostic(&diagnostics,
                           "The dependency 'shared' is required from two "
                           "different locations."),
            "the diagnostic should mention the conflict");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_dependency_paths_are_normalised(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};
    const PebbleResolvedPackage *dependency;

    write_file(WORKSPACE "/normalise/demo/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "dep = { path = \".././dep/\" }\n");
    write_file(WORKSPACE "/normalise/dep/pebble.toml",
               "[package]\n"
               "name = \"dep\"\n"
               "version = \"0.2.0\"\n");

    require(pebble_project_load(WORKSPACE "/normalise/demo",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) != 0,
            "a dependency with a messy path should resolve");

    dependency = find_package(&project, "dep");

    require(dependency != NULL, "the dependency should be resolved");
    require(strstr(dependency->directory, "..") == NULL,
            "the resolved directory should not hold a parent segment");
    require(strstr(dependency->directory, "/dep") != NULL,
            "the resolved directory should point at the dependency");
    require(strstr(dependency->directory, "//") == NULL,
            "the resolved directory should not hold an empty segment");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_the_lockfile_is_stable(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};
    char first[8192];
    char second[8192];

    (void)remove(WORKSPACE "/stable/demo/pebble.lock");

    write_file(WORKSPACE "/stable/demo/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "dep = { path = \"../dep\" }\n");
    write_file(WORKSPACE "/stable/dep/pebble.toml",
               "[package]\n"
               "name = \"dep\"\n"
               "version = \"0.2.0\"\n");

    require(pebble_project_load(WORKSPACE "/stable/demo",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) != 0,
            "the dependency should resolve");
    require(pebble_project_write_lockfile(&project) != 0,
            "the lockfile should be written");
    require(read_file(project.lockfile_path) != NULL,
            "the lockfile should be readable");
    (void)snprintf(first, sizeof(first), "%s", read_file(project.lockfile_path));

    require(pebble_project_write_lockfile(&project) != 0,
            "the lockfile should be written again");
    (void)snprintf(second, sizeof(second), "%s",
                   read_file(project.lockfile_path));

    require(strcmp(first, second) == 0,
            "the lockfile should be written the same way every time");
    require(strstr(first, "lockfile_version = 1") != NULL,
            "the lockfile should hold its version");
    require(strstr(first, "[[package]]") != NULL,
            "the lockfile should hold a package record");

    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_malformed_lockfile_is_reported(void)
{
    PebbleResolvedList packages = {NULL, 0U, 0U};
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/malformed.lock",
               "lockfile_version = 1\n"
               "\n"
               "[[package]]\n"
               "version = \"1.0.0\"\n"
               "path = \"../dep\"\n");

    require(pebble_project_read_lockfile(WORKSPACE "/malformed.lock",
                                         &packages,
                                         &diagnostics) == 0,
            "a lockfile without a package name should be reported");
    require(has_diagnostic(&diagnostics,
                           "A package in the lockfile needs a name."),
            "the diagnostic should mention the missing name");

    pebble_resolved_list_destroy(&packages);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_an_unsupported_lockfile_version_is_reported(void)
{
    PebbleResolvedList packages = {NULL, 0U, 0U};
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    write_file(WORKSPACE "/future.lock",
               "lockfile_version = 2\n"
               "\n"
               "[[package]]\n"
               "name = \"dep\"\n"
               "version = \"1.0.0\"\n"
               "path = \"../dep\"\n");

    require(pebble_project_read_lockfile(WORKSPACE "/future.lock",
                                         &packages,
                                         &diagnostics) == 0,
            "an unsupported lockfile version should be reported");
    require(has_diagnostic(&diagnostics,
                           "The lockfile version '2' is not supported."),
            "the diagnostic should mention the lockfile version");

    pebble_resolved_list_destroy(&packages);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_a_missing_lockfile_is_not_an_error(void)
{
    PebbleResolvedList packages = {NULL, 0U, 0U};
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};

    (void)remove(WORKSPACE "/absent.lock");

    require(pebble_project_read_lockfile(WORKSPACE "/absent.lock",
                                         &packages,
                                         &diagnostics) != 0,
            "a missing lockfile should not be an error");
    require(diagnostics.count == 0U,
            "a missing lockfile should not report diagnostics");
    require(packages.count == 0U, "a missing lockfile should hold no package");

    pebble_resolved_list_destroy(&packages);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

static void test_the_lockfile_keeps_special_characters(void)
{
    PebbleProject project;
    PebbleManifestDiagnostics diagnostics = {NULL, 0U, 0U};
    PebbleResolvedList packages = {NULL, 0U, 0U};
    const PebbleResolvedPackage *dependency;

    (void)remove(WORKSPACE "/special/demo/pebble.lock");

    write_file(WORKSPACE "/special/demo/pebble.toml",
               "[package]\n"
               "name = \"demo\"\n"
               "version = \"1.0.0\"\n"
               "\n"
               "[dependencies]\n"
               "dep = { path = \"../a\\\"b\" }\n");
    write_file(WORKSPACE "/special/a\"b/pebble.toml",
               "[package]\n"
               "name = \"dep\"\n"
               "version = \"0.2.0\"\n");

    require(pebble_project_load(WORKSPACE "/special/demo",
                                &project,
                                &diagnostics) != 0,
            "the project should load");
    require(pebble_project_resolve(&project, &diagnostics) != 0,
            "the dependency should resolve");
    require(pebble_project_write_lockfile(&project) != 0,
            "the lockfile should be written");
    require(pebble_project_read_lockfile(project.lockfile_path,
                                         &packages,
                                         &diagnostics) != 0,
            "the lockfile should be read");

    dependency = find_resolved(&packages, "dep");

    require(dependency != NULL, "the lockfile should hold the dependency");
    require(strcmp(dependency->path, "../a\"b") == 0,
            "the lockfile should keep the path as it is");

    pebble_resolved_list_destroy(&packages);
    pebble_project_destroy(&project);
    pebble_manifest_diagnostics_destroy(&diagnostics);
}

int main(void)
{
    make_directory(WORKSPACE);
    make_directory(WORKSPACE "/demo");
    make_directory(WORKSPACE "/demo/src");
    make_directory(WORKSPACE "/parent");
    make_directory(WORKSPACE "/parent/demo");
    make_directory(WORKSPACE "/parent/dep");
    make_directory(WORKSPACE "/missing");
    make_directory(WORKSPACE "/cycle");
    make_directory(WORKSPACE "/cycle/alpha");
    make_directory(WORKSPACE "/cycle/beta");
    make_directory(WORKSPACE "/lockfile");
    make_directory(WORKSPACE "/lockfile/demo");
    make_directory(WORKSPACE "/lockfile/dep");
    make_directory(WORKSPACE "/broken");
    make_directory(WORKSPACE "/nameless");
    make_directory(WORKSPACE "/versionless");
    make_directory(WORKSPACE "/badversion");
    make_directory(WORKSPACE "/nopath");
    make_directory(WORKSPACE "/duplicate");
    make_directory(WORKSPACE "/constraint");
    make_directory(WORKSPACE "/satisfied");
    make_directory(WORKSPACE "/satisfied/demo");
    make_directory(WORKSPACE "/satisfied/dep");
    make_directory(WORKSPACE "/unsatisfied");
    make_directory(WORKSPACE "/unsatisfied/demo");
    make_directory(WORKSPACE "/unsatisfied/dep");
    make_directory(WORKSPACE "/chain");
    make_directory(WORKSPACE "/chain/demo");
    make_directory(WORKSPACE "/chain/middle");
    make_directory(WORKSPACE "/chain/last");
    make_directory(WORKSPACE "/brokenchain");
    make_directory(WORKSPACE "/brokenchain/demo");
    make_directory(WORKSPACE "/brokenchain/middle");
    make_directory(WORKSPACE "/longcycle");
    make_directory(WORKSPACE "/longcycle/alpha");
    make_directory(WORKSPACE "/longcycle/beta");
    make_directory(WORKSPACE "/longcycle/gamma");
    make_directory(WORKSPACE "/selfcycle");
    make_directory(WORKSPACE "/selfcycle/demo");
    make_directory(WORKSPACE "/conflict");
    make_directory(WORKSPACE "/conflict/demo");
    make_directory(WORKSPACE "/conflict/first");
    make_directory(WORKSPACE "/conflict/second");
    make_directory(WORKSPACE "/shared");
    make_directory(WORKSPACE "/elsewhere");
    make_directory(WORKSPACE "/normalise");
    make_directory(WORKSPACE "/normalise/demo");
    make_directory(WORKSPACE "/normalise/dep");
    make_directory(WORKSPACE "/stable");
    make_directory(WORKSPACE "/stable/demo");
    make_directory(WORKSPACE "/stable/dep");
    make_directory(WORKSPACE "/special");
    make_directory(WORKSPACE "/special/demo");
    make_directory(WORKSPACE "/special/a\"b");

    test_project_load_finds_the_manifest_and_the_entry_point();
    test_project_resolves_a_dependency_in_a_parent_directory();
    test_project_reports_a_missing_dependency();
    test_project_reports_a_dependency_cycle();
    test_lockfile_round_trip();
    test_a_missing_manifest_is_reported();
    test_a_malformed_manifest_is_reported();
    test_a_manifest_without_a_name_is_reported();
    test_a_manifest_without_a_version_is_reported();
    test_an_invalid_version_is_reported();
    test_a_dependency_without_a_path_is_reported();
    test_a_duplicate_dependency_is_reported();
    test_an_invalid_version_constraint_is_reported();
    test_a_version_constraint_is_honoured();
    test_an_unsatisfied_version_constraint_is_reported();
    test_transitive_dependencies_are_resolved();
    test_a_missing_transitive_dependency_is_reported();
    test_a_cycle_of_three_packages_is_reported();
    test_a_package_that_depends_on_itself_is_reported();
    test_the_api_tolerates_null_arguments();
    test_the_lockfile_keeps_a_long_path();
    test_a_manifest_round_trips_special_characters();
    test_a_manifest_is_rejected_when_a_dependency_path_is_malformed();
    test_a_conflicting_dependency_is_reported();
    test_dependency_paths_are_normalised();
    test_the_lockfile_is_stable();
    test_a_malformed_lockfile_is_reported();
    test_an_unsupported_lockfile_version_is_reported();
    test_a_missing_lockfile_is_not_an_error();
    test_the_lockfile_keeps_special_characters();

    printf("All project tests passed.\n");
    return EXIT_SUCCESS;
}
