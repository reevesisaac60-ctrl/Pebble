#include "pebble/type_checker.hpp"

#include <cstdint>
#include <utility>

namespace pebble {

const std::vector<std::string_view>& builtinTypeNames() noexcept {
    static const std::vector<std::string_view> names = {
        "Int", "Float", "String", "Bool", "Void"
    };
    return names;
}

const std::vector<std::string_view>& builtinFunctionNames() noexcept {
    static const std::vector<std::string_view> names = {"print"};
    return names;
}

bool isBuiltinFunction(std::string_view name) noexcept {
    for (const std::string_view builtin : builtinFunctionNames()) {
        if (builtin == name) return true;
    }
    return false;
}

namespace {

constexpr unsigned long long kMinimumIntegerMagnitude = 9223372036854775808ULL;

bool isIntegerLiteral(const std::string& text) {
    if (text.empty()) {
        return false;
    }

    for (const char character : text) {
        if (character < '0' || character > '9') {
            return false;
        }
    }

    return true;
}

bool parseIntegerLiteral(
    const std::string& text,
    unsigned long long& value
) {
    if (text.empty()) {
        return false;
    }

    unsigned long long result = 0;

    for (const char character : text) {
        if (character < '0' || character > '9') {
            return false;
        }

        const unsigned long long digit =
            static_cast<unsigned long long>(character - '0');

        if (result > (UINT64_MAX - digit) / 10ULL) {
            return false;
        }

        result = result * 10ULL + digit;
    }

    value = result;
    return true;
}

bool alwaysReturns(const Node& node) {
    if (node.kind == NodeKind::ReturnStatement) {
        return true;
    }

    if (node.kind == NodeKind::Block) {
        for (const auto& child : node.children) {
            if (alwaysReturns(*child)) {
                return true;
            }
        }

        return false;
    }

    if (node.kind == NodeKind::IfStatement) {
        if (node.children.size() < 3U) {
            return false;
        }

        return alwaysReturns(*node.children[1]) &&
               alwaysReturns(*node.children[2]);
    }

    return false;
}

}

TypeCheckResult TypeChecker::check(const Node& program) const {
    TypeChecker checker;
    checker.collectImports(program);
    checker.collectFunctions(program);

    for (const auto& child : program.children) {
        if (child->kind == NodeKind::Function) {
            checker.checkFunction(*child);
        } else if (child->kind == NodeKind::ImportDeclaration) {
            continue;
        } else {
            checker.report(
                child->location,
                "Only function declarations and package imports are allowed "
                "at the top level."
            );
        }
    }

    return {std::move(checker.diagnostics_)};
}

void TypeChecker::collectImports(const Node& program) {
    for (const auto& child : program.children) {
        if (child->kind != NodeKind::ImportDeclaration) {
            continue;
        }

        if (imports_.contains(child->text)) {
            report(
                child->location,
                "Package '" + child->text + "' is imported more than once."
            );
            continue;
        }

        if (child->local) {
            imports_.emplace(
                child->text,
                ImportedPackage{nullptr, child.get()}
            );
            continue;
        }

        const Package* package = findBuiltinPackage(child->text);

        if (package == nullptr) {
            report(
                child->location,
                "Unknown package '" + child->text + "'."
            );
            continue;
        }

        imports_.emplace(
            child->text,
            ImportedPackage{package, child.get()}
        );
    }
}

const TypeChecker::ImportedPackage* TypeChecker::findImport(
    const std::string& name
) const {
    const auto found = imports_.find(name);

    return found == imports_.end() ? nullptr : &found->second;
}

std::string TypeChecker::packageOf(const std::string& name) const {
    const std::size_t separator = name.find('.');

    if (separator == std::string::npos) {
        return {};
    }

    return name.substr(0, separator);
}

bool TypeChecker::resolvePackageCall(
    const Node& callee,
    PackageCall& call
) {
    if (callee.children.size() != 1 ||
        callee.children.front()->kind != NodeKind::NameExpression) {
        report(callee.location, "Invalid package member access.");
        return false;
    }

    const std::string& packageName = callee.children.front()->text;
    const ImportedPackage* imported = findImport(packageName);

    if (imported == nullptr) {
        report(
            callee.location,
            "Unknown package '" + packageName + "'."
        );
        return false;
    }

    if (imported->builtin != nullptr) {
        const PackageFunction* function =
            findPackageFunction(*imported->builtin, callee.text);

        if (function == nullptr) {
            report(
                callee.location,
                "Package '" + packageName + "' has no function '" +
                    callee.text + "'."
            );
            return false;
        }

        call.parameters = function->parameters;
        call.returnType = function->returnType;
        return true;
    }

    const FunctionSignature* signature =
        findFunction(packageName + "." + callee.text);

    if (signature == nullptr || !signature->exported) {
        report(
            callee.location,
            "Package '" + packageName + "' has no function '" +
                callee.text + "'."
        );
        return false;
    }

    call.parameters = signature->parameters;
    call.returnType = signature->returnType;
    return true;
}

void TypeChecker::report(
    const SourceLocation& location,
    std::string message
) {
    diagnostics_.push_back({location, std::move(message)});
}

void TypeChecker::collectFunctions(const Node& program) {
    for (const auto& child : program.children) {
        if (child->kind != NodeKind::Function) {
            continue;
        }

        const Node& function = *child;

        if (functions_.contains(function.text)) {
            report(
                function.location,
                "Function '" + function.text + "' is already declared."
            );
            continue;
        }

        FunctionSignature signature;
        signature.location = function.location;
        signature.exported = function.exported;

        for (const auto& item : function.children) {
            if (item->kind == NodeKind::Parameter) {
            if (item->children.empty()) {
                signature.parameters.emplace_back("<infer>");
            } else {
                    const std::string type = item->children.front()->text;
                    signature.parameters.push_back(type);

                    if (!isKnownType(type) || type == "Void") {
                        report(
                            item->children.front()->location,
                            "Unknown or invalid parameter type '" + type + "'."
                        );
                    }
                }
            } else if (item->kind == NodeKind::TypeName) {
                signature.returnType = item->text;

                if (!isKnownType(item->text)) {
                    report(
                        item->location,
                        "Unknown return type '" + item->text + "'."
                    );
                }
            }
        }

        functions_.emplace(function.text, std::move(signature));
    }
}void TypeChecker::checkFunction(const Node& function) {
    const auto found = functions_.find(function.text);

    if (found == functions_.end()) {
        return;
    }

    insideFunction_ = true;
    currentReturnType_ = found->second.returnType;
    currentPackage_ = packageOf(function.text);

    scopes_.clear();
    pushScope();

    for (const auto& item : function.children) {
        if (item->kind != NodeKind::Parameter) {
            continue;
        }

        std::string type = "<error>";

        if (!item->children.empty()) {
            type = item->children.front()->text;
        }

        if (scopes_.back().contains(item->text)) {
            report(
                item->location,
                "Parameter '" + item->text + "' is already declared."
            );
        } else {
            scopes_.back().emplace(
                item->text,
                Symbol{type, false, true, item->location}
            );
        }
    }

    for (const auto& item : function.children) {
        if (item->kind == NodeKind::Block) {
            checkBlock(*item);

            if (found->second.returnType != "Void" &&
                found->second.returnType != "<error>" &&
                !alwaysReturns(*item)) {
                report(
                    function.location,
                    "The function '" + function.text + "' must return a value."
                );
            }
        }
    }

    popScope();
    insideFunction_ = false;
    currentReturnType_ = "Void";
}

void TypeChecker::pushScope() {
    scopes_.emplace_back();
}

void TypeChecker::popScope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

TypeChecker::Symbol* TypeChecker::findSymbol(const std::string& name) {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto found = scope->find(name);

        if (found != scope->end()) {
            return &found->second;
        }
    }

