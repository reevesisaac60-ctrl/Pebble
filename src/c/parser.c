#include "pebble_c/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const PebbleCTokenList *tokens;
    size_t current;
    PebbleCDiagnosticList diagnostics;
} Parser;

static PebbleCNode *declaration(Parser *parser);
static PebbleCNode *statement(Parser *parser);
static PebbleCNode *expression(Parser *parser);
static PebbleCNode *assignment(Parser *parser);
static PebbleCNode *parse_block(Parser *parser);

static PebbleCToken fallback_token(void)
{
    PebbleCToken token = {
        PEBBLE_C_TOKEN_EOF,
        NULL,
        {NULL, 1U, 1U}
    };
    return token;
}

static const PebbleCToken *peek(const Parser *parser)
{
    if (parser->tokens == NULL ||
        parser->tokens->count == 0U ||
        parser->current >= parser->tokens->count) {
        static PebbleCToken fallback;
        fallback = fallback_token();
        return &fallback;
    }

    return &parser->tokens->items[parser->current];
}

static const PebbleCToken *previous(const Parser *parser)
{
    if (parser->tokens == NULL || parser->tokens->count == 0U) {
        return peek(parser);
    }

    if (parser->current == 0U) {
        return &parser->tokens->items[0];
    }

    return &parser->tokens->items[parser->current - 1U];
}

static int at_end(const Parser *parser)
{
    return peek(parser)->kind == PEBBLE_C_TOKEN_EOF;
}

static int check(const Parser *parser, PebbleCTokenKind kind)
{
    return !at_end(parser) && peek(parser)->kind == kind;
}

static const PebbleCToken *advance_token(Parser *parser)
{
    const PebbleCToken *token = peek(parser);

    if (!at_end(parser)) {
        ++parser->current;
    }

    return token;
}

static int match(Parser *parser, PebbleCTokenKind kind)
{
    if (!check(parser, kind)) {
        return 0;
    }

    (void)advance_token(parser);
    return 1;
}

static char *copy_string(const char *text)
{
    size_t length;
    char *result;

    if (text == NULL) {
        text = "";
    }

    length = strlen(text);
    result = malloc(length + 1U);

    if (result != NULL) {
        memcpy(result, text, length + 1U);
    }

    return result;
}

static void report(Parser *parser, const PebbleCToken *token,
                   const char *message)
{
    PebbleCDiagnostic *grown;
    PebbleCDiagnostic diagnostic;
    size_t capacity;

    diagnostic.location = token->location;
    diagnostic.message = copy_string(message);

    if (diagnostic.message == NULL) {
        return;
    }

    if (parser->diagnostics.count == parser->diagnostics.capacity) {
        capacity = parser->diagnostics.capacity == 0U
            ? 4U
            : parser->diagnostics.capacity * 2U;

        if (capacity < parser->diagnostics.capacity ||
            capacity > (size_t)-1 / sizeof(*parser->diagnostics.items)) {
            free(diagnostic.message);
            return;
        }

        grown = realloc(
            parser->diagnostics.items,
            capacity * sizeof(*parser->diagnostics.items)
        );

        if (grown == NULL) {
            free(diagnostic.message);
            return;
        }

        parser->diagnostics.items = grown;
        parser->diagnostics.capacity = capacity;
    }

    parser->diagnostics.items[parser->diagnostics.count] = diagnostic;
    ++parser->diagnostics.count;
}

static const PebbleCToken *consume(
    Parser *parser,
    PebbleCTokenKind kind,
    const char *message
)
{
    if (check(parser, kind)) {
        return advance_token(parser);
    }

    report(parser, peek(parser), message);
    return NULL;
}

static PebbleCNode *make_node(
    PebbleCNodeKind kind,
    PebbleCSourceLocation location,
    const char *text
)
{
    return pebble_c_node_create(kind, location, text);
}

static void add_child(PebbleCNode *parent, PebbleCNode *child)
{
    if (child == NULL) {
        return;
    }

    if (!pebble_c_node_add(parent, child)) {
        pebble_c_node_destroy(child);
    }
}

