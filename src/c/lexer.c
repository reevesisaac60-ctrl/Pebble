#include "pebble_c/lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    PebbleCSource source;
    PebbleCTokenList result;
    size_t start;
    size_t current;
    size_t line;
    size_t column;
    size_t start_line;
    size_t start_column;
} Scanner;

static char peek(const Scanner *s) {
    return s->current < s->source.length ? s->source.text[s->current] : '\0';
}

static char peek_next(const Scanner *s) {
    return s->current + 1 < s->source.length
        ? s->source.text[s->current + 1] : '\0';
}

static char advance(Scanner *s) {
    char c = peek(s);
    if (s->current < s->source.length) {
        ++s->current;
        if (c == '\n') {
            ++s->line;
            s->column = 1;
        } else {
            ++s->column;
        }
    }
    return c;
}

static int match(Scanner *s, char expected) {
    if (peek(s) != expected) return 0;
    (void)advance(s);
    return 1;
}

static char *copy_range(const char *text, size_t begin, size_t end) {
    size_t length = end - begin;
    char *copy = malloc(length + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, text + begin, length);
    copy[length] = '\0';
    return copy;
}

static int grow_array(void **items, size_t *capacity,
                      size_t count, size_t item_size) {
    if (count < *capacity) return 1;
    size_t next = *capacity == 0 ? 16 : *capacity * 2;
    if (next < *capacity || next > (size_t)-1 / item_size) return 0;
    void *memory = realloc(*items, next * item_size);
    if (memory == NULL) return 0;
    *items = memory;
    *capacity = next;
    return 1;
}

static void report(Scanner *s, const char *message) {
    PebbleCDiagnosticList *list = &s->result.diagnostics;
    if (!grow_array((void **)&list->items, &list->capacity,
                    list->count, sizeof(*list->items))) return;
    char *copy = copy_range(message, 0, strlen(message));
    if (copy == NULL) return;
    list->items[list->count++] = (PebbleCDiagnostic){
        {s->source.name, s->start_line, s->start_column}, copy
    };
}

static void add_token(Scanner *s, PebbleCTokenKind kind) {
    PebbleCTokenList *list = &s->result;
    if (!grow_array((void **)&list->items, &list->capacity,
                    list->count, sizeof(*list->items))) {
        report(s, "Out of memory while creating token.");
        return;
    }
    char *lexeme = copy_range(s->source.text, s->start, s->current);
    if (lexeme == NULL) {
        report(s, "Out of memory while copying token.");
        return;
    }
    list->items[list->count++] = (PebbleCToken){
        kind, lexeme, {s->source.name, s->start_line, s->start_column}
    };
}

static int identifier_start(char c) {
    return c == '_' || isalpha((unsigned char)c);
}

static int identifier_continue(char c) {
    return identifier_start(c) || isdigit((unsigned char)c);
}

static void identifier(Scanner *s) {
    while (identifier_continue(peek(s))) (void)advance(s);
    char *word = copy_range(s->source.text, s->start, s->current);
    if (word == NULL) {
        report(s, "Out of memory while reading identifier.");
        return;
    }

    static const struct { const char *word; PebbleCTokenKind kind; } keywords[] = {
        {"fn", PEBBLE_C_TOKEN_FN}, {"let", PEBBLE_C_TOKEN_LET},
        {"var", PEBBLE_C_TOKEN_VAR}, {"import", PEBBLE_C_TOKEN_IMPORT},
        {"package", PEBBLE_C_TOKEN_PACKAGE},
        {"export", PEBBLE_C_TOKEN_EXPORT},
        {"if", PEBBLE_C_TOKEN_IF},
        {"else", PEBBLE_C_TOKEN_ELSE}, {"while", PEBBLE_C_TOKEN_WHILE},
        {"return", PEBBLE_C_TOKEN_RETURN}, {"true", PEBBLE_C_TOKEN_TRUE},
        {"false", PEBBLE_C_TOKEN_FALSE}, {"null", PEBBLE_C_TOKEN_NULL}
    };
    PebbleCTokenKind kind = PEBBLE_C_TOKEN_IDENTIFIER;
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
        if (strcmp(word, keywords[i].word) == 0) {
            kind = keywords[i].kind;
            break;
        }
    }
    free(word);
    add_token(s, kind);
}

static void number(Scanner *s) {
    while (isdigit((unsigned char)peek(s))) (void)advance(s);
    PebbleCTokenKind kind = PEBBLE_C_TOKEN_INTEGER;
    if (peek(s) == '.' && isdigit((unsigned char)peek_next(s))) {
        (void)advance(s);
        while (isdigit((unsigned char)peek(s))) (void)advance(s);
        kind = PEBBLE_C_TOKEN_FLOAT;
    }
    add_token(s, kind);
}

