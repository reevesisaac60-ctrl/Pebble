#ifndef PEBBLE_C_LEXER_H
#define PEBBLE_C_LEXER_H

#include "pebble_c/token.h"

typedef struct {
    const char *name;
    const char *text;
    size_t length;
} PebbleCSource;

PebbleCTokenList pebble_c_lex(PebbleCSource source);

#endif
