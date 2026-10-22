#ifndef PEBBLE_C_CLI_H
#define PEBBLE_C_CLI_H

#include "pebble_manifest/manifest.h"
#include "pebble_manifest/project.h"

#include <stddef.h>

typedef struct {
    char *root;
    char *manifest_path;
    char *entry_point;
    char **search_paths;
    size_t search_path_count;
} PebbleCliProject;

int pebble_cli_load_project(const char *root,
                            PebbleCliProject *project,
                            char *message,
                            size_t message_size);

void pebble_cli_project_destroy(PebbleCliProject *project);

int pebble_cli_init(const char *root, const char *name);
int pebble_cli_add(const char *root, const char *path);
int pebble_cli_remove(const char *root, const char *name);
int pebble_cli_install(const char *root);
int pebble_cli_update(const char *root);
int pebble_cli_tree(const char *root);
int pebble_cli_package(const char *root);

#endif
