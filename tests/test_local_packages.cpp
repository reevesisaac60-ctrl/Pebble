#include "pebble/codegen.hpp"
#include "pebble/lexer.hpp"
#include "pebble/module.hpp"
#include "pebble/parser.hpp"
#include "pebble/type_checker.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::filesystem::path makeWorkspace(const std::string& name) {
    const auto directory = std::filesystem::temp_directory_path() / name;

    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    return directory;
}

void writeFile(
    const std::filesystem::path& path,
    const std::string& contents
) {
    std::ofstream file(path, std::ios::binary);
    file << contents;
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

struct Program {
    pebble::LexResult lexed;
    pebble::ParseResult parsed;
    pebble::ModuleResult modules;
    pebble::TypeCheckResult checked;
};

Program load(
    const std::filesystem::path& path,
    const std::string& contents,
    const std::string& directory
) {
    Program result{pebble::LexResult{}, pebble::ParseResult{},
                    pebble::ModuleResult{}, pebble::TypeCheckResult{}};
    const pebble::Source source{path.string(), contents};

    result.lexed = pebble::Lexer{}.scan(source);
    result.parsed = pebble::Parser{}.parse(result.lexed);

    if (!result.parsed.ok() || !result.parsed.root) {
        std::cerr << "Test setup failed during parsing:\n" << contents << '\n';

        for (const pebble::Diagnostic& diagnostic : result.parsed.diagnostics) {
            std::cerr << "  " << diagnostic.message << '\n';
        }

        std::exit(EXIT_FAILURE);
    }

    result.modules = pebble::resolveModules(*result.parsed.root, directory);

    for (auto& function : result.modules.functions) {
        result.parsed.root->add(std::move(function));
    }

    result.modules.functions.clear();
    result.checked = pebble::TypeChecker{}.check(*result.parsed.root);
    return result;
}

bool containsMessage(
    const std::vector<pebble::Diagnostic>& diagnostics,
    std::string_view fragment
) {
    for (const pebble::Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.message.find(fragment) != std::string::npos) {
            return true;
        }
    }

    return false;
}

void testPackageNextToTheImportingFileIsResolved() {
    const auto workspace = makeWorkspace("pebble-local-package-sibling");
    const auto package = workspace / "mylib.peb";

    writeFile(
        package,
        "export fn add(a: Int, b: Int) -> Int { return a + b; }\n"
    );

    const auto main = workspace / "main.peb";
    writeFile(
        main,
        "import package mylib\n"
        "fn main() { print(mylib.add(1, 2)); }\n"
    );

    const Program result = load(
        main, readFile(main), workspace.string()
    );

    require(result.modules.ok(), "the package should resolve");
    require(result.checked.diagnostics.empty(),
            "the program should type check");
    require(result.modules.diagnostics.empty(),
            "no diagnostics should be reported");
}

void testPackageInThePackagesDirectoryIsResolved() {
    const auto workspace = makeWorkspace("pebble-local-package-directory");
    const auto directory = workspace / "packages";

    std::filesystem::create_directories(directory);
    writeFile(
        directory / "tools.peb",
        "export fn triple(x: Int) -> Int { return x * 3; }\n"
    );

    const auto main = workspace / "main.peb";
    writeFile(
        main,
        "import package tools\n"
        "fn main() { print(tools.triple(5)); }\n"
    );

    const Program result = load(
        main, readFile(main), workspace.string()
    );

    require(result.modules.ok(), "the package should resolve");
    require(result.checked.diagnostics.empty(),
            "the program should type check");
}

void testMissingPackageIsReported() {
    const auto workspace = makeWorkspace("pebble-local-package-missing");
    const auto main = workspace / "main.peb";

    writeFile(
        main,
        "import package nothere\n"
        "fn main() { print(1); }\n"
    );

    const Program result = load(
        main, readFile(main), workspace.string()
    );

    require(!result.modules.ok(), "the missing package should be reported");
    require(
        containsMessage(result.modules.diagnostics,
                       "Cannot find package 'nothere'"),
        "the missing package should be named"
    );
}

void testPrivateFunctionIsNotAccessible() {
    const auto workspace = makeWorkspace("pebble-local-package-private");
    const auto package = workspace / "mylib.peb";

    writeFile(
        package,
        "fn helper(x: Int) -> Int { return x * 2; }\n"
        "export fn add(a: Int, b: Int) -> Int { return helper(a) + b; }\n"
    );

    const auto main = workspace / "main.peb";
    writeFile(
        main,
        "import package mylib\n"
        "fn main() { print(mylib.helper(1)); }\n"
    );

    const Program result = load(
        main, readFile(main), workspace.string()
    );

    require(!result.checked.diagnostics.empty(),
            "the private function should be rejected");
    require(
        containsMessage(result.checked.diagnostics,
                       "Package 'mylib' has no function 'helper'."),
        "the private function should be reported"
    );
}

