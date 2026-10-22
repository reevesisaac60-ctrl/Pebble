#ifndef PEBBLE_C_SOURCE_H
#define PEBBLE_C_SOURCE_H

#include <stddef.h>

typedef struct {
    const char *file;
    size_t line;
    size_t column;
} PebbleCSourceLocation;

typedef struct {
    PebbleCSourceLocation location;
    char *message;
} PebbleCDiagnostic;

typedef struct {
    PebbleCDiagnostic *items;
    size_t count;
    size_t capacity;
} PebbleCDiagnosticList;

void pebble_c_diagnostics_destroy(PebbleCDiagnosticList *diagnostics);

int pebble_c_add_diagnostic(
    PebbleCDiagnosticList *diagnostics,
    PebbleCSourceLocation location,
    const char *message
);

#endif
