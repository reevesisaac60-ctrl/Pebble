#include "pebble_c/token.h"

#include <stdlib.h>
#include <string.h>

const char *pebble_c_token_kind_name(PebbleCTokenKind kind) {
    static const char *const names[] = {
        "EOF", "Invalid", "Identifier", "Integer", "Float", "String",
        "Fn", "Let", "Var", "Import", "Package", "Export", "If", "Else",
        "While",
        "Return", "True",
        "False", "Null", "(", ")", "{", "}", ",", ":", ";", ".",
        "+", "-", "*", "/", "%", "=", "==", "!", "!=", "<", "<=",
        ">", ">=", "&&", "||", "->"
    };
    const size_t count = sizeof(names) / sizeof(names[0]);
    return (size_t)kind < count ? names[kind] : "Unknown";
}

void pebble_c_diagnostics_destroy(PebbleCDiagnosticList *diagnostics) {
    if (diagnostics == NULL) return;
    for (size_t i = 0; i < diagnostics->count; ++i)
        free(diagnostics->items[i].message);
    free(diagnostics->items);
    *diagnostics = (PebbleCDiagnosticList){0};
}

int pebble_c_add_diagnostic(PebbleCDiagnosticList *diagnostics,
                             PebbleCSourceLocation location,
                             const char *message) {
    char *copy;
    size_t capacity;

    if (diagnostics == NULL || message == NULL) return 0;

    if (diagnostics->count == diagnostics->capacity) {
        capacity = diagnostics->capacity == 0 ? 8 : diagnostics->capacity * 2;
        if (capacity < diagnostics->capacity ||
            capacity > (size_t)-1 / sizeof(*diagnostics->items)) return 0;
        PebbleCDiagnostic *items =
            realloc(diagnostics->items, capacity * sizeof(*items));
        if (items == NULL) return 0;
        diagnostics->items = items;
        diagnostics->capacity = capacity;
    }

    copy = malloc(strlen(message) + 1);
    if (copy == NULL) return 0;
    (void)strcpy(copy, message);
    diagnostics->items[diagnostics->count++] = (PebbleCDiagnostic){location, copy};
    return 1;
}

void pebble_c_tokens_destroy(PebbleCTokenList *tokens) {
    if (tokens == NULL) return;
    for (size_t i = 0; i < tokens->count; ++i)
        free(tokens->items[i].lexeme);
    free(tokens->items);
    pebble_c_diagnostics_destroy(&tokens->diagnostics);
    *tokens = (PebbleCTokenList){0};
}
