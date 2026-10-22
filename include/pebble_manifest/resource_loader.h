#ifndef PEBBLE_MANIFEST_RESOURCE_LOADER_H
#define PEBBLE_MANIFEST_RESOURCE_LOADER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PEBBLE_RESOURCE_OK = 0,
    PEBBLE_RESOURCE_INVALID_ID = 1,
    PEBBLE_RESOURCE_MISSING = 2,
    PEBBLE_RESOURCE_UNREADABLE = 3,
    PEBBLE_RESOURCE_TOO_LARGE = 4,
    PEBBLE_RESOURCE_UNSUPPORTED = 5
} PebbleResourceStatus;

typedef struct {
    char *identifier;
    char *type;
    char *data;
    size_t size;
    PebbleResourceStatus status;
} PebbleLoadedResource;

typedef struct {
    char *identifier;
    unsigned char *data;
    size_t size;
    PebbleResourceStatus status;
} PebbleBinaryResource;

int pebble_resource_normalize(const char *id, char *out, size_t out_size);

PebbleResourceStatus pebble_resource_load_text(const char *root, const char *id,
                                             PebbleLoadedResource *out);

PebbleResourceStatus pebble_resource_load_binary(const char *root, const char *id,
                                               PebbleBinaryResource *out);

void pebble_loaded_resource_destroy(PebbleLoadedResource *r);
void pebble_binary_resource_destroy(PebbleBinaryResource *r);

#ifdef __cplusplus
}
#endif

#endif
