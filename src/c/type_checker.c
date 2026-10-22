#include "pebble_c/type_checker.h"
#include "pebble_c/package.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_TYPE "<error>"
#define MINIMUM_INTEGER_MAGNITUDE UINT64_C(9223372036854775808)

typedef struct {
    const char *name;
    const char *type;
    int mutable_value;
    int is_parameter;
    PebbleCSourceLocation location;
} Symbol;

typedef struct Scope {
    Symbol *symbols;
    size_t count;
    size_t capacity;
    struct Scope *parent;
} Scope;

typedef struct {
    const char *name;
    const char **parameters;
    size_t parameter_count;
    const char *return_type;
    PebbleCSourceLocation location;
    int exported;
} FunctionSignature;

typedef struct {
    const char *name;
    const PebbleCPackage *builtin;
} ImportedPackage;

typedef struct {
    PebbleCDiagnosticList diagnostics;
    Scope *scope;
    FunctionSignature *functions;
    size_t function_count;
    size_t function_capacity;
    ImportedPackage *imports;
    size_t import_count;
    size_t import_capacity;
    const char *return_type;
    char current_package[256];
    int inside_function;
} TypeChecker;

static int equal(const char *left, const char *right)
{
    return left != NULL && right != NULL && strcmp(left, right) == 0;
}

static void report(TypeChecker *checker,
                   PebbleCSourceLocation location,
                   const char *format, ...)
{
    va_list args;
    va_list copy_args;
    int length;
    char *message;
    PebbleCDiagnostic *items;
    size_t capacity;

    va_start(args, format);
    va_copy(copy_args, args);
    length = vsnprintf(NULL, 0, format, copy_args);
    va_end(copy_args);

    if (length < 0) {
        va_end(args);
        return;
    }

    message = malloc((size_t)length + 1);
    if (message == NULL) {
        va_end(args);
        return;
    }

    (void)vsnprintf(message, (size_t)length + 1, format, args);
    va_end(args);

    if (checker->diagnostics.count == checker->diagnostics.capacity) {
        capacity = checker->diagnostics.capacity == 0
            ? 8
            : checker->diagnostics.capacity * 2;
        items = realloc(checker->diagnostics.items,
                        capacity * sizeof(*items));
        if (items == NULL) {
            free(message);
            return;
        }
        checker->diagnostics.items = items;
        checker->diagnostics.capacity = capacity;
    }

    checker->diagnostics.items[checker->diagnostics.count].location = location;
    checker->diagnostics.items[checker->diagnostics.count].message = message;
    checker->diagnostics.count++;
}

static Scope *push_scope(TypeChecker *checker)
{
    Scope *scope = calloc(1, sizeof(*scope));

    if (scope != NULL) {
        scope->parent = checker->scope;
        checker->scope = scope;
    }
    return scope;
}

static void pop_scope(TypeChecker *checker)
{
    Scope *scope = checker->scope;

    if (scope == NULL) {
        return;
    }

    checker->scope = scope->parent;
    free(scope->symbols);
    free(scope);
}

static Symbol *find_symbol(TypeChecker *checker, const char *name)
{
    Scope *scope;

    for (scope = checker->scope; scope != NULL; scope = scope->parent) {
        size_t index;

        for (index = 0; index < scope->count; index++) {
            if (equal(scope->symbols[index].name, name)) {
                return &scope->symbols[index];
            }
        }
    }

    return NULL;
}

static int declare_symbol(TypeChecker *checker,
                          const char *name,
                          const char *type,
                          int mutable_value,
                          int is_parameter,
                          PebbleCSourceLocation location)
{
    Scope *scope = checker->scope;
    Symbol *symbols;
    size_t index;
    size_t capacity;

    if (scope == NULL) {
        return 0;
    }

    for (index = 0; index < scope->count; index++) {
        if (equal(scope->symbols[index].name, name)) {
            return -1;
        }
    }

    if (scope->count == scope->capacity) {
        capacity = scope->capacity == 0 ? 8 : scope->capacity * 2;
        symbols = realloc(scope->symbols, capacity * sizeof(*symbols));
        if (symbols == NULL) {
            return 0;
        }
        scope->symbols = symbols;
        scope->capacity = capacity;
    }

    scope->symbols[scope->count].name = name;
    scope->symbols[scope->count].type = type;
    scope->symbols[scope->count].mutable_value = mutable_value;
    scope->symbols[scope->count].is_parameter = is_parameter;
    scope->symbols[scope->count].location = location;
    scope->count++;
    return 1;
}

