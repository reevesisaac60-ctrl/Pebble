#ifndef PEBBLE_C_PARSER_H
#define PEBBLE_C_PARSER_H

#include "pebble_c/ast.h"
#include "pebble_c/token.h"

typedef struct {
    PebbleCNode *root;
    PebbleCDiagnosticList diagnostics;
} PebbleCParseResult;

PebbleCParseResult pebble_c_parse(const PebbleCTokenList *tokens);
int pebble_c_parse_result_ok(const PebbleCParseResult *result);
void pebble_c_parse_result_destroy(PebbleCParseResult *result);

#endif
