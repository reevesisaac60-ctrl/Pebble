#include "pebble/module.hpp"
#include "pebble/lexer.hpp"
#include "pebble/package.hpp"
#include "pebble/parser.hpp"

#include <fstream>
#include <sstream>
#include <utility>

namespace pebble {

std::unique_ptr<Node> cloneNode(const Node& node) {
    auto copy = std::make_unique<Node>(node.kind, node.location, node.text);

    copy->exported = node.exported;
    copy->local = node.local;

    for (const auto& child : node.children) {
        copy->add(cloneNode(*child));
    }

    return copy;
}

namespace {

std::string joinPath(const std::string& directory, const std::string& name) {
    if (directory.empty()) {
        return name;
    }

    if (directory.back() == '/') {
        return directory + name;
    }

    return directory + "/" + name;
}

bool readFile(const std::string& path, std::string& contents) {
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    if (file.bad()) {
        return false;
    }

    contents = buffer.str();
    return true;
}

std::string directoryOf(const std::string& path) {
    const std::size_t separator = path.find_last_of('/');

    if (separator == std::string::npos) {
        return {};
    }

    return path.substr(0, separator);
}

struct Resolver {
    ModuleResult result;
    std::vector<std::string> resolved;
    std::vector<std::string> active;
    std::vector<std::unique_ptr<Node>> pendingImports;
    std::vector<std::string> searchPaths;
    Node* program = nullptr;

    void report(const SourceLocation& location, std::string message) {
        result.diagnostics.push_back(Diagnostic{location, std::move(message)});
    }

    bool programImports(const std::string& name) const {
        for (const auto& child : program->children) {
            if (child->kind == NodeKind::ImportDeclaration &&
                child->text == name) {
                return true;
            }
        }

        return false;
    }

    void propagateImports(const Node& root) {
        for (const auto& child : root.children) {
            if (child->kind != NodeKind::ImportDeclaration ||
                !child->local) {
                continue;
            }

            if (programImports(child->text)) {
                continue;
            }

            auto declaration = std::make_unique<Node>(
                NodeKind::ImportDeclaration, child->location, child->text
            );

            declaration->local = true;
            pendingImports.push_back(std::move(declaration));
        }
    }

    void resolveImports(Node& root, const std::string& directory) {
        for (const auto& child : root.children) {
            if (child->kind != NodeKind::ImportDeclaration) {
                continue;
            }

            if (findBuiltinPackage(child->text) != nullptr) {
                continue;
            }

            if (!resolvePackage(*child, directory)) {
                continue;
            }
        }
    }

    std::string cycleDescription(const std::string& name) const {
        std::string description;
        bool started = false;

        for (const std::string& entry : active) {
            if (entry == name) {
                started = true;
            }

            if (!started) {
                continue;
            }

            if (!description.empty()) {
                description += " -> ";
            }

            description += entry;
        }

        if (!description.empty()) {
            description += " -> ";
        }

        return description + name;
    }

    bool resolvePackage(Node& declaration, const std::string& directory) {
        const std::string& name = declaration.text;

        for (const std::string& activeName : active) {
            if (activeName == name) {
                report(
                    declaration.location,
                    "Circular dependency detected: " + cycleDescription(name) +
                        "."
                );
                return false;
            }
        }

        for (const std::string& resolvedName : resolved) {
            if (resolvedName == name) {
                return true;
            }
        }

        std::vector<std::string> candidates{
            joinPath(directory, name + ".peb"),
            joinPath(joinPath(directory, "packages"), name + ".peb")
        };

        for (const std::string& searchPath : searchPaths) {
            candidates.push_back(joinPath(searchPath, name + ".peb"));
        }

        std::string path;
        std::string contents;

        for (const std::string& candidate : candidates) {
            if (readFile(candidate, contents)) {
                path = candidate;
                break;
            }
        }

        if (path.empty()) {
            std::string message = "Cannot find package '" + name + "'. Looked for ";

            for (std::size_t index = 0U; index < candidates.size(); ++index) {
                if (index > 0U) {
                    message += index + 1U == candidates.size() ? " and " : ", ";
                }

                message += "'" + candidates[index] + "'";
            }

            message += ".";
            report(declaration.location, message);
            return false;
        }

        result.sourceFiles.push_back(path);

        const Source source{path, contents};
        const LexResult lexed = Lexer{}.scan(source);
        const ParseResult parsed = Parser{}.parse(lexed);

        if (!parsed.ok() || !parsed.root) {
            for (const Diagnostic& diagnostic : lexed.diagnostics) {
                result.diagnostics.push_back(diagnostic);
            }

            for (const Diagnostic& diagnostic : parsed.diagnostics) {
                result.diagnostics.push_back(diagnostic);
            }

            report(
                declaration.location,
                "Cannot parse package '" + name + "'."
            );
            return false;
        }

        resolved.push_back(name);
        declaration.local = true;

        const std::string packageDirectory = directoryOf(path);

        active.push_back(name);
        resolveImports(*parsed.root, packageDirectory);
        active.pop_back();
        propagateImports(*parsed.root);

        for (const auto& child : parsed.root->children) {
            if (child->kind != NodeKind::Function) {
                continue;
            }

            auto function = std::make_unique<Node>(
                NodeKind::Function, child->location, name + "." + child->text
            );

            function->exported = child->exported;

            for (const auto& item : child->children) {
                function->add(cloneNode(*item));
            }

            result.functions.push_back(std::move(function));
        }

        return true;
    }
};

}

ModuleResult resolveModules(
    Node& program,
    const std::string& baseDirectory,
    const std::vector<std::string>& searchPaths
) {
    Resolver resolver;
    resolver.program = &program;
    resolver.searchPaths = searchPaths;
    resolver.resolveImports(program, baseDirectory);

    for (auto& declaration : resolver.pendingImports) {
        program.add(std::move(declaration));
    }

    return std::move(resolver.result);
}

}
