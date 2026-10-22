#ifndef PEBBLE_MANIFEST_RESOURCES_H
#define PEBBLE_MANIFEST_RESOURCES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *relative_path;
    char *type;
    unsigned long long size;
    unsigned long long hash;
} PebbleResource;

typedef struct {
    PebbleResource *items;
    size_t count;
    size_t capacity;
} PebbleResourceList;

int pebble_resource_scan(const char *root, PebbleResourceList *out);

void pebble_resource_list_destroy(PebbleResourceList *list);

unsigned long long pebble_file_hash(const char *path);

#ifdef __cplusplus
}
#endif

#endif
