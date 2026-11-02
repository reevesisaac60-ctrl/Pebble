#include "pebble_manifest/resource_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#define IS_REG_MODE(m) (((m) & _S_IFMT) == _S_IFREG)
#else
#define IS_REG_MODE(m) S_ISREG(m)
#endif

#define PEBBLE_RESOURCE_MAX_SIZE (1024 * 1024)

static int is_sep(char c) { return c == '/' || c == '\\'; }

int pebble_resource_normalize(const char *id, char *out, size_t out_size) {
    if (id == NULL || out == NULL || out_size == 0) return -1;
    if (id[0] == '/' || id[0] == '\\') return -1;
    if (id[1] == ':') return -1;
    size_t oi = 0;
    const char *p = id;
    while (*p) {
        if (is_sep(*p)) {
            if (oi > 0 && out[oi-1] != '/') {
                if (oi + 1 >= out_size) return -1;
                out[oi++] = '/';
            }
            p++;
            continue;
        }
        if (p[0] == '.' && p[1] == '.' && (p[2] == '\0' || is_sep(p[2]))) {
            return -1;
        }
        if (oi + 1 >= out_size) return -1;
        out[oi++] = *p++;
    }
    if (oi == 0) return -1;
    if (out[oi-1] == '/') { if (oi == 1) return -1; oi--; }
    out[oi] = '\0';
    return 0;
}

PebbleResourceStatus pebble_resource_load_text(const char *root, const char *id,
                                             PebbleLoadedResource *out) {
    if (root == NULL || id == NULL || out == NULL) return PEBBLE_RESOURCE_INVALID_ID;
    memset(out, 0, sizeof(*out));
    char norm[4096];
    if (pebble_resource_normalize(id, norm, sizeof(norm)) != 0) {
        out->status = PEBBLE_RESOURCE_INVALID_ID;
        return out->status;
    }
    char path[8192];
    snprintf(path, sizeof(path), "%s/%s", root, norm);
    struct stat st;
    if (stat(path, &st) != 0) {
        out->status = PEBBLE_RESOURCE_MISSING;
        return out->status;
    }
    if (!IS_REG_MODE(st.st_mode)) {
        out->status = PEBBLE_RESOURCE_UNSUPPORTED;
        return out->status;
    }
    if ((size_t)(st.st_size < 0 ? 0 : st.st_size) > PEBBLE_RESOURCE_MAX_SIZE) {
        out->status = PEBBLE_RESOURCE_TOO_LARGE;
        return out->status;
    }
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        out->status = PEBBLE_RESOURCE_UNREADABLE;
        return out->status;
    }
    char *buf = (char *)malloc((size_t)(st.st_size < 0 ? 0 : st.st_size) + 1);
    if (buf == NULL) { fclose(f); out->status = PEBBLE_RESOURCE_UNREADABLE; return out->status; }
    if (st.st_size > 0 && fread(buf, 1, (size_t)(st.st_size < 0 ? 0 : st.st_size), f) != (size_t)(st.st_size < 0 ? 0 : st.st_size)) {
        free(buf); fclose(f); out->status = PEBBLE_RESOURCE_UNREADABLE; return out->status;
    }
    buf[st.st_size] = '\0';
    fclose(f);
    out->identifier = (char *)malloc(strlen(norm)+1);
    strcpy(out->identifier, norm);
    out->type = (char *)malloc(5); strcpy(out->type, "text");
    out->data = buf;
    out->size = (size_t)(st.st_size < 0 ? 0 : st.st_size);
    out->status = PEBBLE_RESOURCE_OK;
    return out->status;
}

void pebble_loaded_resource_destroy(PebbleLoadedResource *r) {
    if (r == NULL) return;
    free(r->identifier);
    free(r->type);
    free(r->data);
    memset(r, 0, sizeof(*r));
}

PebbleResourceStatus pebble_resource_load_binary(const char *root, const char *id,
                                               PebbleBinaryResource *out) {
    if (root == NULL || id == NULL || out == NULL) return PEBBLE_RESOURCE_INVALID_ID;
    memset(out, 0, sizeof(*out));
    char norm[4096];
    if (pebble_resource_normalize(id, norm, sizeof(norm)) != 0) {
        out->status = PEBBLE_RESOURCE_INVALID_ID;
        return out->status;
    }
    char path[8192];
    snprintf(path, sizeof(path), "%s/%s", root, norm);
    struct stat st;
    if (stat(path, &st) != 0) {
        out->status = PEBBLE_RESOURCE_MISSING;
        return out->status;
    }
    if (!IS_REG_MODE(st.st_mode)) {
        out->status = PEBBLE_RESOURCE_UNSUPPORTED;
        return out->status;
    }
    if (st.st_size < 0 || (size_t)st.st_size > PEBBLE_RESOURCE_MAX_SIZE) {
        out->status = PEBBLE_RESOURCE_TOO_LARGE;
        return out->status;
    }
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        out->status = PEBBLE_RESOURCE_UNREADABLE;
        return out->status;
    }
    size_t sz = (size_t)st.st_size;
    unsigned char *buf = sz == 0 ? NULL : (unsigned char *)malloc(sz);
    if (sz > 0 && buf == NULL) { fclose(f); out->status = PEBBLE_RESOURCE_UNREADABLE; return out->status; }
    if (sz > 0 && fread(buf, 1, sz, f) != sz) {
        free(buf); fclose(f); out->status = PEBBLE_RESOURCE_UNREADABLE; return out->status;
    }
    fclose(f);
    out->identifier = (char *)malloc(strlen(norm)+1);
    strcpy(out->identifier, norm);
    out->data = buf;
    out->size = sz;
    out->status = PEBBLE_RESOURCE_OK;
    return out->status;
}

void pebble_binary_resource_destroy(PebbleBinaryResource *r) {
    if (r == NULL) return;
    free(r->identifier);
    free(r->data);
    memset(r, 0, sizeof(*r));
}
