#pragma once

#include "pebble/source.hpp"
#include "pebble/token.hpp"

#include <string>
#include <vector>

namespace pebble {

struct LexResult {
    std::vector<Token> tokens;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return diagnostics.empty();
    }
};

class Lexer {
public:
    [[nodiscard]] LexResult scan(const Source& source) const;

private:
    const Source* source_ = nullptr;
    std::size_t start_ = 0;
    std::size_t current_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
    std::size_t startLine_ = 1;
    std::size_t startColumn_ = 1;

    std::vector<Token> tokens_;
    std::vector<Diagnostic> diagnostics_;

    [[nodiscard]] bool atEnd() const noexcept;
    [[nodiscard]] char peek() const noexcept;
    [[nodiscard]] char peekNext() const noexcept;
    char advance() noexcept;
    bool match(char expected) noexcept;

    void scanToken();
    void scanIdentifier();
    void scanNumber();
    void scanString();
    void skipLineComment();
    void addToken(TokenKind kind);
    void report(std::string message);
};

[[nodiscard]] const std::vector<TokenKind>& languageKeywordTokens() noexcept;

}
