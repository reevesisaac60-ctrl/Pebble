#include "cli.hpp"
#include "pebble/codegen.hpp"
#include "pebble/lexer.hpp"
#include "pebble/module.hpp"
#include "pebble/parser.hpp"
#include "pebble/type_checker.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace {

namespace fs = std::filesystem;

void printUsage(const char* executable) {
    std::cerr
        << "Pebble Native 0.4.0\n"
        << "Usage:\n"
        << "  " << executable << " lex <file.peb>\n"
        << "  " << executable << " parse <file.peb>\n"
        << "  " << executable << " check [<file.peb>]\n"
        << "  " << executable << " build [<file.peb>]\n"
        << "  " << executable << " run [<file.peb>]\n"
        << "  " << executable << " init [<name>]\n"
        << "  " << executable << " add <path>\n"
        << "  " << executable << " remove <package>\n"
        << "  " << executable << " install\n"
        << "  " << executable << " update\n"
        << "  " << executable << " tree\n"
        << "  " << executable << " package\n";
}

void printAst(const pebble::Node& node, std::size_t depth = 0) {
    for (std::size_t i = 0; i < depth; ++i) {
        std::cout << "  ";
    }

    std::cout << pebble::nodeKindName(node.kind);

    if (!node.text.empty()) {
        std::cout << " `" << node.text << '`';
    }

    if (node.kind == pebble::NodeKind::Function && node.exported) {
        std::cout << " exported";
    }

    std::cout << " [" << node.location.line
              << ':' << node.location.column << "]\n";

    for (const auto& child : node.children) {
        printAst(*child, depth + 1);
    }
}

void printDiagnostics(
    const std::vector<pebble::Diagnostic>& diagnostics
) {
    for (const pebble::Diagnostic& diagnostic : diagnostics) {
        std::cerr
            << diagnostic.location.file << ':'
            << diagnostic.location.line << ':'
            << diagnostic.location.column
            << ": error: " << diagnostic.message << '\n';
    }
}

std::string readSource(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);

    if (!file) {
        throw std::runtime_error("cannot open '" + filename + "'");
    }

    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

std::string shellQuote(const std::string& value) {
#if defined(_WIN32)
    std::string result = "\"";

    for (const char character : value) {
        result += character;
    }

    result += "\"";
    return result;
#else
    std::string result = "'";

    for (const char character : value) {
        if (character == '\'') {
            result += "'\\''";
        } else {
            result += character;
        }
    }

    result += "'";
    return result;
#endif
}

std::string artifactTag(const std::string& filename) {
    std::uint64_t hash = 14695981039346656037ULL;

    for (const char character : filename) {
        hash ^= static_cast<unsigned char>(character);
        hash *= 1099511628211ULL;
    }

    std::ostringstream output;
    output << std::hex << std::setfill('0')
           << std::setw(16) << hash;

    return output.str();
}

std::vector<fs::path> installedResourceRoots() {
    std::vector<fs::path> roots;
    std::error_code code;

#if defined(__linux__)
    const fs::path executable = fs::read_symlink("/proc/self/exe", code);

    if (!code && executable.has_parent_path()) {
        const fs::path relocatable =
            executable.parent_path().parent_path() / "share" / "pebble";

        if (fs::is_directory(relocatable, code)) {
            roots.push_back(relocatable);
        }
    }
#endif

#if defined(PEBBLE_DATA_DIR)
    const fs::path configured(PEBBLE_DATA_DIR);

    if (fs::is_directory(configured, code)) {
        roots.push_back(configured);
    }
#endif

    return roots;
}

std::vector<std::string> resourceRootsFor(const std::string& filename) {
    std::vector<fs::path> roots;
    std::error_code code;

    fs::path program = fs::weakly_canonical(fs::absolute(filename, code), code);

    if (code) {
        program = fs::absolute(filename).lexically_normal();
        code.clear();
    }

    const fs::path programDirectory = program.parent_path();

    if (!programDirectory.empty()) {
        roots.push_back(programDirectory);

        for (fs::path directory = programDirectory; !directory.empty();
             directory = directory.parent_path()) {
            if (fs::is_regular_file(directory / "pebble.toml", code)) {
                roots.push_back(directory);
                break;
            }

            if (directory == directory.parent_path()) {
                break;
            }
        }
    }

    for (const fs::path& root : installedResourceRoots()) {
        roots.push_back(root);
    }

    std::vector<std::string> result;

    for (const fs::path& root : roots) {
        const std::string text = root.string();

        if (std::find(result.begin(), result.end(), text) == result.end()) {
            result.push_back(text);
        }
    }

    return result;
}

fs::path cacheHomeDirectory() {
    if (const char* xdgCacheHome = std::getenv("XDG_CACHE_HOME")) {
        if (xdgCacheHome[0] != '\0') {
            return fs::path(xdgCacheHome);
        }
    }

    if (const char* home = std::getenv("HOME")) {
        if (home[0] != '\0') {
            return fs::path(home) / ".cache";
        }
    }

    return fs::path(".");
}

