#pragma once

#include "pebble/ast.hpp"
#include "pebble/package.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pebble {

[[nodiscard]] const std::vector<std::string_view>& builtinTypeNames() noexcept;
[[nodiscard]] const std::vector<std::string_view>& builtinFunctionNames() noexcept;
[[nodiscard]] bool isBuiltinFunction(std::string_view name) noexcept;

struct TypeCheckResult {
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return diagnostics.empty();
    }
};

class TypeChecker {
public:
    [[nodiscard]] TypeCheckResult check(const Node& program) const;

private:
    struct Symbol {
        std::string type;
        bool mutableValue = false;
        bool isParameter = false;
        SourceLocation location;
    };

    struct FunctionSignature {
        std::vector<std::string> parameters;
        std::string returnType = "Void";
        SourceLocation location;
        bool exported = false;
    };

    struct ImportedPackage {
        const Package* builtin = nullptr;
        const Node* declaration = nullptr;
    };

    struct PackageCall {
        std::vector<std::string> parameters;
        std::string returnType = "Void";
    };

    using Scope = std::unordered_map<std::string, Symbol>;

    std::vector<Scope> scopes_;
    std::unordered_map<std::string, FunctionSignature> functions_;
    std::unordered_map<std::string, ImportedPackage> imports_;
    std::vector<Diagnostic> diagnostics_;
    std::string currentReturnType_ = "Void";
    std::string currentPackage_;
    bool insideFunction_ = false;

    void report(const SourceLocation& location, std::string message);
    void collectImports(const Node& program);
    void collectFunctions(const Node& program);
    void checkFunction(const Node& function);
    void checkStatement(const Node& node);
    void checkBlock(const Node& block);
    std::string checkExpression(const Node& expression);

    void pushScope();
    void popScope();
    [[nodiscard]] Symbol* findSymbol(const std::string& name);
    [[nodiscard]] const FunctionSignature* findFunction(
        const std::string& name
    ) const;
    [[nodiscard]] const ImportedPackage* findImport(
        const std::string& name
    ) const;
    [[nodiscard]] bool resolvePackageCall(
        const Node& callee,
        PackageCall& call
    );
    [[nodiscard]] std::string packageOf(const std::string& name) const;

    [[nodiscard]] std::string declaredType(const Node& node) const;
    [[nodiscard]] bool isKnownType(const std::string& type) const noexcept;
    [[nodiscard]] bool isNumeric(const std::string& type) const noexcept;
    [[nodiscard]] bool isAssignable(
        const std::string& destination,
        const std::string& source
    ) const noexcept;
};

}