static void string_literal(Scanner *s) {
    while (peek(s) != '"' && s->current < s->source.length) {
        if (peek(s) == '\\' && peek_next(s) != '\0') {
            (void)advance(s);
            (void)advance(s);
        } else {
            (void)advance(s);
        }
    }
    if (s->current == s->source.length) {
        report(s, "Unterminated string literal.");
        add_token(s, PEBBLE_C_TOKEN_INVALID);
        return;
    }
    (void)advance(s);
    add_token(s, PEBBLE_C_TOKEN_STRING);
}

static void scan_token(Scanner *s) {
    char c = advance(s);
    switch (c) {
    case ' ': case '\r': case '\t': return;
    case '\n': return;
    case '(': add_token(s, PEBBLE_C_TOKEN_LEFT_PAREN); return;
    case ')': add_token(s, PEBBLE_C_TOKEN_RIGHT_PAREN); return;
    case '{': add_token(s, PEBBLE_C_TOKEN_LEFT_BRACE); return;
    case '}': add_token(s, PEBBLE_C_TOKEN_RIGHT_BRACE); return;
    case ',': add_token(s, PEBBLE_C_TOKEN_COMMA); return;
    case ':': add_token(s, PEBBLE_C_TOKEN_COLON); return;
    case ';': add_token(s, PEBBLE_C_TOKEN_SEMICOLON); return;
    case '.': add_token(s, PEBBLE_C_TOKEN_DOT); return;
    case '+': add_token(s, PEBBLE_C_TOKEN_PLUS); return;
    case '*': add_token(s, PEBBLE_C_TOKEN_STAR); return;
    case '%': add_token(s, PEBBLE_C_TOKEN_PERCENT); return;
    case '-': add_token(s, match(s, '>') ? PEBBLE_C_TOKEN_ARROW : PEBBLE_C_TOKEN_MINUS); return;
    case '!': add_token(s, match(s, '=') ? PEBBLE_C_TOKEN_BANG_EQUAL : PEBBLE_C_TOKEN_BANG); return;
    case '=': add_token(s, match(s, '=') ? PEBBLE_C_TOKEN_EQUAL_EQUAL : PEBBLE_C_TOKEN_EQUAL); return;
    case '<': add_token(s, match(s, '=') ? PEBBLE_C_TOKEN_LESS_EQUAL : PEBBLE_C_TOKEN_LESS); return;
    case '>': add_token(s, match(s, '=') ? PEBBLE_C_TOKEN_GREATER_EQUAL : PEBBLE_C_TOKEN_GREATER); return;
    case '&':
        if (match(s, '&')) add_token(s, PEBBLE_C_TOKEN_AND_AND);
        else { report(s, "Unexpected '&'; use '&&'."); add_token(s, PEBBLE_C_TOKEN_INVALID); }
        return;
    case '|':
        if (match(s, '|')) add_token(s, PEBBLE_C_TOKEN_OR_OR);
        else { report(s, "Unexpected '|'; use '||'."); add_token(s, PEBBLE_C_TOKEN_INVALID); }
        return;
    case '/':
        if (match(s, '/')) {
            while (peek(s) != '\n' && s->current < s->source.length)
                (void)advance(s);
        } else add_token(s, PEBBLE_C_TOKEN_SLASH);
        return;
    case '"': string_literal(s); return;
    default: break;
    }

    if (isdigit((unsigned char)c)) { number(s); return; }
    if (identifier_start(c)) { identifier(s); return; }

    report(s, "Unexpected character.");
    add_token(s, PEBBLE_C_TOKEN_INVALID);
}

PebbleCTokenList pebble_c_lex(PebbleCSource source) {
    Scanner s = {0};
    s.source = source;
    s.line = 1;
    s.column = 1;

    if (source.text == NULL || source.name == NULL) {
        s.source.name = source.name != NULL ? source.name : "<input>";
        s.start_line = 1;
        s.start_column = 1;
        report(&s, "Source text and filename must not be NULL.");
        return s.result;
    }

    while (s.current < source.length) {
        s.start = s.current;
        s.start_line = s.line;
        s.start_column = s.column;
        scan_token(&s);
    }

    s.start = s.current;
    s.start_line = s.line;
    s.start_column = s.column;
    add_token(&s, PEBBLE_C_TOKEN_EOF);
    return s.result;
}
