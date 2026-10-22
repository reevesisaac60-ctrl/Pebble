#ifndef PEBBLE_C_PACKAGE_H
#define PEBBLE_C_PACKAGE_H

#include <stddef.h>

typedef struct {
    const char *name;
    const char *const *parameters;
    size_t parameter_count;
    const char *return_type;
    const char *runtime_symbol;
} PebbleCPackageFunction;

typedef struct {
    const char *name;
    const PebbleCPackageFunction *functions;
    size_t function_count;
} PebbleCPackage;

const PebbleCPackage *pebble_c_builtin_packages(size_t *count);

const PebbleCPackage *pebble_c_find_builtin_package(const char *name);

const PebbleCPackageFunction *pebble_c_find_package_function(
    const PebbleCPackage *package,
    const char *name
);

#endif
