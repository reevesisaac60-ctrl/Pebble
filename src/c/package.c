#include "pebble_c/package.h"

#include <string.h>

static const char *const pebble_c_math_randint_parameters[] = {"Int", "Int"};
static const char *const pebble_c_math_abs_parameters[] = {"Int"};
static const char *const pebble_c_math_min_parameters[] = {"Int", "Int"};
static const char *const pebble_c_math_max_parameters[] = {"Int", "Int"};
static const char *const pebble_c_math_randomseed_parameters[] = {"Int"};
static const char *const pebble_c_math_float_parameters[] = {"Float"};

static const PebbleCPackageFunction pebble_c_math_functions[] = {
    {
        "randint",
        pebble_c_math_randint_parameters,
        sizeof(pebble_c_math_randint_parameters) /
            sizeof(pebble_c_math_randint_parameters[0]),
        "Int",
        "pebble_math_randint"
    },
    {
        "abs",
        pebble_c_math_abs_parameters,
        sizeof(pebble_c_math_abs_parameters) /
            sizeof(pebble_c_math_abs_parameters[0]),
        "Int",
        "pebble_math_abs"
    },
    {
        "min",
        pebble_c_math_min_parameters,
        sizeof(pebble_c_math_min_parameters) /
            sizeof(pebble_c_math_min_parameters[0]),
        "Int",
        "pebble_math_min"
    },
    {
        "max",
        pebble_c_math_max_parameters,
        sizeof(pebble_c_math_max_parameters) /
            sizeof(pebble_c_math_max_parameters[0]),
        "Int",
        "pebble_math_max"
    },
    {
        "randomseed",
        pebble_c_math_randomseed_parameters,
        sizeof(pebble_c_math_randomseed_parameters) /
            sizeof(pebble_c_math_randomseed_parameters[0]),
        "Void",
        "pebble_math_randomseed"
    },
    {
        "random",
        NULL,
        0U,
        "Float",
        "pebble_math_random"
    },
    {
        "sqrt",
        pebble_c_math_float_parameters,
        sizeof(pebble_c_math_float_parameters) /
            sizeof(pebble_c_math_float_parameters[0]),
        "Float",
        "pebble_math_sqrt"
    },
    {
        "floor",
        pebble_c_math_float_parameters,
        sizeof(pebble_c_math_float_parameters) /
            sizeof(pebble_c_math_float_parameters[0]),
        "Float",
        "pebble_math_floor"
    },
    {
        "ceil",
        pebble_c_math_float_parameters,
        sizeof(pebble_c_math_float_parameters) /
            sizeof(pebble_c_math_float_parameters[0]),
        "Float",
        "pebble_math_ceil"
    },
    {
        "round",
        pebble_c_math_float_parameters,
        sizeof(pebble_c_math_float_parameters) /
            sizeof(pebble_c_math_float_parameters[0]),
        "Float",
        "pebble_math_round"
    },
    {
        "pi",
        NULL,
        0U,
        "Float",
        "pebble_math_pi"
    }
};

static const char *const pebble_c_string_parameters[] = {"String"};
static const char *const pebble_c_string_int_parameters[] = {"String", "Int"};
static const char *const pebble_c_string_string_parameters[] = {
    "String", "String"};
static const char *const pebble_c_string_substring_parameters[] = {
    "String", "Int", "Int"};
static const char *const pebble_c_string_replace_parameters[] = {
    "String", "String", "String"};

