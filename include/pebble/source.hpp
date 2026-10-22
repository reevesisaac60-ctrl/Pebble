#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace pebble {

struct SourceLocation {
    std::string file;
    std::size_t line = 1;
    std::size_t column = 1;
};

struct Diagnostic {
    SourceLocation location;
    std::string message;
};

struct Source {
    std::string name;
    std::string text;

    [[nodiscard]] std::string_view view() const noexcept {
        return text;
    }
};

}
