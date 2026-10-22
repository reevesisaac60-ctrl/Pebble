#ifndef PEBBLE_C_MODULE_H
#define PEBBLE_C_MODULE_H

#include "pebble_c/ast.h"
#include "pebble_c/source.h"

#include <stddef.h>

typedef struct {
    PebbleCDiagnosticList diagnostics;
    PebbleCNode **functions;
    size_t function_count;
    size_t function_capacity;
    char **paths;
    size_t path_count;
    size_t path_capacity;
} PebbleCModuleResult;

void pebble_c_module_result_destroy(PebbleCModuleResult *result);

int pebble_c_module_result_ok(const PebbleCModuleResult *result);

PebbleCModuleResult pebble_c_resolve_modules(
    PebbleCNode *program,
    const char *base_directory,
    const char *const *search_paths,
    size_t search_path_count
);

#endif