static const FunctionSignature *find_function(const TypeChecker *checker,
                                               const char *name)
{
    size_t index;

    for (index = 0; index < checker->function_count; index++) {
        if (equal(checker->functions[index].name, name)) {
            return &checker->functions[index];
        }
    }
    return NULL;
}

static const ImportedPackage *find_import(const TypeChecker *checker,
                                           const char *name)
{
    size_t index;

    if (name == NULL) {
        return NULL;
    }

    for (index = 0U; index < checker->import_count; ++index) {
        if (equal(checker->imports[index].name, name)) {
            return &checker->imports[index];
        }
    }

    return NULL;
}

static int add_import(TypeChecker *checker,
                      const char *name,
                      const PebbleCPackage *package)
{
    ImportedPackage *grown;
    size_t capacity;

    if (checker->import_count == checker->import_capacity) {
        capacity = checker->import_capacity == 0U ? 8U
            : checker->import_capacity * 2U;
        grown = realloc(
            checker->imports,
            capacity * sizeof(*checker->imports)
        );

        if (grown == NULL) {
            return 0;
        }

        checker->imports = grown;
        checker->import_capacity = capacity;
    }

    checker->imports[checker->import_count].name = name;
    checker->imports[checker->import_count].builtin = package;
    ++checker->import_count;
    return 1;
}

static int package_of(const char *name, char *buffer, size_t size)
{
    size_t index;

    if (name == NULL) {
        return 0;
    }

    for (index = 0U; name[index] != '\0'; ++index) {
        if (name[index] == '.') {
            if (index >= size) {
                return 0;
            }
            memcpy(buffer, name, index);
            buffer[index] = '\0';
            return 1;
        }
    }

    return 0;
}

static int resolve_package_call(TypeChecker *checker,
                                const PebbleCNode *callee,
                                const PebbleCPackageFunction **package_function,
                                const FunctionSignature **local_function)
{
    const ImportedPackage *imported;
    const char *package_name;
    char qualified[256];

    *package_function = NULL;
    *local_function = NULL;

    if (callee->child_count != 1 ||
        callee->children[0]->kind != PEBBLE_C_NODE_NAME_EXPRESSION) {
        report(checker, callee->location, "Invalid package member access.");
        return 0;
    }

    package_name = callee->children[0]->text;
    imported = find_import(checker, package_name);

    if (imported == NULL) {
        report(checker, callee->location, "Unknown package '%s'.",
               package_name);
        return 0;
    }

    if (imported->builtin != NULL) {
        *package_function = pebble_c_find_package_function(
            imported->builtin, callee->text);

        if (*package_function == NULL) {
            report(checker, callee->location,
                   "Package '%s' has no function '%s'.",
                   package_name, callee->text);
            return 0;
        }

        return 1;
    }

    if (snprintf(qualified, sizeof(qualified), "%s.%s", package_name,
                 callee->text) >= (int)sizeof(qualified)) {
        report(checker, callee->location,
               "Package function name is too long.");
        return 0;
    }

    *local_function = find_function(checker, qualified);

    if (*local_function == NULL || !(*local_function)->exported) {
        report(checker, callee->location,
               "Package '%s' has no function '%s'.",
               package_name, callee->text);
        return 0;
    }

    return 1;
}

static int is_integer_literal(const char *text)
{
    size_t index;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    for (index = 0U; text[index] != '\0'; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return 0;
        }
    }

    return 1;
}