static void synchronize(Parser *parser);

static int at_line_break(const Parser *parser)
{
    const PebbleCToken *next;

    if (parser->tokens == NULL ||
        parser->tokens->count == 0U ||
        parser->current == 0U ||
        parser->current >= parser->tokens->count) {
        return 0;
    }

    next = peek(parser);

    if (next->kind == PEBBLE_C_TOKEN_EOF) {
        return 0;
    }

    return next->location.line > previous(parser)->location.line;
}

static int continues_line(const Parser *parser)
{
    const PebbleCToken *next;

    if (parser->tokens == NULL ||
        parser->tokens->count == 0U ||
        parser->current == 0U ||
        parser->current >= parser->tokens->count) {
        return 0;
    }

    next = peek(parser);

    if (next->kind == PEBBLE_C_TOKEN_EOF) {
        return 0;
    }

    return next->location.line == previous(parser)->location.line;
}

static int at_binary_operator(const Parser *parser)
{
    switch (peek(parser)->kind) {
    case PEBBLE_C_TOKEN_PLUS:
    case PEBBLE_C_TOKEN_STAR:
    case PEBBLE_C_TOKEN_SLASH:
    case PEBBLE_C_TOKEN_PERCENT:
    case PEBBLE_C_TOKEN_EQUAL:
    case PEBBLE_C_TOKEN_EQUAL_EQUAL:
    case PEBBLE_C_TOKEN_BANG_EQUAL:
    case PEBBLE_C_TOKEN_LESS:
    case PEBBLE_C_TOKEN_LESS_EQUAL:
    case PEBBLE_C_TOKEN_GREATER:
    case PEBBLE_C_TOKEN_GREATER_EQUAL:
    case PEBBLE_C_TOKEN_AND_AND:
    case PEBBLE_C_TOKEN_OR_OR:
        return 1;
    default:
        return 0;
    }
}

static int continues_member(const Parser *parser)
{
    return check(parser, PEBBLE_C_TOKEN_DOT) ||
           check(parser, PEBBLE_C_TOKEN_COLON);
}

static void synchronize(Parser *parser)
{
    if (!at_end(parser)) {
        (void)advance_token(parser);
    }

    while (!at_end(parser)) {
        if (previous(parser)->kind == PEBBLE_C_TOKEN_SEMICOLON) {
            return;
        }

        if (at_line_break(parser)) {
            return;
        }

        switch (peek(parser)->kind) {
        case PEBBLE_C_TOKEN_FN:
        case PEBBLE_C_TOKEN_LET:
        case PEBBLE_C_TOKEN_VAR:
        case PEBBLE_C_TOKEN_IMPORT:
        case PEBBLE_C_TOKEN_EXPORT:
        case PEBBLE_C_TOKEN_IF:
        case PEBBLE_C_TOKEN_ELSE:
        case PEBBLE_C_TOKEN_WHILE:
        case PEBBLE_C_TOKEN_RETURN:
        case PEBBLE_C_TOKEN_RIGHT_BRACE:
            return;
        default:
            (void)advance_token(parser);
            break;
        }
    }
}

static int end_statement(Parser *parser, const char *message)
{
    if (match(parser, PEBBLE_C_TOKEN_SEMICOLON)) {
        return 1;
    }

    if (at_end(parser) ||
        check(parser, PEBBLE_C_TOKEN_RIGHT_BRACE) ||
        at_line_break(parser)) {
        return 1;
    }

    report(parser, peek(parser), message);
    return 0;
}

static PebbleCNode *type_annotation(Parser *parser)
{
    const PebbleCToken *type = consume(
        parser,
        PEBBLE_C_TOKEN_IDENTIFIER,
        "Expected a type name."
    );

    if (type == NULL) {
        return NULL;
    }

    return make_node(
        PEBBLE_C_NODE_TYPE_NAME,
        type->location,
        type->lexeme
    );
}

