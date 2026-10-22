#include "pebble_c/ast.h"

#include <stdlib.h>
#include <string.h>

static char *copy_text(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = malloc(length + 1U);
    if (copy != NULL) {
        memcpy(copy, text, length + 1U);
    }

    return copy;
}

PebbleCNode *pebble_c_node_create(
    PebbleCNodeKind kind,
    PebbleCSourceLocation location,
    const char *text
)
{
    PebbleCNode *node = calloc(1U, sizeof(*node));

    if (node == NULL) {
        return NULL;
    }

    node->kind = kind;
    node->location = location;

    if (text != NULL) {
        node->text = copy_text(text);
        if (node->text == NULL) {
            free(node);
            return NULL;
        }
    }

    return node;
}

int pebble_c_node_add(PebbleCNode *parent, PebbleCNode *child)
{
    PebbleCNode **grown;
    size_t capacity;

    if (parent == NULL || child == NULL) {
        return 0;
    }

    if (parent->child_count == parent->child_capacity) {
        capacity = parent->child_capacity == 0U
            ? 4U
            : parent->child_capacity * 2U;

        if (capacity < parent->child_capacity ||
            capacity > (size_t)-1 / sizeof(*parent->children)) {
            return 0;
        }

        grown = realloc(
            parent->children,
            capacity * sizeof(*parent->children)
        );

        if (grown == NULL) {
            return 0;
        }

        parent->children = grown;
        parent->child_capacity = capacity;
    }

    parent->children[parent->child_count] = child;
    ++parent->child_count;
    return 1;
}

void pebble_c_node_destroy(PebbleCNode *node)
{
    size_t index;

    if (node == NULL) {
        return;
    }

    for (index = 0U; index < node->child_count; ++index) {
        pebble_c_node_destroy(node->children[index]);
    }

    free(node->children);
    free(node->text);
    free(node);
}

const char *pebble_c_node_kind_name(PebbleCNodeKind kind)
{
    switch (kind) {
    case PEBBLE_C_NODE_PROGRAM: return "Program";
    case PEBBLE_C_NODE_FUNCTION: return "Function";
    case PEBBLE_C_NODE_PARAMETER: return "Parameter";
    case PEBBLE_C_NODE_BLOCK: return "Block";
    case PEBBLE_C_NODE_LET_DECLARATION: return "LetDeclaration";
    case PEBBLE_C_NODE_VAR_DECLARATION: return "VarDeclaration";
    case PEBBLE_C_NODE_TYPE_NAME: return "TypeName";
    case PEBBLE_C_NODE_IMPORT_DECLARATION: return "ImportDeclaration";
    case PEBBLE_C_NODE_ASSIGNMENT: return "Assignment";
    case PEBBLE_C_NODE_EXPRESSION_STATEMENT: return "ExpressionStatement";
    case PEBBLE_C_NODE_RETURN_STATEMENT: return "ReturnStatement";
    case PEBBLE_C_NODE_IF_STATEMENT: return "IfStatement";
    case PEBBLE_C_NODE_WHILE_STATEMENT: return "WhileStatement";
    case PEBBLE_C_NODE_BINARY_EXPRESSION: return "BinaryExpression";
    case PEBBLE_C_NODE_UNARY_EXPRESSION: return "UnaryExpression";
    case PEBBLE_C_NODE_CALL_EXPRESSION: return "CallExpression";
    case PEBBLE_C_NODE_MEMBER_EXPRESSION: return "MemberExpression";
    case PEBBLE_C_NODE_NAME_EXPRESSION: return "NameExpression";
    case PEBBLE_C_NODE_LITERAL_EXPRESSION: return "LiteralExpression";
    }

    return "Unknown";
}