static int parse_integer_literal(const char *text, uint64_t *value)
{
    uint64_t result = 0U;
    size_t index;

    if (!is_integer_literal(text)) {
        return 0;
    }

    for (index = 0U; text[index] != '\0'; ++index) {
        const uint64_t digit = (uint64_t)(text[index] - '0');

        if (result > (UINT64_MAX - digit) / UINT64_C(10)) {
            return 0;
        }

        result = result * UINT64_C(10) + digit;
    }

    *value = result;
    return 1;
}

static int known_type(const char *type)
{
    return equal(type, "Int") || equal(type, "Float") ||
           equal(type, "String") || equal(type, "Bool") ||
           equal(type, "Void");
}

static int numeric_type(const char *type)
{
    return equal(type, "Int") || equal(type, "Float");
}

static int assignable(const char *destination, const char *source)
{
    if (equal(destination, ERROR_TYPE) || equal(source, ERROR_TYPE)) {
        return 1;
    }
    if (equal(destination, source)) {
        return 1;
    }
    return equal(destination, "Float") && equal(source, "Int");
}

static const char *declared_type(const PebbleCNode *node)
{
    size_t index;

    for (index = 0; index < node->child_count; index++) {
        if (node->children[index]->kind == PEBBLE_C_NODE_TYPE_NAME) {
            return node->children[index]->text;
        }
    }
    return NULL;
}

static const char *check_expression(TypeChecker *checker,
                                    const PebbleCNode *expression);

static void check_statement(TypeChecker *checker, const PebbleCNode *node);

static void check_block(TypeChecker *checker, const PebbleCNode *block)
{
    size_t index;

    if (push_scope(checker) == NULL) {
        report(checker, block->location, "Unable to allocate a variable scope.");
        return;
    }

    for (index = 0; index < block->child_count; index++) {
        check_statement(checker, block->children[index]);
    }

    pop_scope(checker);
}