static PebbleCNode *parameter(Parser *parser)
{
    const PebbleCToken *name = consume(
        parser,
        PEBBLE_C_TOKEN_IDENTIFIER,
        "Expected a parameter name."
    );
    PebbleCNode *node;

    if (name == NULL) {
        return NULL;
    }

    node = make_node(
        PEBBLE_C_NODE_PARAMETER,
        name->location,
        name->lexeme
    );

    if (node == NULL) {
        return NULL;
    }

    if (match(parser, PEBBLE_C_TOKEN_COLON)) {
        add_child(node, type_annotation(parser));
    }

    return node;
}

static PebbleCNode *import_declaration(Parser *parser)
{
    const int explicit_package = match(parser, PEBBLE_C_TOKEN_PACKAGE);
    const char *message = explicit_package
        ? "Expected a package name after 'package'."
        : "Expected a package name after 'import'.";
    const PebbleCToken *name;

    if (at_end(parser) ||
        check(parser, PEBBLE_C_TOKEN_RIGHT_BRACE) ||
        at_line_break(parser)) {
        report(parser, previous(parser), message);
        return NULL;
    }

    name = consume(parser, PEBBLE_C_TOKEN_IDENTIFIER, message);

    if (name == NULL) {
        return NULL;
    }

    (void)end_statement(parser, "Expected a line break or ';' after the import.");

    return make_node(
        PEBBLE_C_NODE_IMPORT_DECLARATION,
        name->location,
        name->lexeme
    );
}

static PebbleCNode *function_declaration(Parser *parser)
{
    const PebbleCToken *name = consume(
        parser,
        PEBBLE_C_TOKEN_IDENTIFIER,
        "Expected a function name after 'fn'."
    );
    PebbleCNode *function;

    if (name == NULL) {
        return NULL;
    }

    function = make_node(
        PEBBLE_C_NODE_FUNCTION,
        name->location,
        name->lexeme
    );

    if (function == NULL) {
        return NULL;
    }

    if (consume(
            parser,
            PEBBLE_C_TOKEN_LEFT_PAREN,
            "Expected '(' after function name."
        ) == NULL) {
        return function;
    }

    if (!check(parser, PEBBLE_C_TOKEN_RIGHT_PAREN)) {
        do {
            PebbleCNode *argument = parameter(parser);

            if (argument == NULL) {
                break;
            }

            add_child(function, argument);
        } while (match(parser, PEBBLE_C_TOKEN_COMMA));
    }

    (void)consume(
        parser,
        PEBBLE_C_TOKEN_RIGHT_PAREN,
        "Expected ')' after function parameters."
    );

    if (match(parser, PEBBLE_C_TOKEN_ARROW)) {
        add_child(function, type_annotation(parser));
    }

    add_child(function, parse_block(parser));
    return function;
}

static PebbleCNode *variable_declaration(Parser *parser)
{
    const PebbleCToken *keyword = previous(parser);
    const PebbleCToken *name = consume(
        parser,
        PEBBLE_C_TOKEN_IDENTIFIER,
        "Expected a variable name."
    );
    PebbleCNode *node;

    if (name == NULL) {
        return NULL;
    }

    node = make_node(
        keyword->kind == PEBBLE_C_TOKEN_LET
            ? PEBBLE_C_NODE_LET_DECLARATION
            : PEBBLE_C_NODE_VAR_DECLARATION,
        keyword->location,
        name->lexeme
    );

    if (node == NULL) {
        return NULL;
    }

    if (match(parser, PEBBLE_C_TOKEN_COLON)) {
        add_child(node, type_annotation(parser));
    }

    if (match(parser, PEBBLE_C_TOKEN_EQUAL)) {
        add_child(node, expression(parser));
    } else {
        report(
            parser,
            peek(parser),
            "Expected '=' and an initializer for this variable."
        );
    }

    (void)end_statement(
        parser,
        "Expected a line break or ';' after the variable declaration."
    );

    return node;
}

