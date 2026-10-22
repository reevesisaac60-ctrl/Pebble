#pragma once

#include "pebble/ast.hpp"

#include <string>
#include <vector>

namespace pebble {

struct CodegenOptions {
    std::vector<std::string> resourceRoots;
};

class CodeGenerator {
public:
    [[nodiscard]] std::string generate(const Node& program) const;
    [[nodiscard]] std::string generate(
        const Node& program,
        const CodegenOptions& options
    ) const;
};

}