    return nullptr;
}

const TypeChecker::FunctionSignature* TypeChecker::findFunction(
    const std::string& name
) const {
    const auto found = functions_.find(name);

    return found == functions_.end() ? nullptr : &found->second;
}

std::string TypeChecker::declaredType(const Node& node) const {
    for (const auto& child : node.children) {
        if (child->kind == NodeKind::TypeName) {
            return child->text;
        }
    }

    return {};
}

bool TypeChecker::isKnownType(const std::string& type) const noexcept {
    for (const std::string_view builtin : builtinTypeNames()) {
        if (builtin == type) return true;
    }
    return false;
}

bool TypeChecker::isNumeric(const std::string& type) const noexcept {
    return type == "Int" || type == "Float";
}

bool TypeChecker::isAssignable(
    const std::string& destination,
    const std::string& source
) const noexcept {
    if (destination == "<error>" || source == "<error>") {
        return true;
    }

    if (destination == "<infer>") {
        return true;
    }

    if (destination == source) {
        return true;
    }

    return destination == "Float" && source == "Int";
}

void TypeChecker::checkBlock(const Node& block) {
    pushScope();

    for (const auto& statement : block.children) {
        checkStatement(*statement);
    }

    popScope();
}

void TypeChecker::checkStatement(const Node& node) {
    switch (node.kind) {
    case NodeKind::Block:
        checkBlock(node);
        return;

    case NodeKind::LetDeclaration:
    case NodeKind::VarDeclaration: {
        const bool mutableValue = node.kind == NodeKind::VarDeclaration;
        const std::string annotation = declaredType(node);
        std::string initializerType;

        for (const auto& child : node.children) {
            if (child->kind != NodeKind::TypeName) {
                initializerType = checkExpression(*child);
                break;
            }
        }

        std::string type = annotation.empty() ? initializerType : annotation;

        if (!annotation.empty() && !isKnownType(annotation)) {
            report(
                node.location,
                "Unknown type '" + annotation + "'."
            );
            type = "<error>";
        }

        if (type == "Void") {
            report(
                node.location,
                "Variables cannot have type 'Void'."
            );
            type = "<error>";
        }

        if (type.empty()) {
            report(node.location, "Could not infer variable type.");
            type = "<error>";
        }

        if (!annotation.empty() &&
            !isAssignable(annotation, initializerType)) {
            report(
                node.location,
                "Cannot initialise '" + node.text + "' of type '" +
                    annotation + "' with a value of type '" +
                    initializerType + "'."
            );
        }

        if (!scopes_.empty() && scopes_.back().contains(node.text)) {
            report(
                node.location,
                "Variable '" + node.text +
                    "' is already declared in this scope."
            );
        } else if (!scopes_.empty()) {
            bool shadowParameter = false;
            if (Symbol* existing = findSymbol(node.text)) {
                shadowParameter = existing->isParameter;
            }

            if (shadowParameter) {
                report(
                    node.location,
                    "Cannot shadow parameter '" + node.text + "'."
                );
            } else {
                scopes_.back().emplace(
                    node.text,
                    Symbol{type, mutableValue, false, node.location}
                );
            }
        }

        return;
    }

    case NodeKind::ImportDeclaration:
        report(
            node.location,
            "Package imports are only allowed at the top level of a file."
        );
        return;

    case NodeKind::Assignment: {
        Symbol* symbol = findSymbol(node.text);

        if (symbol == nullptr) {
            report(
                node.location,
                "Undeclared variable '" + node.text + "'."
            );
            if (!node.children.empty()) {
                static_cast<void>(checkExpression(*node.children.front()));
            }
            return;
        }

        if (!symbol->mutableValue) {
            report(
                node.location,
                "Cannot assign to immutable variable '" + node.text + "'."
            );
        }

        if (!node.children.empty()) {
            const std::string valueType =
                checkExpression(*node.children.front());

            if (!isAssignable(symbol->type, valueType)) {
                report(
                    node.location,
                    "Cannot assign a value of type '" + valueType +
                        "' to variable '" + node.text +
                        "' of type '" + symbol->type + "'."
                );
            }
        }

        return;
    }

    case NodeKind::ExpressionStatement:
        if (!node.children.empty()) {
            const Node& expression = *node.children.front();

            if (expression.kind == NodeKind::Assignment) {
                checkStatement(expression);
            } else {
                static_cast<void>(checkExpression(expression));
            }
        }
        return;

    case NodeKind::IfStatement: {
        if (node.children.size() < 2) {
            report(node.location, "Malformed if statement.");
            return;
        }

        const std::string conditionType =
            checkExpression(*node.children[0]);

        if (conditionType != "Bool" && conditionType != "<error>") {
            report(
                node.children[0]->location,
                "If condition must have type 'Bool', not '" +
                    conditionType + "'."
            );
        }

        checkStatement(*node.children[1]);

        if (node.children.size() >= 3) {
            checkStatement(*node.children[2]);
        }

        return;
    }

    case NodeKind::WhileStatement: {
        if (node.children.size() != 2) {
            report(node.location, "Malformed while statement.");
            return;
        }

        const std::string conditionType =
            checkExpression(*node.children[0]);

        if (conditionType != "Bool" && conditionType != "<error>") {
            report(
                node.children[0]->location,
                "While condition must have type 'Bool', not '" +
                    conditionType + "'."
            );
        }

        checkStatement(*node.children[1]);
        return;
    }

    case NodeKind::ReturnStatement: {
        std::string actualType = "Void";

        if (!node.children.empty()) {
            actualType = checkExpression(*node.children.front());
        }

        if (!insideFunction_) {
            report(node.location, "'return' is only valid inside a function.");
        } else if (!isAssignable(currentReturnType_, actualType)) {
            report(
                node.location,
                "Return type mismatch: expected '" + currentReturnType_ +
                    "' but got '" + actualType + "'."
            );
        }

        return;
    }

    default:
        report(node.location, "Unexpected syntax node in statement position.");
        return;
    }
}

