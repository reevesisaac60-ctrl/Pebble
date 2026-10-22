#ifndef PEBBLE_MANIFEST_PROJECT_H
#define PEBBLE_MANIFEST_PROJECT_H

#include "pebble_manifest/manifest.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *name;
    char *version;
    char *path;
    char *directory;
} PebbleResolvedPackage;

typedef struct {
    PebbleResolvedPackage *items;
    size_t count;
    size_t capacity;
} PebbleResolvedList;

typedef struct {
    char *root;
    char *manifest_path;
    char *entry_point;
    char *lockfile_path;
    PebbleManifest *manifest;
    PebbleResolvedList packages;
} PebbleProject;

#define PEBBLE_MANIFEST_NAME "pebble.toml"

#define PEBBLE_LOCKFILE_NAME "pebble.lock"

int pebble_project_load(
    const char *root,
    PebbleProject *project,
    PebbleManifestDiagnostics *diagnostics
);

int pebble_project_resolve(
    PebbleProject *project,
    PebbleManifestDiagnostics *diagnostics
);

int pebble_project_write_lockfile(const PebbleProject *project);

int pebble_project_read_lockfile(
    const char *path,
    PebbleResolvedList *packages,
    PebbleManifestDiagnostics *diagnostics
);

void pebble_project_destroy(PebbleProject *project);

void pebble_resolved_list_destroy(PebbleResolvedList *packages);

#ifdef __cplusplus
}
#endif

#endif