static const PebbleCPackageFunction pebble_c_string_functions[] = {
    {
        "length",
        pebble_c_string_parameters,
        sizeof(pebble_c_string_parameters) /
            sizeof(pebble_c_string_parameters[0]),
        "Int",
        "pebble_string_length"
    },
    {
        "substring",
        pebble_c_string_substring_parameters,
        sizeof(pebble_c_string_substring_parameters) /
            sizeof(pebble_c_string_substring_parameters[0]),
        "String",
        "pebble_string_substring"
    },
    {
        "left",
        pebble_c_string_int_parameters,
        sizeof(pebble_c_string_int_parameters) /
            sizeof(pebble_c_string_int_parameters[0]),
        "String",
        "pebble_string_left"
    },
    {
        "right",
        pebble_c_string_int_parameters,
        sizeof(pebble_c_string_int_parameters) /
            sizeof(pebble_c_string_int_parameters[0]),
        "String",
        "pebble_string_right"
    },
    {
        "upper",
        pebble_c_string_parameters,
        sizeof(pebble_c_string_parameters) /
            sizeof(pebble_c_string_parameters[0]),
        "String",
        "pebble_string_upper"
    },
    {
        "lower",
        pebble_c_string_parameters,
        sizeof(pebble_c_string_parameters) /
            sizeof(pebble_c_string_parameters[0]),
        "String",
        "pebble_string_lower"
    },
    {
        "trim",
        pebble_c_string_parameters,
        sizeof(pebble_c_string_parameters) /
            sizeof(pebble_c_string_parameters[0]),
        "String",
        "pebble_string_trim"
    },
    {
        "contains",
        pebble_c_string_string_parameters,
        sizeof(pebble_c_string_string_parameters) /
            sizeof(pebble_c_string_string_parameters[0]),
        "Bool",
        "pebble_string_contains"
    },
    {
        "startswith",
        pebble_c_string_string_parameters,
        sizeof(pebble_c_string_string_parameters) /
            sizeof(pebble_c_string_string_parameters[0]),
        "Bool",
        "pebble_string_startswith"
    },
    {
        "endswith",
        pebble_c_string_string_parameters,
        sizeof(pebble_c_string_string_parameters) /
            sizeof(pebble_c_string_string_parameters[0]),
        "Bool",
        "pebble_string_endswith"
    },
    {
        "indexof",
        pebble_c_string_string_parameters,
        sizeof(pebble_c_string_string_parameters) /
            sizeof(pebble_c_string_string_parameters[0]),
        "Int",
        "pebble_string_indexof"
    },
    {
        "replace",
        pebble_c_string_replace_parameters,
        sizeof(pebble_c_string_replace_parameters) /
            sizeof(pebble_c_string_replace_parameters[0]),
        "String",
        "pebble_string_replace"
    },
    {
        "repeat",
        pebble_c_string_int_parameters,
        sizeof(pebble_c_string_int_parameters) /
            sizeof(pebble_c_string_int_parameters[0]),
        "String",
        "pebble_string_repeat"
    },
    {
        "toint",
        pebble_c_string_parameters,
        sizeof(pebble_c_string_parameters) /
            sizeof(pebble_c_string_parameters[0]),
        "Int",
        "pebble_string_toint"
    },
    {
        "tofloat",
        pebble_c_string_parameters,
        sizeof(pebble_c_string_parameters) /
            sizeof(pebble_c_string_parameters[0]),
        "Float",
        "pebble_string_tofloat"
    }
};

static const char *const pebble_c_time_float_parameters[] = {"Float"};

static const PebbleCPackageFunction pebble_c_time_functions[] = {
    {
        "now",
        NULL,
        0U,
        "Float",
        "pebble_time_now"
    },
    {
        "ticks",
        NULL,
        0U,
        "Int",
        "pebble_time_ticks"
    },
    {
        "delta",
        NULL,
        0U,
        "Float",
        "pebble_time_delta"
    },
    {
        "sleep",
        pebble_c_time_float_parameters,
        sizeof(pebble_c_time_float_parameters) /
            sizeof(pebble_c_time_float_parameters[0]),
        "Void",
        "pebble_time_sleep"
    },
    {
        "unix",
        NULL,
        0U,
        "Int",
        "pebble_time_unix"
    }
};

static const char *const pebble_c_filesystem_string_parameters[] = {"String"};
static const char *const pebble_c_filesystem_two_string_parameters[] = {
    "String",
    "String"
};