void testPackageCallsItsOwnFunctions() {
    const auto workspace = makeWorkspace("pebble-local-package-internal");
    const auto package = workspace / "mylib.peb";

    writeFile(
        package,
        "fn helper(x: Int) -> Int { return x * 2; }\n"
        "export fn add(a: Int, b: Int) -> Int { return helper(a) + b; }\n"
    );

    const auto main = workspace / "main.peb";
    writeFile(
        main,
        "import package mylib\n"
        "fn main() { print(mylib.add(1, 2)); }\n"
    );

    const Program result = load(
        main, readFile(main), workspace.string()
    );

    require(result.checked.diagnostics.empty(),
            "the package should call its own functions");
}

void testNestedPackageImportsAreResolved() {
    const auto workspace = makeWorkspace("pebble-local-package-nested");
    const auto package = workspace / "outer.peb";

    writeFile(
        package,
        "import package tools\n"
        "fn helper(x: Int) -> Int { return tools.triple(x) + 1; }\n"
        "export fn compute(x: Int) -> Int { return helper(x); }\n"
    );

    const auto directory = workspace / "packages";
    std::filesystem::create_directories(directory);
    writeFile(
        directory / "tools.peb",
        "export fn triple(x: Int) -> Int { return x * 3; }\n"
    );

    const auto main = workspace / "main.peb";
    writeFile(
        main,
        "import package outer\n"
        "fn main() { print(outer.compute(2)); }\n"
    );

    const Program result = load(
        main, readFile(main), workspace.string()
    );

    require(result.modules.ok(), "the nested package should resolve");
    require(result.checked.diagnostics.empty(),
            "the nested package should type check");
}

void testPackageFunctionsAreMangled() {
    const auto workspace = makeWorkspace("pebble-local-package-mangling");
    const auto package = workspace / "mylib.peb";

    writeFile(
        package,
        "export fn add(a: Int, b: Int) -> Int { return a + b; }\n"
    );

    const auto main = workspace / "main.peb";
    writeFile(
        main,
        "import package mylib\n"
        "fn main() { print(mylib.add(1, 2)); }\n"
    );

    const Program result = load(
        main, readFile(main), workspace.string()
    );

    require(result.checked.diagnostics.empty(),
            "the program should type check");

    const std::string generated =
        pebble::CodeGenerator{}.generate(*result.parsed.root);

    require(
        generated.find("pebble_fn_mylib_add") != std::string::npos,
        "the package function should be mangled"
    );
}

void testPackageFunctionShadowsAGlobalFunction() {
    const auto workspace = makeWorkspace("pebble-local-package-shadowing");
    const auto package = workspace / "mylib.peb";

    writeFile(
        package,
        "fn helper(x: Int) -> Int { return x + 1; }\n"
        "export fn compute(x: Int) -> Int { return helper(x); }\n"
    );

    const auto main = workspace / "main.peb";
    writeFile(
        main,
        "import package mylib\n"
        "fn helper(x: Int) -> Int { return x * 100; }\n"
        "fn main() { print(mylib.compute(5)); }\n"
    );

    const Program result = load(
        main, readFile(main), workspace.string()
    );

    require(result.checked.diagnostics.empty(),
            "the package should resolve its own function");
}

void testTypeErrorInPackageKeepsThePackageLocation() {
    const auto workspace = makeWorkspace("pebble-local-package-location");
    const auto package = workspace / "mylib.peb";

    writeFile(
        package,
        "export fn add(a: Int, b: Int) -> Int { return a + \"text\"; }\n"
    );

    const auto main = workspace / "main.peb";
    writeFile(
        main,
        "import package mylib\n"
        "fn main() { print(mylib.add(1, 2)); }\n"
    );

    const Program result = load(
        main, readFile(main), workspace.string()
    );

    require(!result.checked.diagnostics.empty(),
            "the type error should be reported");
    require(
        result.checked.diagnostics.front().location.file ==
            package.string(),
        "the diagnostic should point at the package file"
    );
}

}

int main() {
    testPackageNextToTheImportingFileIsResolved();
    testPackageInThePackagesDirectoryIsResolved();
    testMissingPackageIsReported();
    testPrivateFunctionIsNotAccessible();
    testPackageCallsItsOwnFunctions();
    testNestedPackageImportsAreResolved();
    testPackageFunctionShadowsAGlobalFunction();
    testPackageFunctionsAreMangled();
    testTypeErrorInPackageKeepsThePackageLocation();

    std::cout << "Local package tests passed.\n";
    return EXIT_SUCCESS;
}