static PebbleCNode *parse_block(Parser *parser)
{
    const PebbleCToken *opening = consume(
        parser,
        PEBBLE_C_TOKEN_LEFT_BRACE,
        "Expected '{' to begin a block."
    );
    PebbleCNode *block;

    if (opening == NULL) {
        return NULL;
    }

    block = make_node(PEBBLE_C_NODE_BLOCK, opening->location, NULL);

    if (block == NULL) {
        return NULL;
    }

    while (!at_end(parser) &&
           !check(parser, PEBBLE_C_TOKEN_RIGHT_BRACE)) {
        size_t before = parser->current;
        PebbleCNode *item = declaration(parser);

        add_child(block, item);

        if (parser->current == before) {
            synchronize(parser);

            if (parser->current == before && !at_end(parser)) {
                (void)advance_token(parser);
            }
        }
    }

    (void)consume(
        parser,
        PEBBLE_C_TOKEN_RIGHT_BRACE,
        "Expected '}' after block."
    );

    return block;
}

static PebbleCNode *if_statement(Parser *parser)
{
    const PebbleCToken *keyword = previous(parser);
    PebbleCNode *condition = expression(parser);
    PebbleCNode *then_branch;
    PebbleCNode *result;

    if (condition == NULL) {
        return NULL;
    }

    then_branch = parse_block(parser);

    if (then_branch == NULL) {
        pebble_c_node_destroy(condition);
        return NULL;
    }

    result = make_node(
        PEBBLE_C_NODE_IF_STATEMENT,
        keyword->location,
        NULL
    );

    if (result == NULL) {
        pebble_c_node_destroy(condition);
        pebble_c_node_destroy(then_branch);
        return NULL;
    }

    add_child(result, condition);
    add_child(result, then_branch);

    if (match(parser, PEBBLE_C_TOKEN_ELSE)) {
        PebbleCNode *else_branch;

        if (match(parser, PEBBLE_C_TOKEN_IF)) {
            else_branch = if_statement(parser);
        } else {
            else_branch = parse_block(parser);
        }

        add_child(result, else_branch);
    }

    return result;
}

static PebbleCNode *while_statement(Parser *parser)
{
    const PebbleCToken *keyword = previous(parser);
    PebbleCNode *condition = expression(parser);
    PebbleCNode *body;
    PebbleCNode *result;

    if (condition == NULL) {
        return NULL;
    }

    body = parse_block(parser);

    if (body == NULL) {
        pebble_c_node_destroy(condition);
        return NULL;
    }

    result = make_node(
        PEBBLE_C_NODE_WHILE_STATEMENT,
        keyword->location,
        NULL
    );

    if (result == NULL) {
        pebble_c_node_destroy(condition);
        pebble_c_node_destroy(body);
        return NULL;
    }

    add_child(result, condition);
    add_child(result, body);
    return result;
}

static PebbleCNode *return_statement(Parser *parser)
{
    const PebbleCToken *keyword = previous(parser);
    PebbleCNode *result = make_node(
        PEBBLE_C_NODE_RETURN_STATEMENT,
        keyword->location,
        NULL
    );

    if (result == NULL) {
        return NULL;
    }

    if (!check(parser, PEBBLE_C_TOKEN_SEMICOLON) &&
        !at_end(parser) &&
        !check(parser, PEBBLE_C_TOKEN_RIGHT_BRACE) &&
        !at_line_break(parser)) {
        add_child(result, expression(parser));
    }

    (void)end_statement(
        parser,
        "Expected a line break or ';' after the return statement."
    );

    return result;
}

static PebbleCNode *statement(Parser *parser)
{
    PebbleCNode *value;
    PebbleCNode *result;

    if (match(parser, PEBBLE_C_TOKEN_IF)) {
        return if_statement(parser);
    }

    if (match(parser, PEBBLE_C_TOKEN_WHILE)) {
        return while_statement(parser);
    }

    if (check(parser, PEBBLE_C_TOKEN_LEFT_BRACE)) {
        return parse_block(parser);
    }

    if (match(parser, PEBBLE_C_TOKEN_RETURN)) {
        return return_statement(parser);
    }

    value = expression(parser);

    if (value == NULL) {
        return NULL;
    }

    (void)end_statement(
        parser,
        "Expected a line break or ';' after the expression."
    );

    result = make_node(
        PEBBLE_C_NODE_EXPRESSION_STATEMENT,
        value->location,
        NULL
    );

    if (result == NULL) {
        pebble_c_node_destroy(value);
        return NULL;
    }

    add_child(result, value);
    return result;
}