static const PebbleCPackageFunction pebble_c_filesystem_functions[] = {
    {
        "read",
        pebble_c_filesystem_string_parameters,
        sizeof(pebble_c_filesystem_string_parameters) /
            sizeof(pebble_c_filesystem_string_parameters[0]),
        "String",
        "pebble_filesystem_read"
    },
    {
        "write",
        pebble_c_filesystem_two_string_parameters,
        sizeof(pebble_c_filesystem_two_string_parameters) /
            sizeof(pebble_c_filesystem_two_string_parameters[0]),
        "Void",
        "pebble_filesystem_write"
    },
    {
        "exists",
        pebble_c_filesystem_string_parameters,
        sizeof(pebble_c_filesystem_string_parameters) /
            sizeof(pebble_c_filesystem_string_parameters[0]),
        "Bool",
        "pebble_filesystem_exists"
    },
    {
        "list",
        pebble_c_filesystem_string_parameters,
        sizeof(pebble_c_filesystem_string_parameters) /
            sizeof(pebble_c_filesystem_string_parameters[0]),
        "String",
        "pebble_filesystem_list"
    },
    {
        "mkdir",
        pebble_c_filesystem_string_parameters,
        sizeof(pebble_c_filesystem_string_parameters) /
            sizeof(pebble_c_filesystem_string_parameters[0]),
        "Void",
        "pebble_filesystem_mkdir"
    }
};

static const char *const pebble_c_log_string_parameters[] = {"String"};

static const PebbleCPackageFunction pebble_c_log_functions[] = {
    {
        "Print",
        pebble_c_log_string_parameters,
        sizeof(pebble_c_log_string_parameters) /
            sizeof(pebble_c_log_string_parameters[0]),
        "Void",
        "pebble_log_print"
    },
    {
        "Warning",
        pebble_c_log_string_parameters,
        sizeof(pebble_c_log_string_parameters) /
            sizeof(pebble_c_log_string_parameters[0]),
        "Void",
        "pebble_log_warning"
    },
    {
        "Error",
        pebble_c_log_string_parameters,
        sizeof(pebble_c_log_string_parameters) /
            sizeof(pebble_c_log_string_parameters[0]),
        "Void",
        "pebble_log_error"
    }
};

static const PebbleCPackage pebble_c_builtin_package_table[] = {
    {
        "Log",
        pebble_c_log_functions,
        sizeof(pebble_c_log_functions) / sizeof(pebble_c_log_functions[0])
    },
    {
        "math",
        pebble_c_math_functions,
        sizeof(pebble_c_math_functions) / sizeof(pebble_c_math_functions[0])
    },
    {
        "string",
        pebble_c_string_functions,
        sizeof(pebble_c_string_functions) / sizeof(pebble_c_string_functions[0])
    },
    {
        "time",
        pebble_c_time_functions,
        sizeof(pebble_c_time_functions) / sizeof(pebble_c_time_functions[0])
    },
    {
        "filesystem",
        pebble_c_filesystem_functions,
        sizeof(pebble_c_filesystem_functions) /
            sizeof(pebble_c_filesystem_functions[0])
    }
};

const PebbleCPackage *pebble_c_builtin_packages(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(pebble_c_builtin_package_table) /
            sizeof(pebble_c_builtin_package_table[0]);
    }

    return pebble_c_builtin_package_table;
}

const PebbleCPackage *pebble_c_find_builtin_package(const char *name)
{
    size_t count = 0U;
    const PebbleCPackage *packages = pebble_c_builtin_packages(&count);
    size_t index;

    if (name == NULL) {
        return NULL;
    }

    for (index = 0U; index < count; ++index) {
        if (strcmp(packages[index].name, name) == 0) {
            return &packages[index];
        }
    }

    return NULL;
}

const PebbleCPackageFunction *pebble_c_find_package_function(
    const PebbleCPackage *package,
    const char *name
)
{
    size_t index;

    if (package == NULL || name == NULL) {
        return NULL;
    }

    for (index = 0U; index < package->function_count; ++index) {
        if (strcmp(package->functions[index].name, name) == 0) {
            return &package->functions[index];
        }
    }

    return NULL;
}