static void check_statement(TypeChecker *checker, const PebbleCNode *node)
{
    size_t index;

    switch (node->kind) {
    case PEBBLE_C_NODE_BLOCK:
        check_block(checker, node);
        return;

    case PEBBLE_C_NODE_LET_DECLARATION:
    case PEBBLE_C_NODE_VAR_DECLARATION: {
        int mutable_value = node->kind == PEBBLE_C_NODE_VAR_DECLARATION;
        const char *annotation = declared_type(node);
        const char *initializer_type = NULL;
        const char *type;
        int declared;

        for (index = 0; index < node->child_count; index++) {
            if (node->children[index]->kind != PEBBLE_C_NODE_TYPE_NAME) {
                initializer_type = check_expression(checker,
                                                    node->children[index]);
                break;
            }
        }

        if (initializer_type == NULL) {
            initializer_type = ERROR_TYPE;
        }
        type = annotation != NULL ? annotation : initializer_type;

        if (annotation != NULL && !known_type(annotation)) {
            report(checker, node->location, "Unknown type '%s'.", annotation);
            type = ERROR_TYPE;
        }

        if (equal(type, "Void")) {
            report(checker, node->location,
                   "Variables cannot have type 'Void'.");
            type = ERROR_TYPE;
        }

        if (type == NULL || type[0] == '\0') {
            report(checker, node->location, "Could not infer variable type.");
            type = ERROR_TYPE;
        }

        if (annotation != NULL && !assignable(annotation, initializer_type)) {
            report(checker, node->location,
                   "Cannot initialise '%s' of type '%s' with a value of type '%s'.",
                   node->text, annotation, initializer_type);
        }

        {
            int shadow_parameter = 0;
            Symbol *existing = find_symbol(checker, node->text);

            if (existing != NULL && existing->is_parameter) {
                shadow_parameter = 1;
            }

            if (shadow_parameter) {
                report(checker, node->location,
                       "Cannot shadow parameter '%s'.", node->text);
            } else {
                declared = declare_symbol(checker, node->text, type,
                                          mutable_value, 0, node->location);
                if (declared < 0) {
                    report(checker, node->location,
                           "Variable '%s' is already declared in this scope.",
                           node->text);
                } else if (declared == 0) {
                    report(checker, node->location,
                           "Unable to allocate variable '%s'.", node->text);
                }
            }
        }
        return;
    }

    case PEBBLE_C_NODE_IMPORT_DECLARATION:
        report(checker, node->location,
               "Package imports are only allowed at the top level of a file.");
        return;

    case PEBBLE_C_NODE_ASSIGNMENT: {
        Symbol *symbol = find_symbol(checker, node->text);
        const char *value_type = ERROR_TYPE;

        if (node->child_count > 0) {
            value_type = check_expression(checker, node->children[0]);
        }

        if (symbol == NULL) {
            report(checker, node->location, "Undeclared variable '%s'.",
                   node->text);
            return;
        }

        if (!symbol->mutable_value) {
            report(checker, node->location,
                   "Cannot assign to immutable variable '%s'.", node->text);
        }

        if (!assignable(symbol->type, value_type)) {
            report(checker, node->location,
                   "Cannot assign a value of type '%s' to variable '%s' of type '%s'.",
                   value_type, node->text, symbol->type);
        }
        return;
    }

    case PEBBLE_C_NODE_EXPRESSION_STATEMENT:
        if (node->child_count > 0) {
            const PebbleCNode *expression = node->children[0];

            if (expression->kind == PEBBLE_C_NODE_ASSIGNMENT) {
                check_statement(checker, expression);
            } else {
                (void)check_expression(checker, expression);
            }
        }
        return;

    case PEBBLE_C_NODE_IF_STATEMENT: {
        const char *condition_type;

        if (node->child_count < 2) {
            report(checker, node->location, "Malformed if statement.");
            return;
        }

        condition_type = check_expression(checker, node->children[0]);
        if (!equal(condition_type, "Bool") &&
            !equal(condition_type, ERROR_TYPE)) {
            report(checker, node->children[0]->location,
                   "If condition must have type 'Bool', not '%s'.",
                   condition_type);
        }

        check_statement(checker, node->children[1]);
        if (node->child_count >= 3) {
            check_statement(checker, node->children[2]);
        }
        return;
    }

    case PEBBLE_C_NODE_WHILE_STATEMENT: {
        const char *condition_type;

        if (node->child_count != 2) {
            report(checker, node->location, "Malformed while statement.");
            return;
        }

        condition_type = check_expression(checker, node->children[0]);
        if (!equal(condition_type, "Bool") &&
            !equal(condition_type, ERROR_TYPE)) {
            report(checker, node->children[0]->location,
                   "While condition must have type 'Bool', not '%s'.",
                   condition_type);
        }

        check_statement(checker, node->children[1]);
        return;
    }

    case PEBBLE_C_NODE_RETURN_STATEMENT: {
        const char *actual_type = "Void";

        if (node->child_count > 0) {
            actual_type = check_expression(checker, node->children[0]);
        }

        if (!checker->inside_function) {
            report(checker, node->location,
                   "'return' is only valid inside a function.");
        } else if (!assignable(checker->return_type, actual_type)) {
            report(checker, node->location,
                   "Return type mismatch: expected '%s' but got '%s'.",
                   checker->return_type, actual_type);
        }
        return;
    }

    default:
        report(checker, node->location,
               "Unexpected syntax node in statement position.");
        return;
    }
}

