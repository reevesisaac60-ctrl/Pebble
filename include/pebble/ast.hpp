#pragma once

#include "pebble/token.hpp"

#include <memory>
#include <string>
#include <vector>

namespace pebble {

enum class NodeKind {
    Program,
    Function,
    Parameter,
    Block,
    LetDeclaration,
    VarDeclaration,
    TypeName,
    ImportDeclaration,
    Assignment,
    ExpressionStatement,
    ReturnStatement,
    IfStatement,
    WhileStatement,
    BinaryExpression,
    UnaryExpression,
    CallExpression,
    MemberExpression,
    NameExpression,
    LiteralExpression
};

struct Node {
    NodeKind kind;
    SourceLocation location;
    std::string text;
    std::vector<std::unique_ptr<Node>> children;
    bool exported = false;
    bool local = false;

    Node(NodeKind nodeKind, SourceLocation sourceLocation, std::string nodeText = {})
        : kind(nodeKind),
          location(std::move(sourceLocation)),
          text(std::move(nodeText)) {}

    Node& add(std::unique_ptr<Node> child) {
        children.push_back(std::move(child));
        return *this;
    }
};

using Ast = std::unique_ptr<Node>;

[[nodiscard]] std::string_view nodeKindName(NodeKind kind) noexcept;

}