fs::path buildOutputDirectory(const std::string& filename) {
    std::error_code code;

    const fs::path inputPath = fs::absolute(filename, code);
    const fs::path sourceDirectory = inputPath.has_parent_path()
        ? inputPath.parent_path()
        : fs::current_path(code);

    const fs::path preferred = sourceDirectory / "build";

    if (fs::create_directories(preferred, code) ||
        fs::is_directory(preferred, code)) {
        return preferred;
    }

    code.clear();

    const fs::path fallback =
        cacheHomeDirectory() / "pebble" / "build" /
        artifactTag(sourceDirectory.string());

    if (fs::create_directories(fallback, code) ||
        fs::is_directory(fallback, code)) {
        return fallback;
    }

    throw std::runtime_error(
        "cannot create build directory '" + preferred.string() +
        "': " + code.message() +
        " (working directory: " + fs::current_path().string() + ")"
    );
}

struct BuildOutput {
    fs::path source;
    fs::path executable;
};

BuildOutput buildProgram(
    const std::string& filename,
    const pebble::Node& program
) {
    const fs::path inputPath(filename);
    const std::string stem = inputPath.stem().string();

    if (stem.empty()) {
        throw std::runtime_error("source filename has no usable stem");
    }

    const fs::path outputDirectory = buildOutputDirectory(filename);

    const std::string artifactName = stem + "-" + artifactTag(filename);

    const fs::path generatedPath =
        outputDirectory / (artifactName + ".generated.cpp");
#if defined(_WIN32)
    const fs::path executablePath = outputDirectory / (artifactName + ".exe");
#else
    const fs::path executablePath = outputDirectory / artifactName;
#endif

    pebble::CodegenOptions options;
    options.resourceRoots = resourceRootsFor(filename);

    const std::string generated =
        pebble::CodeGenerator{}.generate(program, options);

    {
        std::ofstream output(generatedPath, std::ios::binary);

        if (!output) {
            throw std::runtime_error(
                "cannot write '" + generatedPath.string() + "'"
            );
        }

        output << generated;

        if (!output) {
            throw std::runtime_error(
                "failed writing '" + generatedPath.string() + "'"
            );
        }
    }

    const char* configuredCompiler = std::getenv("CXX");
    const std::string compiler =
        configuredCompiler != nullptr && configuredCompiler[0] != '\0'
            ? configuredCompiler
            : "c++";

#if defined(_WIN32)
    const std::string nativeLibraries = " -lglfw3 -lopengl32 -lgdi32";
#elif defined(__APPLE__)
    const std::string nativeLibraries =
        " -lglfw3 -framework OpenGL -framework Cocoa -framework IOKit "
        "-framework CoreVideo";
#else
    const std::string nativeLibraries =
        " $(pkg-config --cflags --libs glfw3 gl 2>/dev/null || "
        "echo -lglfw -lGL -lX11 -lpthread -ldl)";
#endif

    const std::string command =
        shellQuote(compiler) +
        " -std=c++20 -Wall -Wextra -Wpedantic -Werror " +
        shellQuote(generatedPath.string()) +
        " -o " + shellQuote(executablePath.string()) + nativeLibraries;

    std::cout << "Compiling " << generatedPath.string() << "...\n";

    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("native C++ compilation failed");
    }

    return BuildOutput{generatedPath, executablePath};
}

int runExecutable(const fs::path& executable) {
    const std::string command =
        shellQuote((fs::path(".") / executable).string());

    const int status = std::system(command.c_str());

    if (status == -1) {
        std::cerr << "error: could not start native executable\n";
        return 1;
    }

    return status == 0 ? 0 : 1;
}

}

int compileAndRun(
    const std::string& command,
    const std::string& filename,
    const std::vector<std::string>& searchPaths
) {
    try {
        pebble::Source source{filename, readSource(filename)};
        const pebble::LexResult lexed = pebble::Lexer{}.scan(source);

        if (!lexed.ok()) {
            printDiagnostics(lexed.diagnostics);
            return 1;
        }

        const pebble::ParseResult parsed =
            pebble::Parser{}.parse(lexed);

        if (!parsed.ok() || parsed.root == nullptr) {
            printDiagnostics(parsed.diagnostics);
            return 1;
        }

        const fs::path inputPath(filename);
        pebble::ModuleResult modules = pebble::resolveModules(
            *parsed.root,
            inputPath.parent_path().string(),
            searchPaths
        );

        printDiagnostics(modules.diagnostics);

        if (!modules.ok()) {
            return 1;
        }

        for (auto& function : modules.functions) {
            parsed.root->add(std::move(function));
        }

        const pebble::TypeCheckResult checked =
            pebble::TypeChecker{}.check(*parsed.root);

        printDiagnostics(checked.diagnostics);

        if (!checked.ok()) {
            return 1;
        }

        if (command == "check") {
            std::cout << "Type check passed: " << filename << '\n';
            return 0;
        }

        const BuildOutput output = buildProgram(filename, *parsed.root);

        if (command == "build") {
            std::cout << "Built native executable: "
                      << output.executable.string() << '\n';
            return 0;
        }

        return runExecutable(output.executable);
    } catch (const std::exception& exception) {
        std::cerr << "error: " << exception.what() << '\n';
        return 1;
    }
}