static const char *check_expression(TypeChecker *checker,
                                    const PebbleCNode *expression)
{
    size_t index;

    switch (expression->kind) {
    case PEBBLE_C_NODE_LITERAL_EXPRESSION:
        if (equal(expression->text, "true") ||
            equal(expression->text, "false")) {
            return "Bool";
        }
        if (equal(expression->text, "null")) {
            return "Null";
        }
        if (expression->text != NULL && expression->text[0] == '"') {
            return "String";
        }
        if (is_integer_literal(expression->text)) {
            uint64_t magnitude = 0U;

            if (!parse_integer_literal(expression->text, &magnitude) ||
                magnitude >= MINIMUM_INTEGER_MAGNITUDE) {
                report(checker, expression->location,
                       "Integer literal '%s' is out of range for type 'Int'.",
                       expression->text != NULL ? expression->text : "");
                return ERROR_TYPE;
            }
        }
        return expression->text != NULL &&
               strchr(expression->text, '.') != NULL ? "Float" : "Int";

    case PEBBLE_C_NODE_NAME_EXPRESSION:
        if (equal(expression->text, "print")) {
            return "Builtin";
        }
        {
            Symbol *symbol = find_symbol(checker, expression->text);
            if (symbol != NULL) {
                return symbol->type;
            }
        }
        if (find_import(checker, expression->text) != NULL) {
            return "Package";
        }
        if (find_function(checker, expression->text) != NULL) {
            return "Function";
        }
        report(checker, expression->location, "Undeclared name '%s'.",
               expression->text);
        return ERROR_TYPE;

    case PEBBLE_C_NODE_MEMBER_EXPRESSION:
        report(checker, expression->location,
               "Package functions can only be used in a direct call.");
        return ERROR_TYPE;

    case PEBBLE_C_NODE_UNARY_EXPRESSION: {
        const char *operand;

        if (expression->child_count == 0) {
            return ERROR_TYPE;
        }

        if (equal(expression->text, "-") &&
            expression->children[0]->kind == PEBBLE_C_NODE_LITERAL_EXPRESSION &&
            is_integer_literal(expression->children[0]->text)) {
            uint64_t magnitude = 0U;

            if (parse_integer_literal(expression->children[0]->text,
                                      &magnitude) &&
                magnitude == MINIMUM_INTEGER_MAGNITUDE) {
                return "Int";
            }
        }

        operand = check_expression(checker, expression->children[0]);
        if (equal(expression->text, "!")) {
            if (!equal(operand, "Bool") && !equal(operand, ERROR_TYPE)) {
                report(checker, expression->location,
                       "Operator '!' requires a Bool operand.");
            }
            return "Bool";
        }

        if (!numeric_type(operand) && !equal(operand, ERROR_TYPE)) {
            report(checker, expression->location,
                   "Unary '-' requires a numeric operand.");
            return ERROR_TYPE;
        }
        return operand;
    }

    case PEBBLE_C_NODE_BINARY_EXPRESSION: {
        const char *left;
        const char *right;
        const char *op = expression->text;

        if (expression->child_count != 2) {
            return ERROR_TYPE;
        }

        left = check_expression(checker, expression->children[0]);
        right = check_expression(checker, expression->children[1]);

        if (equal(left, ERROR_TYPE) || equal(right, ERROR_TYPE)) {
            return ERROR_TYPE;
        }

        if (equal(op, "&&") || equal(op, "||")) {
            if (!equal(left, "Bool") || !equal(right, "Bool")) {
                report(checker, expression->location,
                       "Logical operators require Bool operands.");
                return ERROR_TYPE;
            }
            return "Bool";
        }

        if (equal(op, "==") || equal(op, "!=")) {
            if (!equal(left, right) &&
                !(numeric_type(left) && numeric_type(right))) {
                report(checker, expression->location,
                       "Cannot compare values of type '%s' and '%s'.",
                       left, right);
                return ERROR_TYPE;
            }
            return "Bool";
        }

        if (equal(op, "<") || equal(op, "<=") ||
            equal(op, ">") || equal(op, ">=")) {
            if (!numeric_type(left) || !numeric_type(right)) {
                report(checker, expression->location,
                       "Comparison operators require numeric operands.");
                return ERROR_TYPE;
            }
            return "Bool";
        }

        if (equal(op, "+") && equal(left, "String") &&
            equal(right, "String")) {
            return "String";
        }

        if (!numeric_type(left) || !numeric_type(right)) {
            report(checker, expression->location,
                   "Operator '%s' requires numeric operands (or two Strings for '+').",
                   op);
            return ERROR_TYPE;
        }

        if (equal(op, "%") &&
            (!equal(left, "Int") || !equal(right, "Int"))) {
            report(checker, expression->location,
                   "Operator '%%' requires Int operands.");
            return ERROR_TYPE;
        }

        return equal(left, "Float") || equal(right, "Float")
            ? "Float" : "Int";
    }

    case PEBBLE_C_NODE_CALL_EXPRESSION: {
        const PebbleCNode *callee;
        const FunctionSignature *signature;
        const char *name;
        size_t argument_count;

        if (expression->child_count == 0) {
            return ERROR_TYPE;
        }

        argument_count = expression->child_count - 1;
        callee = expression->children[0];

        if (callee->kind == PEBBLE_C_NODE_MEMBER_EXPRESSION) {
            const PebbleCPackageFunction *package_function = NULL;
            const FunctionSignature *local_function = NULL;
            char qualified_name[256];

            if (!resolve_package_call(checker, callee, &package_function,
                                      &local_function)) {
                for (index = 1; index < expression->child_count; index++) {
                    (void)check_expression(checker, expression->children[index]);
                }
                return ERROR_TYPE;
            }

            (void)snprintf(qualified_name, sizeof(qualified_name), "%s.%s",
                           callee->children[0]->text, callee->text);

            if (package_function != NULL) {
                if (argument_count != package_function->parameter_count) {
                    report(checker, expression->location,
                           "Function '%s' expects %zu argument(s), but got %zu.",
                           qualified_name, package_function->parameter_count,
                           argument_count);
                }

                for (index = 1; index < expression->child_count; index++) {
                    const char *actual = check_expression(
                        checker, expression->children[index]);
                    size_t parameter_index = index - 1;

                    if (parameter_index < package_function->parameter_count) {
                        const char *expected =
                            package_function->parameters[parameter_index];
                        if (!assignable(expected, actual)) {
                            report(checker,
                                   expression->children[index]->location,
                                   "Argument %zu of '%s' expects '%s', but got "
                                   "'%s'.",
                                   index, qualified_name, expected, actual);
                        }
                    }
                }

                return package_function->return_type;
            }

            if (argument_count != local_function->parameter_count) {
                report(checker, expression->location,
                       "Function '%s' expects %zu argument(s), but got %zu.",
                       qualified_name, local_function->parameter_count,
                       argument_count);
            }

            for (index = 1; index < expression->child_count; index++) {
                const char *actual =
                    check_expression(checker, expression->children[index]);
                size_t parameter_index = index - 1;

                if (parameter_index < local_function->parameter_count) {
                    const char *expected =
                        local_function->parameters[parameter_index];
                    if (!assignable(expected, actual)) {
                        report(checker, expression->children[index]->location,
                               "Argument %zu of '%s' expects '%s', but got '%s'.",
                               index, qualified_name, expected, actual);
                    }
                }
            }

            return local_function->return_type;
        }

        if (callee->kind != PEBBLE_C_NODE_NAME_EXPRESSION) {
            report(checker, expression->location,
                   "Only named functions can be called currently.");
            for (index = 1; index < expression->child_count; index++) {
                (void)check_expression(checker, expression->children[index]);
            }
            return ERROR_TYPE;
        }

        name = callee->text;

        if (equal(name, "print")) {
            for (index = 1; index < expression->child_count; index++) {
                (void)check_expression(checker, expression->children[index]);
            }
            return "Void";
        }

        signature = NULL;
        if (checker->current_package[0] != '\0') {
            char qualified[256];

            if (snprintf(qualified, sizeof(qualified), "%s.%s",
                         checker->current_package, name) <
                (int)sizeof(qualified)) {
                signature = find_function(checker, qualified);
            }
        }

        if (signature == NULL) {
            signature = find_function(checker, name);
        }

        if (signature == NULL) {
            report(checker, expression->location,
                   "Call to undeclared function '%s'.", name);
            for (index = 1; index < expression->child_count; index++) {
                (void)check_expression(checker, expression->children[index]);
            }
            return ERROR_TYPE;
        }

        if (argument_count != signature->parameter_count) {
            report(checker, expression->location,
                   "Function '%s' expects %zu argument(s), but got %zu.",
                   name, signature->parameter_count, argument_count);
        }

        for (index = 1; index < expression->child_count; index++) {
            const char *actual =
                check_expression(checker, expression->children[index]);
            size_t parameter_index = index - 1;

            if (parameter_index < signature->parameter_count) {
                const char *expected = signature->parameters[parameter_index];
                if (!assignable(expected, actual)) {
                    report(checker, expression->children[index]->location,
                           "Argument %zu of '%s' expects '%s', but got '%s'.",
                           index, name, expected, actual);
                }
            }
        }
        return signature->return_type;
    }

    default:
        report(checker, expression->location,
               "Unexpected syntax node in expression position.");
        return ERROR_TYPE;
    }
}

