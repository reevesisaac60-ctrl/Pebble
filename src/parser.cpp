#include "pebble/parser.hpp"

#include <utility>

namespace pebble {

std::string_view nodeKindName(NodeKind kind) noexcept {
    switch (kind) {
    case NodeKind::Program: return "Program";
    case NodeKind::Function: return "Function";
    case NodeKind::Parameter: return "Parameter";
    case NodeKind::Block: return "Block";
    case NodeKind::LetDeclaration: return "LetDeclaration";
    case NodeKind::VarDeclaration: return "VarDeclaration";
    case NodeKind::TypeName: return "TypeName";
    case NodeKind::ImportDeclaration: return "ImportDeclaration";
    case NodeKind::Assignment: return "Assignment";
    case NodeKind::ExpressionStatement: return "ExpressionStatement";
    case NodeKind::ReturnStatement: return "ReturnStatement";
    case NodeKind::IfStatement: return "IfStatement";
    case NodeKind::WhileStatement: return "WhileStatement";
    case NodeKind::BinaryExpression: return "BinaryExpression";
    case NodeKind::UnaryExpression: return "UnaryExpression";
    case NodeKind::CallExpression: return "CallExpression";
    case NodeKind::MemberExpression: return "MemberExpression";
    case NodeKind::NameExpression: return "NameExpression";
    case NodeKind::LiteralExpression: return "LiteralExpression";
    }

    return "Unknown";
}

ParseResult Parser::parse(const LexResult& lexed) const {
    ParseResult result = parse(lexed.tokens);

    if (!lexed.diagnostics.empty()) {
        result.diagnostics.insert(
            result.diagnostics.begin(),
            lexed.diagnostics.begin(),
            lexed.diagnostics.end()
        );
    }

    return result;
}

ParseResult Parser::parse(const std::vector<Token>& tokens) const {
    Parser parser;
    parser.tokens_ = &tokens;

    auto root = std::make_unique<Node>(
        NodeKind::Program,
        tokens.empty() ? SourceLocation{} : tokens.front().location
    );

    while (!parser.atEnd()) {
        const std::size_t before = parser.current_;
        Ast item = parser.declaration();

        if (item) {
            root->add(std::move(item));
        }

        if (parser.current_ == before) {
            parser.synchronize();

            if (parser.current_ == before && !parser.atEnd()) {
                ++parser.current_;
            }
        }
    }

    return {
        std::move(root),
        std::move(parser.diagnostics_)
    };
}

bool Parser::atEnd() const noexcept {
    return tokens_->empty() ||
           current_ >= tokens_->size() ||
           peek().kind == TokenKind::EndOfFile;
}

const Token& Parser::peek() const noexcept {
    static const Token fallback{
        TokenKind::EndOfFile, "", SourceLocation{}
    };

    if (tokens_->empty()) {
        return fallback;
    }

    return (*tokens_)[current_ < tokens_->size()
        ? current_
        : tokens_->size() - 1];
}

const Token& Parser::previous() const noexcept {
    return (*tokens_)[current_ == 0 ? 0 : current_ - 1];
}

bool Parser::check(TokenKind kind) const noexcept {
    return !atEnd() && peek().kind == kind;
}

bool Parser::match(TokenKind kind) noexcept {
    if (!check(kind)) {
        return false;
    }

    ++current_;
    return true;
}

const Token* Parser::consume(TokenKind kind, const char* message) {
    if (check(kind)) {
        ++current_;
        return &previous();
    }

    report(peek(), message);
    return nullptr;
}

void Parser::report(const Token& token, std::string message) {
    diagnostics_.push_back({
        token.location,
        std::move(message)
    });
}

bool Parser::atLineBreak() const noexcept {
    if (tokens_->empty() || current_ == 0U || current_ >= tokens_->size()) {
        return false;
    }

    const Token& next = (*tokens_)[current_];

    if (next.kind == TokenKind::EndOfFile) {
        return false;
    }

    return next.location.line > (*tokens_)[current_ - 1U].location.line;
}

bool Parser::continuesLine() const noexcept {
    if (tokens_->empty() || current_ == 0U || current_ >= tokens_->size()) {
        return false;
    }

    const Token& next = (*tokens_)[current_];

    if (next.kind == TokenKind::EndOfFile) {
        return false;
    }

    return next.location.line == (*tokens_)[current_ - 1U].location.line;
}

bool Parser::atBinaryOperator() const noexcept {
    switch (peek().kind) {
    case TokenKind::Plus:
    case TokenKind::Star:
    case TokenKind::Slash:
    case TokenKind::Percent:
    case TokenKind::Equal:
    case TokenKind::EqualEqual:
    case TokenKind::BangEqual:
    case TokenKind::Less:
    case TokenKind::LessEqual:
    case TokenKind::Greater:
    case TokenKind::GreaterEqual:
    case TokenKind::AndAnd:
    case TokenKind::OrOr:
        return true;
    default:
        return false;
    }
}

bool Parser::continuesMember() const noexcept {
    return check(TokenKind::Dot) || check(TokenKind::Colon);
}

bool Parser::endStatement(const char* message) {
    if (match(TokenKind::Semicolon)) {
        return true;
    }

    if (atEnd() || check(TokenKind::RightBrace) || atLineBreak()) {
        return true;
    }

    report(peek(), message);
    return false;
}

void Parser::synchronize() {
    if (!atEnd()) {
        ++current_;
    }

    while (!atEnd()) {
        if (previous().kind == TokenKind::Semicolon) {
            return;
        }

        if (atLineBreak()) {
            return;
        }

        switch (peek().kind) {
        case TokenKind::Fn:
        case TokenKind::Let:
        case TokenKind::Var:
        case TokenKind::Import:
        case TokenKind::Export:
        case TokenKind::If:
        case TokenKind::Else:
        case TokenKind::While:
        case TokenKind::Return:
        case TokenKind::RightBrace:
            return;
        default:
            ++current_;
            break;
        }
    }
}

Ast Parser::declaration() {
    if (match(TokenKind::Semicolon)) {
        return nullptr;
    }

    if (match(TokenKind::Export)) {
        if (!match(TokenKind::Fn)) {
            report(previous(), "Expected 'fn' after 'export'.");
            return nullptr;
        }

        Ast function = functionDeclaration();

        if (function != nullptr) {
            function->exported = true;
        }

        return function;
    }

    if (match(TokenKind::Fn)) {
        return functionDeclaration();
    }

    if (match(TokenKind::Import)) {
        return importDeclaration();
    }

    if (match(TokenKind::Let)) {
        return variableDeclaration();
    }

    if (match(TokenKind::Var)) {
        return variableDeclaration();
    }

    if (check(TokenKind::EndOfFile)) {
        return nullptr;
    }

    return statement();
}

Ast Parser::importDeclaration() {
    const bool explicitPackage = match(TokenKind::Package);
    const char* const message = explicitPackage
        ? "Expected a package name after 'package'."
        : "Expected a package name after 'import'.";

    if (atEnd() || check(TokenKind::RightBrace) || atLineBreak()) {
        report(previous(), message);
        return nullptr;
    }

    const Token* name = consume(TokenKind::Identifier, message);

    if (name == nullptr) {
        return nullptr;
    }

    static_cast<void>(endStatement("Expected a line break or ';' after the import."));

    return std::make_unique<Node>(
        NodeKind::ImportDeclaration, name->location, name->lexeme
    );
}

Ast Parser::functionDeclaration() {
    const Token* name = consume(
        TokenKind::Identifier,
        "Expected a function name after 'fn'."
    );

    if (name == nullptr) {
        return nullptr;
    }

    auto function = std::make_unique<Node>(
        NodeKind::Function, name->location, name->lexeme
    );

    if (consume(TokenKind::LeftParen, "Expected '(' after function name.") == nullptr) {
        return function;
    }

    if (!check(TokenKind::RightParen)) {
        do {
            Ast argument = parameter();

            if (argument) {
                function->add(std::move(argument));
            } else {
                break;
            }
        } while (match(TokenKind::Comma));
    }

    static_cast<void>(consume(
        TokenKind::RightParen,
        "Expected ')' after function parameters."
    ));

    if (match(TokenKind::Arrow)) {
        Ast returnType = typeAnnotation();

        if (returnType) {
            function->add(std::move(returnType));
        }
    }

    Ast body = block();

    if (body) {
        function->add(std::move(body));
    }

    return function;
}

Ast Parser::parameter() {
    const Token* name = consume(
        TokenKind::Identifier,
        "Expected a parameter name."
    );

    if (name == nullptr) {
        return nullptr;
    }

    auto result = std::make_unique<Node>(
        NodeKind::Parameter, name->location, name->lexeme
    );

    if (match(TokenKind::Colon)) {
        const Token* type = consume(
            TokenKind::Identifier,
            "Expected a type name after ':'."
        );

        if (type) {
            result->add(std::make_unique<Node>(
                NodeKind::TypeName, type->location, type->lexeme
            ));
        }
    }

    return result;
}

Ast Parser::typeAnnotation() {
    const Token* type = consume(
        TokenKind::Identifier,
        "Expected a type name."
    );

    if (!type) {
        return nullptr;
    }

    return std::make_unique<Node>(
        NodeKind::TypeName, type->location, type->lexeme
    );
}

Ast Parser::block() {
    const Token* opening = consume(
        TokenKind::LeftBrace,
        "Expected '{' to begin a block."
    );

    if (!opening) {
        return nullptr;
    }

    auto result = std::make_unique<Node>(
        NodeKind::Block, opening->location
    );

    while (!atEnd() && !check(TokenKind::RightBrace)) {
        const std::size_t before = current_;
        Ast item = declaration();

        if (item) {
            result->add(std::move(item));
        }

        if (current_ == before) {
            synchronize();

            if (current_ == before && !atEnd()) {
                ++current_;
            }
        }
    }

    static_cast<void>(consume(
        TokenKind::RightBrace,
        "Expected '}' after block."
    ));

    return result;
}

Ast Parser::statement() {
    if (match(TokenKind::If)) {
        return ifStatement();
    }

    if (match(TokenKind::While)) {
        return whileStatement();
    }

    if (match(TokenKind::LeftBrace)) {
        --current_;
        return block();
    }

    if (match(TokenKind::Return)) {
        return returnStatement();
    }

    Ast value = expression();

    if (!value) {
        return nullptr;
    }

    static_cast<void>(endStatement(
        "Expected a line break or ';' after the expression."
    ));

    auto result = std::make_unique<Node>(
        NodeKind::ExpressionStatement, value->location
    );

    result->add(std::move(value));
    return result;
}

Ast Parser::ifStatement() {
    const Token& keyword = previous();
    Ast condition = expression();

    if (!condition) {
        return nullptr;
    }

    Ast thenBranch = block();

    if (!thenBranch) {
        return nullptr;
    }

    auto result = std::make_unique<Node>(
        NodeKind::IfStatement, keyword.location
    );
    result->add(std::move(condition));
    result->add(std::move(thenBranch));

    if (match(TokenKind::Else)) {
        Ast elseBranch;

        if (match(TokenKind::If)) {
            elseBranch = ifStatement();
        } else {
            elseBranch = block();
        }

        if (elseBranch) {
            result->add(std::move(elseBranch));
        }
    }

    return result;
}

Ast Parser::whileStatement() {
    const Token& keyword = previous();
    Ast condition = expression();

    if (!condition) {
        return nullptr;
    }

    Ast body = block();

    if (!body) {
        return nullptr;
    }

    auto result = std::make_unique<Node>(
        NodeKind::WhileStatement, keyword.location
    );
    result->add(std::move(condition));
    result->add(std::move(body));
    return result;
}

Ast Parser::variableDeclaration() {
    const Token& keyword = previous();
    const NodeKind kind = keyword.kind == TokenKind::Let
        ? NodeKind::LetDeclaration
        : NodeKind::VarDeclaration;

    const Token* name = consume(
        TokenKind::Identifier,
        "Expected a variable name."
    );

    if (!name) {
        return nullptr;
    }

    auto result = std::make_unique<Node>(
        kind, keyword.location, name->lexeme
    );

    if (match(TokenKind::Colon)) {
        Ast type = typeAnnotation();

        if (type) {
            result->add(std::move(type));
        }
    }

    if (match(TokenKind::Equal)) {
        Ast initializer = expression();

        if (initializer) {
            result->add(std::move(initializer));
        }
    } else {
        report(peek(), "Expected '=' and an initializer for this variable.");
    }

    static_cast<void>(endStatement(
        "Expected a line break or ';' after the variable declaration."
    ));

    return result;
}

Ast Parser::returnStatement() {
    const Token& keyword = previous();

    auto result = std::make_unique<Node>(
        NodeKind::ReturnStatement, keyword.location
    );

    if (!check(TokenKind::Semicolon) &&
        !atEnd() &&
        !check(TokenKind::RightBrace) &&
        !atLineBreak()) {
        Ast value = expression();

        if (value) {
            result->add(std::move(value));
        }
    }

    static_cast<void>(endStatement(
        "Expected a line break or ';' after the return statement."
    ));

    return result;
}

Ast Parser::expression() {
    return assignment();
}

Ast Parser::assignment() {
    Ast left = logicalOr();

    if (!check(TokenKind::Equal) || !continuesLine()) {
        return left;
    }

    ++current_;

    const Token& op = previous();
    Ast right = assignment();

    if (!right) {
        report(op, "Expected an expression after '='.");
        return left;
    }

    if (!left || left->kind != NodeKind::NameExpression) {
        report(op, "Invalid assignment target; expected a variable name.");
        return binary(std::move(left), op, std::move(right));
    }

    auto result = std::make_unique<Node>(
        NodeKind::Assignment, op.location, left->text
    );

    result->add(std::move(right));
    return result;
}

Ast Parser::logicalOr() {
    Ast left = logicalAnd();

    while (check(TokenKind::OrOr) && continuesLine()) {
        ++current_;
        const Token& op = previous();
        left = binary(std::move(left), op, logicalAnd());
    }

    return left;
}

Ast Parser::logicalAnd() {
    Ast left = equality();

    while (check(TokenKind::AndAnd) && continuesLine()) {
        ++current_;
        const Token& op = previous();
        left = binary(std::move(left), op, equality());
    }

    return left;
}

Ast Parser::equality() {
    Ast left = comparison();

    while ((check(TokenKind::EqualEqual) || check(TokenKind::BangEqual)) &&
           continuesLine()) {
        ++current_;
        const Token& op = previous();
        left = binary(std::move(left), op, comparison());
    }

    return left;
}

Ast Parser::comparison() {
    Ast left = term();

    while ((check(TokenKind::Less) ||
            check(TokenKind::LessEqual) ||
            check(TokenKind::Greater) ||
            check(TokenKind::GreaterEqual)) &&
           continuesLine()) {
        ++current_;
        const Token& op = previous();
        left = binary(std::move(left), op, term());
    }

    return left;
}

Ast Parser::term() {
    Ast left = factor();

    while ((check(TokenKind::Plus) || check(TokenKind::Minus)) &&
           continuesLine()) {
        ++current_;
        const Token& op = previous();
        left = binary(std::move(left), op, factor());
    }

    return left;
}

Ast Parser::factor() {
    Ast left = unary();

    while ((check(TokenKind::Star) ||
            check(TokenKind::Slash) ||
            check(TokenKind::Percent)) &&
           continuesLine()) {
        ++current_;
        const Token& op = previous();
        left = binary(std::move(left), op, unary());
    }

    return left;
}

Ast Parser::unary() {
    if (match(TokenKind::Bang) || match(TokenKind::Minus)) {
        const Token& op = previous();
        Ast operand = unary();

        auto result = std::make_unique<Node>(
            NodeKind::UnaryExpression, op.location, op.lexeme
        );

        if (operand) {
            result->add(std::move(operand));
        }

        return result;
    }

    return call();
}

Ast Parser::call() {
    Ast callee = primary();

    while (callee && check(TokenKind::LeftParen) && continuesLine()) {
        ++current_;

        const Token& opening = previous();

        auto invocation = std::make_unique<Node>(
            NodeKind::CallExpression, opening.location
        );

        invocation->add(std::move(callee));

        if (!check(TokenKind::RightParen)) {
            do {
                Ast argument = expression();

                if (argument) {
                    invocation->add(std::move(argument));
                } else {
                    break;
                }
            } while (match(TokenKind::Comma));
        }

        static_cast<void>(consume(
            TokenKind::RightParen,
            "Expected ')' after call arguments."
        ));

        callee = std::move(invocation);
    }

    return callee;
}

Ast Parser::primary() {
    if (match(TokenKind::Integer) ||
        match(TokenKind::Float) ||
        match(TokenKind::String) ||
        match(TokenKind::True) ||
        match(TokenKind::False) ||
        match(TokenKind::Null)) {
        const Token& token = previous();

        return std::make_unique<Node>(
            NodeKind::LiteralExpression, token.location, token.lexeme
        );
    }

    if (match(TokenKind::Identifier)) {
        const Token& token = previous();

        auto name = std::make_unique<Node>(
            NodeKind::NameExpression, token.location, token.lexeme
        );

        if (!continuesLine() && !continuesMember()) {
            return name;
        }

        if (!match(TokenKind::Dot) && !match(TokenKind::Colon)) {
            return name;
        }

        const Token* member = consume(
            TokenKind::Identifier,
            "Expected a member name after '.'."
        );

        if (member == nullptr) {
            return name;
        }

        auto result = std::make_unique<Node>(
            NodeKind::MemberExpression, member->location, member->lexeme
        );

        result->add(std::move(name));
        return result;
    }

    if (match(TokenKind::LeftParen)) {
        Ast value = expression();

        static_cast<void>(consume(
            TokenKind::RightParen,
            "Expected ')' after expression."
        ));

        return value;
    }

    if (atLineBreak() && continuesMember()) {
        report(
            peek(),
            "Unexpected '" + std::string(peek().lexeme) + "'. A member "
            "continues the expression above it, so it needs a name or a call "
            "before it."
        );

        return nullptr;
    }

    if (atLineBreak() && atBinaryOperator()) {
        if (check(TokenKind::Equal)) {
            report(
                peek(),
                "Unexpected '='. The '=' of an assignment stands on the line "
                "of the name it assigns to."
            );
        } else {
            report(
                peek(),
                "Unexpected '" + std::string(peek().lexeme) + "'. A binary "
                "operator cannot start a line; it stands at the end of the "
                "line of the expression it continues."
            );
        }

        return nullptr;
    }

    report(peek(), "Expected an expression.");
    return nullptr;
}

Ast Parser::binary(Ast left, const Token& op, Ast right) {
    auto result = std::make_unique<Node>(
        NodeKind::BinaryExpression, op.location, op.lexeme
    );

    if (left) {
        result->add(std::move(left));
    }

    if (right) {
        result->add(std::move(right));
    }

    return result;
}

}
