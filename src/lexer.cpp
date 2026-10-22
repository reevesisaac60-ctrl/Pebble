#include "pebble/lexer.hpp"

#include <utility>

namespace pebble {

const std::vector<TokenKind>& languageKeywordTokens() noexcept {
    static const std::vector<TokenKind> tokens = {
        TokenKind::Fn, TokenKind::Let, TokenKind::Var, TokenKind::Import,
        TokenKind::Package, TokenKind::Export, TokenKind::If, TokenKind::Else,
        TokenKind::While, TokenKind::Return, TokenKind::True, TokenKind::False,
        TokenKind::Null
    };
    return tokens;
}

std::string_view tokenKindName(TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::EndOfFile: return "end of file";
    case TokenKind::Invalid: return "invalid token";
    case TokenKind::Identifier: return "identifier";
    case TokenKind::Integer: return "integer";
    case TokenKind::Float: return "float";
    case TokenKind::String: return "string";
    case TokenKind::Fn: return "fn";
    case TokenKind::Let: return "let";
    case TokenKind::Var: return "var";
    case TokenKind::Import: return "import";
    case TokenKind::Package: return "package";
    case TokenKind::Export: return "export";
    case TokenKind::If: return "if";
    case TokenKind::Else: return "else";
    case TokenKind::While: return "while";
    case TokenKind::Return: return "return";
    case TokenKind::True: return "true";
    case TokenKind::False: return "false";
    case TokenKind::Null: return "null";
    case TokenKind::LeftParen: return "(";
    case TokenKind::RightParen: return ")";
    case TokenKind::LeftBrace: return "{";
    case TokenKind::RightBrace: return "}";
    case TokenKind::Comma: return ",";
    case TokenKind::Colon: return ":";
    case TokenKind::Semicolon: return ";";
    case TokenKind::Dot: return ".";
    case TokenKind::Plus: return "+";
    case TokenKind::Minus: return "-";
    case TokenKind::Star: return "*";
    case TokenKind::Slash: return "/";
    case TokenKind::Percent: return "%";
    case TokenKind::Equal: return "=";
    case TokenKind::EqualEqual: return "==";
    case TokenKind::Bang: return "!";
    case TokenKind::BangEqual: return "!=";
    case TokenKind::Less: return "<";
    case TokenKind::LessEqual: return "<=";
    case TokenKind::Greater: return ">";
    case TokenKind::GreaterEqual: return ">=";
    case TokenKind::AndAnd: return "&&";
    case TokenKind::OrOr: return "||";
    case TokenKind::Arrow: return "->";
    }

    return "unknown";
}

LexResult Lexer::scan(const Source& source) const {
    Lexer lexer;
    lexer.source_ = &source;

    while (!lexer.atEnd()) {
        lexer.start_ = lexer.current_;
        lexer.startLine_ = lexer.line_;
        lexer.startColumn_ = lexer.column_;
        lexer.scanToken();
    }

    lexer.tokens_.push_back({
        TokenKind::EndOfFile,
        "",
        {source.name, lexer.line_, lexer.column_}
    });

    return {
        std::move(lexer.tokens_),
        std::move(lexer.diagnostics_)
    };
}

bool Lexer::atEnd() const noexcept {
    return current_ >= source_->text.size();
}

char Lexer::peek() const noexcept {
    return atEnd() ? '\0' : source_->text[current_];
}

char Lexer::peekNext() const noexcept {
    return current_ + 1 >= source_->text.size()
        ? '\0'
        : source_->text[current_ + 1];
}

char Lexer::advance() noexcept {
    const char character = source_->text[current_++];

    if (character == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }

    return character;
}

bool Lexer::match(char expected) noexcept {
    if (atEnd() || peek() != expected) {
        return false;
    }

    static_cast<void>(advance());
    return true;
}

void Lexer::addToken(TokenKind kind) {
    tokens_.push_back({
        kind,
        source_->text.substr(start_, current_ - start_),
        {source_->name, startLine_, startColumn_}
    });
}

