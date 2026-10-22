#include "pebble_c/lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static PebbleCTokenList lex(const char *text) {
    PebbleCSource source = {"test.peb", text, strlen(text)};
    return pebble_c_lex(source);
}

static void test_keywords_and_literals(void) {
    PebbleCTokenList t = lex("fn main() { let x = 42; var ok = true; }");
    require(t.diagnostics.count == 0, "valid source has no diagnostics");
    require(t.items[0].kind == PEBBLE_C_TOKEN_FN, "fn keyword");
    require(t.items[1].kind == PEBBLE_C_TOKEN_IDENTIFIER, "identifier");
    require(t.items[5].kind == PEBBLE_C_TOKEN_LET, "let keyword");
    require(t.items[8].kind == PEBBLE_C_TOKEN_INTEGER, "integer literal");
    require(t.items[t.count - 1].kind == PEBBLE_C_TOKEN_EOF, "EOF token");
    pebble_c_tokens_destroy(&t);
}

static void test_operators_and_comments(void) {
    PebbleCTokenList t = lex("a == b && c != d // comment\nx -> y");
    require(t.diagnostics.count == 0, "operators and comments lex");
    int found_arrow = 0;
    for (size_t i = 0; i < t.count; ++i) {
        if (t.items[i].kind == PEBBLE_C_TOKEN_ARROW) {
            found_arrow = 1;
            require(t.items[i].location.line == 2, "arrow line number");
        }
    }
    require(found_arrow, "arrow operator recognised");
    pebble_c_tokens_destroy(&t);
}

static void test_locations(void) {
    PebbleCTokenList t = lex("let first = 1;\n    let second = 2;");
    int found = 0;
    for (size_t i = 0; i < t.count; ++i) {
        if (strcmp(t.items[i].lexeme, "second") == 0) {
            found = 1;
            require(t.items[i].location.line == 2, "line location");
            require(t.items[i].location.column == 9, "column location");
        }
    }
    require(found, "second identifier exists");
    pebble_c_tokens_destroy(&t);
}

static void test_errors(void) {
    PebbleCTokenList t = lex("let x = @;");
    require(t.diagnostics.count == 1, "invalid character diagnosed");
    require(t.diagnostics.items[0].location.column == 9, "error column");
    pebble_c_tokens_destroy(&t);

    t = lex("\"unfinished");
    require(t.diagnostics.count == 1, "unterminated string diagnosed");
    require(strcmp(t.diagnostics.items[0].message,
                   "Unterminated string literal.") == 0,
            "descriptive string error");
    pebble_c_tokens_destroy(&t);
}

int main(void) {
    test_keywords_and_literals();
    test_operators_and_comments();
    test_locations();
    test_errors();
    puts("All C lexer tests passed.");
    return EXIT_SUCCESS;
}
