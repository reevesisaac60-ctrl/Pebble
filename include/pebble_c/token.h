#ifndef PEBBLE_C_TOKEN_H
#define PEBBLE_C_TOKEN_H

#include "pebble_c/source.h"

#include <stddef.h>

typedef enum {
    PEBBLE_C_TOKEN_EOF,
    PEBBLE_C_TOKEN_INVALID,
    PEBBLE_C_TOKEN_IDENTIFIER,
    PEBBLE_C_TOKEN_INTEGER,
    PEBBLE_C_TOKEN_FLOAT,
    PEBBLE_C_TOKEN_STRING,
    PEBBLE_C_TOKEN_FN,
    PEBBLE_C_TOKEN_LET,
    PEBBLE_C_TOKEN_VAR,
    PEBBLE_C_TOKEN_IMPORT,
    PEBBLE_C_TOKEN_PACKAGE,
    PEBBLE_C_TOKEN_EXPORT,
    PEBBLE_C_TOKEN_IF,
    PEBBLE_C_TOKEN_ELSE,
    PEBBLE_C_TOKEN_WHILE,
    PEBBLE_C_TOKEN_RETURN,
    PEBBLE_C_TOKEN_TRUE,
    PEBBLE_C_TOKEN_FALSE,
    PEBBLE_C_TOKEN_NULL,
    PEBBLE_C_TOKEN_LEFT_PAREN,
    PEBBLE_C_TOKEN_RIGHT_PAREN,
    PEBBLE_C_TOKEN_LEFT_BRACE,
    PEBBLE_C_TOKEN_RIGHT_BRACE,
    PEBBLE_C_TOKEN_COMMA,
    PEBBLE_C_TOKEN_COLON,
    PEBBLE_C_TOKEN_SEMICOLON,
    PEBBLE_C_TOKEN_DOT,
    PEBBLE_C_TOKEN_PLUS,
    PEBBLE_C_TOKEN_MINUS,
    PEBBLE_C_TOKEN_STAR,
    PEBBLE_C_TOKEN_SLASH,
    PEBBLE_C_TOKEN_PERCENT,
    PEBBLE_C_TOKEN_EQUAL,
    PEBBLE_C_TOKEN_EQUAL_EQUAL,
    PEBBLE_C_TOKEN_BANG,
    PEBBLE_C_TOKEN_BANG_EQUAL,
    PEBBLE_C_TOKEN_LESS,
    PEBBLE_C_TOKEN_LESS_EQUAL,
    PEBBLE_C_TOKEN_GREATER,
    PEBBLE_C_TOKEN_GREATER_EQUAL,
    PEBBLE_C_TOKEN_AND_AND,
    PEBBLE_C_TOKEN_OR_OR,
    PEBBLE_C_TOKEN_ARROW
} PebbleCTokenKind;

typedef struct {
    PebbleCTokenKind kind;
    char *lexeme;
    PebbleCSourceLocation location;
} PebbleCToken;

typedef struct {
    PebbleCToken *items;
    size_t count;
    size_t capacity;
    PebbleCDiagnosticList diagnostics;
} PebbleCTokenList;

const char *pebble_c_token_kind_name(PebbleCTokenKind kind);
void pebble_c_tokens_destroy(PebbleCTokenList *tokens);

#endif