void Lexer::report(std::string message) {
    diagnostics_.push_back({
        {source_->name, startLine_, startColumn_},
        std::move(message)
    });
}

void Lexer::skipLineComment() {
    while (!atEnd() && peek() != '\n') {
        static_cast<void>(advance());
    }
}

void Lexer::scanIdentifier() {
    const auto isAlpha = [](char c) {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               c == '_';
    };

    const auto isDigit = [](char c) {
        return c >= '0' && c <= '9';
    };

    while (isAlpha(peek()) || isDigit(peek())) {
        static_cast<void>(advance());
    }

    const std::string word =
        source_->text.substr(start_, current_ - start_);

    for (const TokenKind kind : languageKeywordTokens()) {
        if (word == tokenKindName(kind)) {
            addToken(kind);
            return;
        }
    }

    addToken(TokenKind::Identifier);
}

void Lexer::scanNumber() {
    while (peek() >= '0' && peek() <= '9') {
        static_cast<void>(advance());
    }

    if (peek() == '.' && peekNext() >= '0' && peekNext() <= '9') {
        static_cast<void>(advance());

        while (peek() >= '0' && peek() <= '9') {
            static_cast<void>(advance());
        }

        addToken(TokenKind::Float);
        return;
    }

    addToken(TokenKind::Integer);
}

void Lexer::scanString() {
    while (!atEnd() && peek() != '"') {
        if (peek() == '\n') {
            report("Unterminated string literal.");
            addToken(TokenKind::Invalid);
            return;
        }

        if (peek() == '\\') {
            static_cast<void>(advance());

            if (atEnd()) {
                break;
            }

            static_cast<void>(advance());
            continue;
        }

        static_cast<void>(advance());
    }

    if (atEnd()) {
        report("Unterminated string literal.");
        addToken(TokenKind::Invalid);
        return;
    }

    static_cast<void>(advance());
    addToken(TokenKind::String);
}

void Lexer::scanToken() {
    const char character = advance();

    switch (character) {
    case ' ':
    case '\r':
    case '\t':
    case '\n':
        return;

    case '(' : addToken(TokenKind::LeftParen); return;
    case ')' : addToken(TokenKind::RightParen); return;
    case '{' : addToken(TokenKind::LeftBrace); return;
    case '}' : addToken(TokenKind::RightBrace); return;
    case ',' : addToken(TokenKind::Comma); return;
    case ':' : addToken(TokenKind::Colon); return;
    case ';' : addToken(TokenKind::Semicolon); return;
    case '.' : addToken(TokenKind::Dot); return;
    case '+' : addToken(TokenKind::Plus); return;
    case '*' : addToken(TokenKind::Star); return;
    case '%' : addToken(TokenKind::Percent); return;

    case '-':
        addToken(match('>') ? TokenKind::Arrow : TokenKind::Minus);
        return;

    case '!':
        addToken(match('=') ? TokenKind::BangEqual : TokenKind::Bang);
        return;

    case '=':
        addToken(match('=') ? TokenKind::EqualEqual : TokenKind::Equal);
        return;

    case '<':
        addToken(match('=') ? TokenKind::LessEqual : TokenKind::Less);
        return;

    case '>':
        addToken(match('=') ? TokenKind::GreaterEqual : TokenKind::Greater);
        return;

    case '&':
        if (match('&')) {
            addToken(TokenKind::AndAnd);
        } else {
            report("Expected '&' after '&'.");
            addToken(TokenKind::Invalid);
        }
        return;

    case '|':
        if (match('|')) {
            addToken(TokenKind::OrOr);
        } else {
            report("Expected '|' after '|'.");
            addToken(TokenKind::Invalid);
        }
        return;

    case '/':
        if (match('/')) {
            skipLineComment();
        } else {
            addToken(TokenKind::Slash);
        }
        return;

    case '"':
        scanString();
        return;

    default:
        break;
    }

    if (character >= '0' && character <= '9') {
        scanNumber();
        return;
    }

    if ((character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        character == '_') {
        scanIdentifier();
        return;
    }

    report("Unexpected character.");
    addToken(TokenKind::Invalid);
}

}