std::string TypeChecker::checkExpression(const Node& expression) {
    switch (expression.kind) {
    case NodeKind::LiteralExpression: {
        const std::string& value = expression.text;

        if (value == "true" || value == "false") {
            return "Bool";
        }

        if (value == "null") {
            return "Null";
        }

        if (!value.empty() && value.front() == '"') {
            return "String";
        }

        if (isIntegerLiteral(value)) {
            unsigned long long magnitude = 0;

            if (!parseIntegerLiteral(value, magnitude) ||
                magnitude >= kMinimumIntegerMagnitude) {
                report(
                    expression.location,
                    "Integer literal '" + value +
                        "' is out of range for type 'Int'."
                );
                return "<error>";
            }
        }

        return value.find('.') == std::string::npos ? "Int" : "Float";
    }

    case NodeKind::NameExpression: {
        if (isBuiltinFunction(expression.text)) {
            return "Builtin";
        }

        if (Symbol* symbol = findSymbol(expression.text)) {
            return symbol->type;
        }

        if (findImport(expression.text) != nullptr) {
            return "Package";
        }

        if (findFunction(expression.text) != nullptr) {
            return "Function";
        }

        report(
            expression.location,
            "Undeclared name '" + expression.text + "'."
        );
        return "<error>";
    }

    case NodeKind::MemberExpression: {
        PackageCall call;

        if (!resolvePackageCall(expression, call)) {
            return "<error>";
        }

        report(
            expression.location,
            "Package functions can only be used in a direct call."
        );
        return "<error>";
    }

    case NodeKind::UnaryExpression: {
        if (expression.children.empty()) {
            return "<error>";
        }

        const Node& operandNode = *expression.children.front();

        if (expression.text == "-" &&
            operandNode.kind == NodeKind::LiteralExpression &&
            isIntegerLiteral(operandNode.text)) {
            unsigned long long magnitude = 0;

            if (parseIntegerLiteral(operandNode.text, magnitude) &&
                magnitude == kMinimumIntegerMagnitude) {
                return "Int";
            }
        }

        const std::string operand =
            checkExpression(operandNode);

        if (expression.text == "!") {
            if (operand != "Bool" && operand != "<error>") {
                report(
                    expression.location,
                    "Operator '!' requires a Bool operand."
                );
            }
            return "Bool";
        }

        if (!isNumeric(operand) && operand != "<error>") {
            report(
                expression.location,
                "Unary '-' requires a numeric operand."
            );
            return "<error>";
        }

        return operand;
    }

    case NodeKind::BinaryExpression: {
        if (expression.children.size() != 2) {
            return "<error>";
        }

        const std::string left =
            checkExpression(*expression.children[0]);
        const std::string right =
            checkExpression(*expression.children[1]);
        const std::string& op = expression.text;

        if (left == "<error>" || right == "<error>") {
            return "<error>";
        }

        if (op == "&&" || op == "||") {
            if (left != "Bool" || right != "Bool") {
                report(
                    expression.location,
                    "Logical operators require Bool operands."
                );
                return "<error>";
            }
            return "Bool";
        }

        if (op == "==" || op == "!=") {
            if (left != right &&
                !(isNumeric(left) && isNumeric(right))) {
                report(
                    expression.location,
                    "Cannot compare values of type '" + left +
                        "' and '" + right + "'."
                );
                return "<error>";
            }
            return "Bool";
        }

        if (op == "<" || op == "<=" || op == ">" || op == ">=") {
            if (!isNumeric(left) || !isNumeric(right)) {
                report(
                    expression.location,
                    "Comparison operators require numeric operands."
                );
                return "<error>";
            }
            return "Bool";
        }

        if (op == "+" && left == "String" && right == "String") {
            return "String";
        }

        if (!isNumeric(left) || !isNumeric(right)) {
            report(
                expression.location,
                "Operator '" + op +
                    "' requires numeric operands (or two Strings for '+')."
            );
            return "<error>";
        }

        if (op == "%" && (left != "Int" || right != "Int")) {
            report(
                expression.location,
                "Operator '%' requires Int operands."
            );
            return "<error>";
        }

        return left == "Float" || right == "Float" ? "Float" : "Int";
    }

    case NodeKind::CallExpression: {
        if (expression.children.empty()) {
            return "<error>";
        }

        const Node& callee = *expression.children.front();

        if (callee.kind == NodeKind::MemberExpression) {
            PackageCall signature;
            const std::size_t argumentCount = expression.children.size() - 1;

            if (!resolvePackageCall(callee, signature)) {
                for (std::size_t i = 1; i < expression.children.size(); ++i) {
                    static_cast<void>(checkExpression(*expression.children[i]));
                }

                return "<error>";
            }

            const std::string qualifiedName =
                callee.children.front()->text + "." + callee.text;

            if (argumentCount != signature.parameters.size()) {
                report(
                    expression.location,
                    "Function '" + qualifiedName + "' expects " +
                        std::to_string(signature.parameters.size()) +
                        " argument(s), but got " +
                        std::to_string(argumentCount) + "."
                );
            }

            for (std::size_t i = 1; i < expression.children.size(); ++i) {
                const std::string actual =
                    checkExpression(*expression.children[i]);
                const std::size_t parameterIndex = i - 1;

                if (parameterIndex < signature.parameters.size()) {
                    const std::string& expected =
                        signature.parameters[parameterIndex];

                    if (!isAssignable(expected, actual)) {
                        report(
                            expression.children[i]->location,
                            "Argument " + std::to_string(i) + " of '" +
                                qualifiedName + "' expects '" + expected +
                                "', but got '" + actual + "'."
                        );
                    }
                }
            }

            return signature.returnType;
        }

        if (callee.kind != NodeKind::NameExpression) {
            report(
                expression.location,
                "Only named functions can be called currently."
            );

            for (std::size_t i = 1; i < expression.children.size(); ++i) {
                static_cast<void>(checkExpression(*expression.children[i]));
            }

            return "<error>";
        }

        const std::string& name = callee.text;

        if (isBuiltinFunction(name)) {
            for (std::size_t i = 1; i < expression.children.size(); ++i) {
                static_cast<void>(checkExpression(*expression.children[i]));
            }
            return "Void";
        }

        const FunctionSignature* signature = nullptr;

        if (!currentPackage_.empty()) {
            signature = findFunction(currentPackage_ + "." + name);
        }

        if (signature == nullptr) {
            signature = findFunction(name);
        }

        if (signature == nullptr) {
            report(
                expression.location,
                "Call to undeclared function '" + name + "'."
            );

            for (std::size_t i = 1; i < expression.children.size(); ++i) {
                static_cast<void>(checkExpression(*expression.children[i]));
            }

            return "<error>";
        }

        const std::size_t argumentCount = expression.children.size() - 1;

        if (argumentCount != signature->parameters.size()) {
            report(
                expression.location,
                "Function '" + name + "' expects " +
                    std::to_string(signature->parameters.size()) +
                    " argument(s), but got " +
                    std::to_string(argumentCount) + "."
            );
        }

        for (std::size_t i = 1; i < expression.children.size(); ++i) {
            const std::string actual =
                checkExpression(*expression.children[i]);
            const std::size_t parameterIndex = i - 1;

            if (parameterIndex < signature->parameters.size()) {
                const std::string& expected =
                    signature->parameters[parameterIndex];

                if (!isAssignable(expected, actual)) {
                    report(
                        expression.children[i]->location,
                        "Argument " + std::to_string(i) +
                            " of '" + name + "' expects '" + expected +
                            "', but got '" + actual + "'."
                    );
                }
            }
        }

        return signature->returnType;
    }

    default:
        report(
            expression.location,
            "Unexpected syntax node in expression position."
        );
        return "<error>";
    }
}

}