static PebbleCNode *declaration(Parser *parser)
{
    if (match(parser, PEBBLE_C_TOKEN_SEMICOLON)) {
        return NULL;
    }

    if (match(parser, PEBBLE_C_TOKEN_EXPORT)) {
        PebbleCNode *function;

        if (!match(parser, PEBBLE_C_TOKEN_FN)) {
            report(parser, previous(parser),
                   "Expected 'fn' after 'export'.");
            return NULL;
        }

        function = function_declaration(parser);

        if (function != NULL) {
            function->exported = 1;
        }

        return function;
    }

    if (match(parser, PEBBLE_C_TOKEN_FN)) {
        return function_declaration(parser);
    }

    if (match(parser, PEBBLE_C_TOKEN_IMPORT)) {
        return import_declaration(parser);
    }

    if (match(parser, PEBBLE_C_TOKEN_LET) ||
        match(parser, PEBBLE_C_TOKEN_VAR)) {
        return variable_declaration(parser);
    }

    return statement(parser);
}

static PebbleCNode *binary_node(
    PebbleCNode *left,
    const PebbleCToken *op,
    PebbleCNode *right
)
{
    PebbleCNode *result = make_node(
        PEBBLE_C_NODE_BINARY_EXPRESSION,
        op->location,
        op->lexeme
    );

    if (result == NULL) {
        pebble_c_node_destroy(left);
        pebble_c_node_destroy(right);
        return NULL;
    }

    add_child(result, left);
    add_child(result, right);
    return result;
}

static PebbleCNode *logical_or(Parser *parser);
static PebbleCNode *logical_and(Parser *parser);
static PebbleCNode *equality(Parser *parser);
static PebbleCNode *comparison(Parser *parser);
static PebbleCNode *term(Parser *parser);
static PebbleCNode *factor(Parser *parser);
static PebbleCNode *unary(Parser *parser);
static PebbleCNode *call(Parser *parser);
static PebbleCNode *primary(Parser *parser);

static PebbleCNode *assignment(Parser *parser)
{
    PebbleCNode *left = logical_or(parser);
    const PebbleCToken *op;
    PebbleCNode *right;
    PebbleCNode *result;

    if (!check(parser, PEBBLE_C_TOKEN_EQUAL) || !continues_line(parser)) {
        return left;
    }

    (void)advance_token(parser);

    op = previous(parser);
    right = assignment(parser);

    if (right == NULL) {
        report(parser, op, "Expected an expression after '='.");
        return left;
    }

    if (left == NULL ||
        left->kind != PEBBLE_C_NODE_NAME_EXPRESSION) {
        report(
            parser,
            op,
            "Invalid assignment target; expected a variable name."
        );
        return binary_node(left, op, right);
    }

    result = make_node(
        PEBBLE_C_NODE_ASSIGNMENT,
        op->location,
        left->text
    );

    pebble_c_node_destroy(left);

    if (result == NULL) {
        pebble_c_node_destroy(right);
        return NULL;
    }

    add_child(result, right);
    return result;
}

static PebbleCNode *expression(Parser *parser)
{
    return assignment(parser);
}

static PebbleCNode *logical_or(Parser *parser)
{
    PebbleCNode *left = logical_and(parser);

    while (check(parser, PEBBLE_C_TOKEN_OR_OR) && continues_line(parser)) {
        const PebbleCToken *op;

        (void)advance_token(parser);
        op = previous(parser);
        left = binary_node(left, op, logical_and(parser));
    }

    return left;
}

static PebbleCNode *logical_and(Parser *parser)
{
    PebbleCNode *left = equality(parser);

    while (check(parser, PEBBLE_C_TOKEN_AND_AND) && continues_line(parser)) {
        const PebbleCToken *op;

        (void)advance_token(parser);
        op = previous(parser);
        left = binary_node(left, op, equality(parser));
    }

    return left;
}

