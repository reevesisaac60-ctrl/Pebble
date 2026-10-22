#pragma once

#include "pebble/source.hpp"

#include <string>
#include <string_view>

namespace pebble {

enum class TokenKind {
    EndOfFile,
    Invalid,

    Identifier,
    Integer,
    Float,
    String,

    Fn,
    Let,
    Var,
    Import,
    Package,
    Export,
    If,
    Else,
    While,
    Return,
    True,
    False,
    Null,

    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    Comma,
    Colon,
    Semicolon,
    Dot,

    Plus,
    Minus,
    Star,
    Slash,
    Percent,

    Equal,
    EqualEqual,
    Bang,
    BangEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,

    AndAnd,
    OrOr,
    Arrow
};

struct Token {
    TokenKind kind = TokenKind::Invalid;
    std::string lexeme;
    SourceLocation location;
};

[[nodiscard]] std::string_view tokenKindName(TokenKind kind) noexcept;

}
