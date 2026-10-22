#ifndef PEBBLE_C_TYPE_CHECKER_H
#define PEBBLE_C_TYPE_CHECKER_H

#include "pebble_c/ast.h"

typedef struct {
    PebbleCDiagnosticList diagnostics;
} PebbleCTypeCheckResult;

PebbleCTypeCheckResult pebble_c_type_check(const PebbleCNode *program);
int pebble_c_type_check_result_ok(const PebbleCTypeCheckResult *result);
void pebble_c_type_check_result_destroy(PebbleCTypeCheckResult *result);

#endif