static PebbleCNode *equality(Parser *parser)
{
    PebbleCNode *left = comparison(parser);

    while ((check(parser, PEBBLE_C_TOKEN_EQUAL_EQUAL) ||
            check(parser, PEBBLE_C_TOKEN_BANG_EQUAL)) &&
           continues_line(parser)) {
        const PebbleCToken *op = advance_token(parser);
        left = binary_node(left, op, comparison(parser));
    }

    return left;
}

static PebbleCNode *comparison(Parser *parser)
{
    PebbleCNode *left = term(parser);

    while ((check(parser, PEBBLE_C_TOKEN_LESS) ||
            check(parser, PEBBLE_C_TOKEN_LESS_EQUAL) ||
            check(parser, PEBBLE_C_TOKEN_GREATER) ||
            check(parser, PEBBLE_C_TOKEN_GREATER_EQUAL)) &&
           continues_line(parser)) {
        const PebbleCToken *op = advance_token(parser);
        left = binary_node(left, op, term(parser));
    }

    return left;
}

static PebbleCNode *term(Parser *parser)
{
    PebbleCNode *left = factor(parser);

    while ((check(parser, PEBBLE_C_TOKEN_PLUS) ||
            check(parser, PEBBLE_C_TOKEN_MINUS)) &&
           continues_line(parser)) {
        const PebbleCToken *op = advance_token(parser);
        left = binary_node(left, op, factor(parser));
    }

    return left;
}

static PebbleCNode *factor(Parser *parser)
{
    PebbleCNode *left = unary(parser);

    while ((check(parser, PEBBLE_C_TOKEN_STAR) ||
            check(parser, PEBBLE_C_TOKEN_SLASH) ||
            check(parser, PEBBLE_C_TOKEN_PERCENT)) &&
           continues_line(parser)) {
        const PebbleCToken *op = advance_token(parser);
        left = binary_node(left, op, unary(parser));
    }

    return left;
}

static PebbleCNode *unary(Parser *parser)
{
    if (match(parser, PEBBLE_C_TOKEN_BANG) ||
        match(parser, PEBBLE_C_TOKEN_MINUS)) {
        const PebbleCToken *op = previous(parser);
        PebbleCNode *result = make_node(
            PEBBLE_C_NODE_UNARY_EXPRESSION,
            op->location,
            op->lexeme
        );

        if (result == NULL) {
            return NULL;
        }

        add_child(result, unary(parser));
        return result;
    }

    return call(parser);
}

static PebbleCNode *call(Parser *parser)
{
    PebbleCNode *callee = primary(parser);

    while (callee != NULL &&
           check(parser, PEBBLE_C_TOKEN_LEFT_PAREN) &&
           continues_line(parser)) {
        const PebbleCToken *opening;
        PebbleCNode *invocation;

        (void)advance_token(parser);
        opening = previous(parser);
        invocation = make_node(
            PEBBLE_C_NODE_CALL_EXPRESSION,
            opening->location,
            NULL
        );

        if (invocation == NULL) {
            pebble_c_node_destroy(callee);
            return NULL;
        }

        add_child(invocation, callee);

        if (!check(parser, PEBBLE_C_TOKEN_RIGHT_PAREN)) {
            do {
                PebbleCNode *argument = expression(parser);

                if (argument == NULL) {
                    break;
                }

                add_child(invocation, argument);
            } while (match(parser, PEBBLE_C_TOKEN_COMMA));
        }

        (void)consume(
            parser,
            PEBBLE_C_TOKEN_RIGHT_PAREN,
            "Expected ')' after call arguments."
        );

        callee = invocation;
    }

    return callee;
}

