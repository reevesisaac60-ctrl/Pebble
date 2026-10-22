#ifndef PEBBLE_C_AST_H
#define PEBBLE_C_AST_H

#include "pebble_c/source.h"

#include <stddef.h>

typedef enum {
    PEBBLE_C_NODE_PROGRAM,
    PEBBLE_C_NODE_FUNCTION,
    PEBBLE_C_NODE_PARAMETER,
    PEBBLE_C_NODE_BLOCK,
    PEBBLE_C_NODE_LET_DECLARATION,
    PEBBLE_C_NODE_VAR_DECLARATION,
    PEBBLE_C_NODE_TYPE_NAME,
    PEBBLE_C_NODE_IMPORT_DECLARATION,
    PEBBLE_C_NODE_ASSIGNMENT,
    PEBBLE_C_NODE_EXPRESSION_STATEMENT,
    PEBBLE_C_NODE_RETURN_STATEMENT,
    PEBBLE_C_NODE_IF_STATEMENT,
    PEBBLE_C_NODE_WHILE_STATEMENT,
    PEBBLE_C_NODE_BINARY_EXPRESSION,
    PEBBLE_C_NODE_UNARY_EXPRESSION,
    PEBBLE_C_NODE_CALL_EXPRESSION,
    PEBBLE_C_NODE_MEMBER_EXPRESSION,
    PEBBLE_C_NODE_NAME_EXPRESSION,
    PEBBLE_C_NODE_LITERAL_EXPRESSION
} PebbleCNodeKind;

typedef struct PebbleCNode {
    PebbleCNodeKind kind;
    PebbleCSourceLocation location;
    char *text;
    struct PebbleCNode **children;
    size_t child_count;
    size_t child_capacity;
    int exported;
    int local;
} PebbleCNode;

PebbleCNode *pebble_c_node_create(
    PebbleCNodeKind kind,
    PebbleCSourceLocation location,
    const char *text
);

int pebble_c_node_add(PebbleCNode *parent, PebbleCNode *child);
void pebble_c_node_destroy(PebbleCNode *node);
const char *pebble_c_node_kind_name(PebbleCNodeKind kind);

#endif
