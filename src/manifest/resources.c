#include "pebble_manifest/resources.h"

#ifndef _WIN32
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <dirent.h>
#include <unistd.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <direct.h>
#include <io.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#define IS_DIR_MODE(m) (((m) & _S_IFMT) == _S_IFDIR)
#define IS_REG_MODE(m) (((m) & _S_IFMT) == _S_IFREG)
#else
#define IS_DIR_MODE(m) S_ISDIR(m)
#define IS_REG_MODE(m) S_ISREG(m)
#endif

static int is_ignored_dir(const char *name) {
    return strcmp(name, ".pebble") == 0 ||
           strcmp(name, "build") == 0 ||
           strcmp(name, ".git") == 0;
}

static const char *resource_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (dot == NULL) return "unknown";
    if (strcmp(dot, ".peb") == 0) return "source";
    if (strcmp(dot, ".toml") == 0) return "manifest";
    if (strcmp(dot, ".json") == 0) return "json";
    if (strcmp(dot, ".lock") == 0) return "lockfile";
    return "binary";
}

static char *dup_string(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p != NULL) memcpy(p, s, n);
    return p;
}

static int append_resource(PebbleResourceList *list, const char *relpath,
                           const char *fullpath) {
    struct stat st;
    if (stat(fullpath, &st) != 0) return -1;
    if (list->count == list->capacity) {
        size_t newcap = list->capacity == 0 ? 16 : list->capacity * 2;
        PebbleResource *tmp = (PebbleResource *)realloc(
            list->items, newcap * sizeof(PebbleResource));
        if (tmp == NULL) return -1;
        list->items = tmp;
        list->capacity = newcap;
    }
    list->items[list->count].relative_path = dup_string(relpath);
    list->items[list->count].type = dup_string(resource_type(relpath));
    list->items[list->count].size = (unsigned long long)st.st_size;
    list->items[list->count].hash = pebble_file_hash(fullpath);
    list->count++;
    return 0;
}

static int compare_resources(const void *a, const void *b) {
    const PebbleResource *ra = (const PebbleResource *)a;
    const PebbleResource *rb = (const PebbleResource *)b;
    return strcmp(ra->relative_path, rb->relative_path);
}

static void scan_dir(const char *root, const char *prefix, PebbleResourceList *out) {
    char path[8192];
    if (prefix[0] == '\0') {
        snprintf(path, sizeof(path), "%s", root);
    } else {
        snprintf(path, sizeof(path), "%s/%s", root, prefix);
    }
#ifdef _WIN32
    char search[8192];
    snprintf(search, sizeof(search), "%s/*", path);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0 ||
            strcmp(fd.cFileName, "..") == 0) continue;
        if (is_ignored_dir(fd.cFileName)) continue;
        char rel[8192];
        if (prefix[0] == '\0') {
            snprintf(rel, sizeof(rel), "%s", fd.cFileName);
        } else {
            snprintf(rel, sizeof(rel), "%s/%s", prefix, fd.cFileName);
        }
        char full[8192];
        snprintf(full, sizeof(full), "%s/%s", root, rel);
        struct _stat st;
        if (_stat(full, &st) != 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scan_dir(root, rel, out);
        } else if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            append_resource(out, rel, full);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *dir = opendir(path);
    if (dir == NULL) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) continue;
        if (is_ignored_dir(entry->d_name)) continue;
        char rel[8192];
        if (prefix[0] == '\0') {
            snprintf(rel, sizeof(rel), "%s", entry->d_name);
        } else {
            snprintf(rel, sizeof(rel), "%s/%s", prefix, entry->d_name);
        }
        char full[8192];
        snprintf(full, sizeof(full), "%s/%s", root, rel);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (IS_DIR_MODE(st.st_mode)) {
            scan_dir(root, rel, out);
        } else if (IS_REG_MODE(st.st_mode)) {
            append_resource(out, rel, full);
        }
    }
    closedir(dir);
#endif
}

int pebble_resource_scan(const char *root, PebbleResourceList *out) {
    if (root == NULL || out == NULL) return -1;
    memset(out, 0, sizeof(*out));
    scan_dir(root, "", out);
    qsort(out->items, out->count, sizeof(PebbleResource), compare_resources);
    return 0;
}

void pebble_resource_list_destroy(PebbleResourceList *list) {
    if (list == NULL) return;
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].relative_path);
        free(list->items[i].type);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

unsigned long long pebble_file_hash(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    unsigned long long hash = 14695981039346656037ULL;
    int c;
    while ((c = fgetc(f)) != EOF) {
        hash ^= (unsigned char)c;
        hash *= 1099511628211ULL;
    }
    fclose(f);
    return hash;
}