static PebbleCNode *primary(Parser *parser)
{
    const PebbleCToken *token;

    if (match(parser, PEBBLE_C_TOKEN_INTEGER) ||
        match(parser, PEBBLE_C_TOKEN_FLOAT) ||
        match(parser, PEBBLE_C_TOKEN_STRING) ||
        match(parser, PEBBLE_C_TOKEN_TRUE) ||
        match(parser, PEBBLE_C_TOKEN_FALSE) ||
        match(parser, PEBBLE_C_TOKEN_NULL)) {
        token = previous(parser);
        return make_node(
            PEBBLE_C_NODE_LITERAL_EXPRESSION,
            token->location,
            token->lexeme
        );
    }

    if (match(parser, PEBBLE_C_TOKEN_IDENTIFIER)) {
        PebbleCNode *name;

        token = previous(parser);
        name = make_node(
            PEBBLE_C_NODE_NAME_EXPRESSION,
            token->location,
            token->lexeme
        );

        if (name == NULL) {
            return NULL;
        }

        if (!continues_line(parser) && !continues_member(parser)) {
            return name;
        }

        if (!match(parser, PEBBLE_C_TOKEN_DOT) &&
            !match(parser, PEBBLE_C_TOKEN_COLON)) {
            return name;
        }

        {
            const PebbleCToken *member = consume(
                parser,
                PEBBLE_C_TOKEN_IDENTIFIER,
                "Expected a member name after '.'."
            );
            PebbleCNode *result;

            if (member == NULL) {
                return name;
            }

            result = make_node(
                PEBBLE_C_NODE_MEMBER_EXPRESSION,
                member->location,
                member->lexeme
            );

            if (result == NULL) {
                pebble_c_node_destroy(name);
                return NULL;
            }

            add_child(result, name);
            return result;
        }
    }

    if (match(parser, PEBBLE_C_TOKEN_LEFT_PAREN)) {
        PebbleCNode *value = expression(parser);

        (void)consume(
            parser,
            PEBBLE_C_TOKEN_RIGHT_PAREN,
            "Expected ')' after expression."
        );

        return value;
    }

    if (at_line_break(parser) && continues_member(parser)) {
        char message[256];

        (void)snprintf(
            message,
            sizeof(message),
            "Unexpected '%s'. A member continues the expression above it, so "
            "it needs a name or a call before it.",
            peek(parser)->lexeme
        );

        report(parser, peek(parser), message);
        return NULL;
    }

    if (at_line_break(parser) && at_binary_operator(parser)) {
        char message[256];

        if (check(parser, PEBBLE_C_TOKEN_EQUAL)) {
            (void)snprintf(
                message,
                sizeof(message),
                "Unexpected '='. The '=' of an assignment stands on the line "
                "of the name it assigns to."
            );
        } else {
            (void)snprintf(
                message,
                sizeof(message),
                "Unexpected '%s'. A binary operator cannot start a line; it "
                "stands at the end of the line of the expression it "
                "continues.",
                peek(parser)->lexeme
            );
        }

        report(parser, peek(parser), message);
        return NULL;
    }

    report(parser, peek(parser), "Expected an expression.");
    return NULL;
}

PebbleCParseResult pebble_c_parse(const PebbleCTokenList *tokens)
{
    Parser parser = {0};
    PebbleCNode *root;
    PebbleCParseResult result;

    parser.tokens = tokens;

    {
        PebbleCSourceLocation location = {NULL, 1U, 1U};

        if (tokens != NULL && tokens->count > 0U) {
            location = tokens->items[0].location;
        }

        root = make_node(PEBBLE_C_NODE_PROGRAM, location, NULL);
    }

    if (root != NULL) {
        while (!at_end(&parser)) {
            size_t before = parser.current;
            add_child(root, declaration(&parser));

            if (parser.current == before) {
                synchronize(&parser);

                if (parser.current == before && !at_end(&parser)) {
                    (void)advance_token(&parser);
                }
            }
        }
    }

    result.root = root;
    result.diagnostics = parser.diagnostics;
    return result;
}

int pebble_c_parse_result_ok(const PebbleCParseResult *result)
{
    return result != NULL &&
           result->root != NULL &&
           result->diagnostics.count == 0U;
}

void pebble_c_parse_result_destroy(PebbleCParseResult *result)
{
    if (result == NULL) {
        return;
    }

    pebble_c_node_destroy(result->root);
    result->root = NULL;
    pebble_c_diagnostics_destroy(&result->diagnostics);
}