static int add_function(TypeChecker *checker, const PebbleCNode *node)
{
    FunctionSignature signature;
    FunctionSignature *functions;
    size_t index;
    size_t capacity;
    size_t parameter_count = 0;
    size_t parameter_index = 0;

    for (index = 0; index < node->child_count; index++) {
        if (node->children[index]->kind == PEBBLE_C_NODE_PARAMETER) {
            parameter_count++;
        }
    }

    for (index = 0; index < checker->function_count; index++) {
        if (equal(checker->functions[index].name, node->text)) {
            report(checker, node->location,
                   "Function '%s' is already declared.", node->text);
            return 1;
        }
    }

    signature.name = node->text;
    signature.location = node->location;
    signature.return_type = "Void";
    signature.exported = node->exported;
    signature.parameter_count = parameter_count;
    signature.parameters = parameter_count == 0
        ? NULL
        : calloc(parameter_count, sizeof(*signature.parameters));

    if (parameter_count > 0 && signature.parameters == NULL) {
        return 0;
    }

    for (index = 0; index < node->child_count; index++) {
        const PebbleCNode *child = node->children[index];

        if (child->kind == PEBBLE_C_NODE_PARAMETER) {
            const char *type = child->child_count > 0
                ? child->children[0]->text : ERROR_TYPE;

            if (child->child_count == 0) {
                report(checker, child->location,
                       "Parameter '%s' needs an explicit type annotation.",
                       child->text);
            } else if (!known_type(type) || equal(type, "Void")) {
                report(checker, child->children[0]->location,
                       "Unknown or invalid parameter type '%s'.", type);
            }
            signature.parameters[parameter_index++] = type;
        } else if (child->kind == PEBBLE_C_NODE_TYPE_NAME) {
            signature.return_type = child->text;
            if (!known_type(child->text)) {
                report(checker, child->location,
                       "Unknown return type '%s'.", child->text);
            }
        }
    }

    if (checker->function_count == checker->function_capacity) {
        capacity = checker->function_capacity == 0
            ? 8 : checker->function_capacity * 2;
        functions = realloc(checker->functions,
                            capacity * sizeof(*functions));
        if (functions == NULL) {
            free(signature.parameters);
            return 0;
        }
        checker->functions = functions;
        checker->function_capacity = capacity;
    }

    checker->functions[checker->function_count++] = signature;
    return 1;
}

