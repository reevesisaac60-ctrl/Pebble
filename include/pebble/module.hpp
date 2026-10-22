#pragma once

#include "pebble/ast.hpp"

#include <memory>
#include <string>
#include <vector>

namespace pebble {

struct ModuleResult {
    std::vector<Diagnostic> diagnostics;
    std::vector<std::unique_ptr<Node>> functions;
    std::vector<std::string> sourceFiles;

    [[nodiscard]] bool ok() const noexcept {
        return diagnostics.empty();
    }
};

[[nodiscard]] std::unique_ptr<Node> cloneNode(const Node& node);

[[nodiscard]] ModuleResult resolveModules(
    Node& program,
    const std::string& baseDirectory,
    const std::vector<std::string>& searchPaths = {}
);

}