int runProjectCommand(const std::string& command) {
    pebble_cli::Project project;
    std::string message;
    const std::string root = ".";

    if (!pebble_cli::loadProject(root, project, message)) {
        std::cerr << "error: " << message << '\n';
        return 1;
    }

    if (project.entryPoint.empty()) {
        std::cerr << "error: the project has no entry point. Add "
                     "'src/main.peb' or 'main.peb'.\n";
        return 1;
    }

    return compileAndRun(command, project.entryPoint, project.searchPaths);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 2;
    }

    const std::string command = argv[1];
    const std::string root = ".";

    if (command == "init") {
        if (argc > 3) {
            printUsage(argv[0]);
            return 2;
        }

        return pebble_cli::initCommand(
            root, argc == 3 ? std::string(argv[2]) : std::string());
    }

    if (command == "add") {
        if (argc != 3) {
            printUsage(argv[0]);
            return 2;
        }

        return pebble_cli::addCommand(root, argv[2]);
    }

    if (command == "remove") {
        if (argc != 3) {
            printUsage(argv[0]);
            return 2;
        }

        return pebble_cli::removeCommand(root, argv[2]);
    }

    if (command == "install" || command == "update") {
        if (argc != 2) {
            printUsage(argv[0]);
            return 2;
        }

        return command == "install"
            ? pebble_cli::installCommand(root)
            : pebble_cli::updateCommand(root);
    }

    if (command == "tree") {
        if (argc != 2) {
            printUsage(argv[0]);
            return 2;
        }

        return pebble_cli::treeCommand(root);
    }

    if (command == "package") {
        if (argc != 2) {
            printUsage(argv[0]);
            return 2;
        }

        return pebble_cli::packageCommand(root);
    }

    if (command != "lex" && command != "parse" &&
        command != "check" && command != "build" &&
        command != "run") {
        printUsage(argv[0]);
        return 2;
    }

    if (command == "lex" || command == "parse") {
        if (argc != 3) {
            printUsage(argv[0]);
            return 2;
        }
    } else if (argc == 2) {
        return runProjectCommand(command);
    } else if (argc != 3) {
        printUsage(argv[0]);
        return 2;
    }

    const std::string filename = argv[2];

    try {
        pebble::Source source{filename, readSource(filename)};
        const pebble::LexResult lexed = pebble::Lexer{}.scan(source);

        if (command == "lex") {
            for (const pebble::Token& token : lexed.tokens) {
                std::cout
                    << token.location.line << ':'
                    << token.location.column << "  "
                    << pebble::tokenKindName(token.kind);

                if (!token.lexeme.empty()) {
                    std::cout << "  `" << token.lexeme << '`';
                }

                std::cout << '\n';
            }

            printDiagnostics(lexed.diagnostics);
            return lexed.ok() ? 0 : 1;
        }

        if (!lexed.ok()) {
            printDiagnostics(lexed.diagnostics);
            return 1;
        }

        const pebble::ParseResult parsed =
            pebble::Parser{}.parse(lexed);

        if (command == "parse") {
            if (parsed.root != nullptr) {
                printAst(*parsed.root);
            }

            printDiagnostics(parsed.diagnostics);
            return parsed.ok() ? 0 : 1;
        }

        if (!parsed.ok() || parsed.root == nullptr) {
            printDiagnostics(parsed.diagnostics);
            return 1;
        }

        const fs::path inputPath(filename);
        pebble::ModuleResult modules = pebble::resolveModules(
            *parsed.root,
            inputPath.parent_path().string()
        );

        printDiagnostics(modules.diagnostics);

        if (!modules.ok()) {
            return 1;
        }

        for (auto& function : modules.functions) {
            parsed.root->add(std::move(function));
        }

        const pebble::TypeCheckResult checked =
            pebble::TypeChecker{}.check(*parsed.root);

        printDiagnostics(checked.diagnostics);

        if (!checked.ok()) {
            return 1;
        }

        if (command == "check") {
            std::cout << "Type check passed: " << filename << '\n';
            return 0;
        }

        const BuildOutput output = buildProgram(filename, *parsed.root);

        if (command == "build") {
            std::cout << "Built native executable: "
                      << output.executable.string() << '\n';
            return 0;
        }

        return runExecutable(output.executable);
    } catch (const std::exception& exception) {
        std::cerr << "error: " << exception.what() << '\n';
        return 1;
    }
}