static int always_returns(const PebbleCNode *node)
{
    size_t index;

    if (node == NULL) {
        return 0;
    }

    if (node->kind == PEBBLE_C_NODE_RETURN_STATEMENT) {
        return 1;
    }

    if (node->kind == PEBBLE_C_NODE_BLOCK) {
        for (index = 0U; index < node->child_count; ++index) {
            if (always_returns(node->children[index]) != 0) {
                return 1;
            }
        }

        return 0;
    }

    if (node->kind == PEBBLE_C_NODE_IF_STATEMENT) {
        if (node->child_count < 3U) {
            return 0;
        }

        return always_returns(node->children[1]) != 0 &&
               always_returns(node->children[2]) != 0;
    }

    return 0;
}

static void check_function(TypeChecker *checker, const PebbleCNode *function)
{
    const FunctionSignature *signature = find_function(checker, function->text);
    size_t index;

    if (signature == NULL) {
        return;
    }

    checker->inside_function = 1;
    checker->return_type = signature->return_type;

    if (!package_of(function->text, checker->current_package,
                    sizeof(checker->current_package))) {
        checker->current_package[0] = '\0';
    }

    if (push_scope(checker) == NULL) {
        report(checker, function->location,
               "Unable to allocate a function scope.");
        checker->inside_function = 0;
        return;
    }

    for (index = 0; index < function->child_count; index++) {
        const PebbleCNode *parameter = function->children[index];
        int declared;

        if (parameter->kind != PEBBLE_C_NODE_PARAMETER) {
            continue;
        }

        declared = declare_symbol(
            checker, parameter->text,
            parameter->child_count > 0
                ? parameter->children[0]->text : ERROR_TYPE,
            0, 1, parameter->location);

        if (declared < 0) {
            report(checker, parameter->location,
                   "Parameter '%s' is already declared.", parameter->text);
        } else if (declared == 0) {
            report(checker, parameter->location,
                   "Unable to allocate parameter '%s'.", parameter->text);
        }
    }

    for (index = 0; index < function->child_count; index++) {
        if (function->children[index]->kind == PEBBLE_C_NODE_BLOCK) {
            check_block(checker, function->children[index]);

            if (strcmp(signature->return_type, "Void") != 0 &&
                strcmp(signature->return_type, ERROR_TYPE) != 0 &&
                always_returns(function->children[index]) == 0) {
                report(checker, function->location,
                       "The function '%s' must return a value.", function->text);
            }
        }
    }

    pop_scope(checker);
    checker->inside_function = 0;
    checker->return_type = "Void";
    checker->current_package[0] = '\0';
}

