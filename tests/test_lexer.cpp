#include "pebble/lexer.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void testKeywordsAndIdentifiers() {
    const pebble::Source source{
        "keywords.peb",
        "fn main() { let answer = 42; var score = true; }"
    };

    const pebble::LexResult result = pebble::Lexer{}.scan(source);

    require(result.ok(), "valid keywords should lex without errors");
    require(result.tokens[0].kind == pebble::TokenKind::Fn,
            "'fn' should be a keyword");
    require(result.tokens[1].kind == pebble::TokenKind::Identifier,
            "'main' should be an identifier");
    require(result.tokens[5].kind == pebble::TokenKind::Let,
            "'let' should be a keyword");
    require(result.tokens[7].kind == pebble::TokenKind::Equal,
            "'=' should be an assignment token");
    require(result.tokens[8].kind == pebble::TokenKind::Integer,
            "integer literals should be recognised");
    require(result.tokens.back().kind == pebble::TokenKind::EndOfFile,
            "the token stream should end with EOF");
}

void testNumbersAndStrings() {
    const pebble::Source source{
        "values.peb",
        "123 45.67 \"hello world\""
    };

    const pebble::LexResult result = pebble::Lexer{}.scan(source);

    require(result.ok(), "valid literals should lex without errors");
    require(result.tokens[0].kind == pebble::TokenKind::Integer,
            "integer literal expected");
    require(result.tokens[1].kind == pebble::TokenKind::Float,
            "decimal literal expected");
    require(result.tokens[2].kind == pebble::TokenKind::String,
            "string literal expected");
    require(result.tokens[2].lexeme == "\"hello world\"",
            "string token should preserve its source spelling");
}

void testOperatorsAndComments() {
    const pebble::Source source{
        "operators.peb",
        "a == b && c != d // ignored\nx -> y"
    };

    const pebble::LexResult result = pebble::Lexer{}.scan(source);

    require(result.ok(), "operators and comments should lex correctly");
    require(result.tokens[1].kind == pebble::TokenKind::EqualEqual,
            "'==' should be one token");
    require(result.tokens[3].kind == pebble::TokenKind::AndAnd,
            "'&&' should be one token");

    bool foundArrow = false;

    for (const pebble::Token& token : result.tokens) {
        if (token.kind == pebble::TokenKind::Arrow) {
            foundArrow = true;
            require(token.location.line == 2,
                    "arrow should have the correct line number");
        }
    }

    require(foundArrow, "'->' should be recognised");
}

void testSourceLocations() {
    const pebble::Source source{
        "locations.peb",
        "let first = 1;\n    let second = 2;"
    };

    const pebble::LexResult result = pebble::Lexer{}.scan(source);

    require(result.ok(), "source-location test input should be valid");

    bool foundSecond = false;

    for (const pebble::Token& token : result.tokens) {
        if (token.lexeme == "second") {
            foundSecond = true;
            require(token.location.line == 2,
                    "second identifier should be on line two");
            require(token.location.column == 9,
                    "second identifier should start at column nine");
        }
    }

    require(foundSecond, "second identifier should exist");
}

void testInvalidCharacter() {
    const pebble::Source source{"invalid.peb", "let x = @;"};
    const pebble::LexResult result = pebble::Lexer{}.scan(source);

    require(!result.ok(), "invalid characters should produce diagnostics");
    require(result.diagnostics[0].location.file == "invalid.peb",
            "diagnostic should include the source filename");
    require(result.diagnostics[0].location.line == 1,
            "diagnostic should include the line");
    require(result.diagnostics[0].location.column == 9,
            "diagnostic should include the column");
}

void testUnterminatedString() {
    const pebble::Source source{"string.peb", "\"unfinished"};
    const pebble::LexResult result = pebble::Lexer{}.scan(source);

    require(!result.ok(), "unterminated strings should produce diagnostics");
    require(result.diagnostics[0].message == "Unterminated string literal.",
            "unterminated string diagnostic should be descriptive");
}

}

int main() {
    testKeywordsAndIdentifiers();
    testNumbersAndStrings();
    testOperatorsAndComments();
    testSourceLocations();
    testInvalidCharacter();
    testUnterminatedString();

    std::cout << "All lexer tests passed.\n";
    return EXIT_SUCCESS;
}
