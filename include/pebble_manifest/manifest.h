#ifndef PEBBLE_MANIFEST_H
#define PEBBLE_MANIFEST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *file;
    int line;
    int column;
    char *message;
} PebbleManifestDiagnostic;

typedef struct {
    PebbleManifestDiagnostic *items;
    size_t count;
    size_t capacity;
} PebbleManifestDiagnostics;

typedef struct {
    char *name;
    char *path;
    char *constraint;
} PebbleManifestDependency;

typedef struct {
    char *name;
    char *version;
    char *description;
    char *license;
    char *toolchain;
    char **authors;
    size_t author_count;
    PebbleManifestDependency *dependencies;
    size_t dependency_count;
} PebbleManifest;

void pebble_manifest_report(
    PebbleManifestDiagnostics *diagnostics,
    const char *path,
    int line,
    int column,
    const char *message
);

void pebble_manifest_diagnostics_destroy(
    PebbleManifestDiagnostics *diagnostics
);

void pebble_manifest_destroy(PebbleManifest *manifest);

PebbleManifest *pebble_manifest_parse(
    const char *text,
    size_t length,
    const char *path,
    PebbleManifestDiagnostics *diagnostics
);

PebbleManifest *pebble_manifest_load(
    const char *path,
    PebbleManifestDiagnostics *diagnostics
);

PebbleManifest *pebble_manifest_create(const char *name, const char *version);

void pebble_manifest_set_description(PebbleManifest *manifest,
                                      const char *description);
void pebble_manifest_set_license(PebbleManifest *manifest, const char *license);
void pebble_manifest_set_toolchain(PebbleManifest *manifest,
                                   const char *toolchain);

int pebble_manifest_add_author(PebbleManifest *manifest, const char *author);
int pebble_manifest_add_dependency(PebbleManifest *manifest,
                                   const char *name,
                                   const char *path,
                                   const char *constraint);

int pebble_manifest_remove_dependency(PebbleManifest *manifest, const char *name);

char *pebble_manifest_write(const PebbleManifest *manifest);

const PebbleManifestDependency *pebble_manifest_find_dependency(
    const PebbleManifest *manifest,
    const char *name
);

int pebble_version_compare(const char *left, const char *right);

int pebble_version_satisfies(const char *version, const char *constraint);

#ifdef __cplusplus
}
#endif

#endif