static void collect_imports(TypeChecker *checker, const PebbleCNode *program)
{
    size_t index;

    for (index = 0U; index < program->child_count; ++index) {
        const PebbleCNode *child = program->children[index];
        const PebbleCPackage *package;

        if (child->kind != PEBBLE_C_NODE_IMPORT_DECLARATION) {
            continue;
        }

        if (find_import(checker, child->text) != NULL) {
            report(checker, child->location,
                   "Package '%s' is imported more than once.", child->text);
            continue;
        }

        if (child->local) {
            if (!add_import(checker, child->text, NULL)) {
                report(checker, child->location,
                       "Unable to allocate package '%s'.", child->text);
            }
            continue;
        }

        package = pebble_c_find_builtin_package(child->text);

        if (package == NULL) {
            report(checker, child->location, "Unknown package '%s'.",
                   child->text);
            continue;
        }

        if (!add_import(checker, child->text, package)) {
            report(checker, child->location,
                   "Unable to allocate package '%s'.", child->text);
        }
    }
}

PebbleCTypeCheckResult pebble_c_type_check(const PebbleCNode *program)
{
    TypeChecker checker = {0};
    PebbleCTypeCheckResult result = {0};
    size_t index;

    if (program == NULL || program->kind != PEBBLE_C_NODE_PROGRAM) {
        report(&checker, (PebbleCSourceLocation){NULL, 1, 1},
               "Expected a program syntax node.");
        result.diagnostics = checker.diagnostics;
        return result;
    }

    collect_imports(&checker, program);

    for (index = 0; index < program->child_count; index++) {
        const PebbleCNode *child = program->children[index];

        if (child->kind == PEBBLE_C_NODE_FUNCTION) {
            if (!add_function(&checker, child)) {
                report(&checker, child->location,
                       "Unable to allocate function signature.");
            }
        }
    }

    for (index = 0; index < program->child_count; index++) {
        const PebbleCNode *child = program->children[index];

        if (child->kind == PEBBLE_C_NODE_FUNCTION) {
            check_function(&checker, child);
        } else if (child->kind == PEBBLE_C_NODE_IMPORT_DECLARATION) {
            continue;
        } else {
            report(&checker, child->location,
                   "Only function declarations and package imports are allowed "
                   "at the top level.");
        }
    }

    while (checker.scope != NULL) {
        pop_scope(&checker);
    }

    for (index = 0; index < checker.function_count; index++) {
        free(checker.functions[index].parameters);
    }
    free(checker.functions);
    free(checker.imports);

    result.diagnostics = checker.diagnostics;
    return result;
}

int pebble_c_type_check_result_ok(const PebbleCTypeCheckResult *result)
{
    return result != NULL && result->diagnostics.count == 0;
}

void pebble_c_type_check_result_destroy(PebbleCTypeCheckResult *result)
{
    if (result == NULL) {
        return;
    }
    pebble_c_diagnostics_destroy(&result->diagnostics);
}
