#pragma once

#include "pebble/ast.hpp"
#include "pebble/lexer.hpp"

#include <vector>

namespace pebble {

struct ParseResult {
    Ast root;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return diagnostics.empty() && root != nullptr;
    }
};

class Parser {
public:
    [[nodiscard]] ParseResult parse(const LexResult& lexed) const;
    [[nodiscard]] ParseResult parse(const std::vector<Token>& tokens) const;

private:
    const std::vector<Token>* tokens_ = nullptr;
    std::size_t current_ = 0;
    std::vector<Diagnostic> diagnostics_;

    [[nodiscard]] bool atEnd() const noexcept;
    [[nodiscard]] const Token& peek() const noexcept;
    [[nodiscard]] const Token& previous() const noexcept;
    bool check(TokenKind kind) const noexcept;
    bool match(TokenKind kind) noexcept;
    const Token* consume(TokenKind kind, const char* message);

    [[nodiscard]] bool atLineBreak() const noexcept;
    [[nodiscard]] bool continuesLine() const noexcept;
    [[nodiscard]] bool atBinaryOperator() const noexcept;
    [[nodiscard]] bool continuesMember() const noexcept;
    bool endStatement(const char* message);

    Ast declaration();
    Ast functionDeclaration();
    Ast importDeclaration();
    Ast parameter();
    Ast block();
    Ast statement();
    Ast ifStatement();
    Ast whileStatement();
    Ast variableDeclaration();
    Ast returnStatement();

    Ast expression();
    Ast assignment();
    Ast logicalOr();
    Ast logicalAnd();
    Ast equality();
    Ast comparison();
    Ast term();
    Ast factor();
    Ast unary();
    Ast call();
    Ast primary();

    Ast binary(Ast left, const Token& op, Ast right);
    Ast typeAnnotation();
    void report(const Token& token, std::string message);
    void synchronize();
};

}
