#include "pebble/codegen.hpp"
#include "pebble/lexer.hpp"
#include "pebble/package.hpp"
#include "pebble/parser.hpp"
#include "pebble/type_checker.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct Parsed {
    pebble::LexResult lexed;
    pebble::ParseResult parsed;
};

Parsed parse(std::string text) {
    const pebble::Source source{"packages.peb", std::move(text)};
    Parsed result{pebble::Lexer{}.scan(source), {}};
    result.parsed = pebble::Parser{}.parse(result.lexed);
    return result;
}

pebble::TypeCheckResult check(std::string text) {
    const std::string copy = text;
    Parsed result = parse(std::move(text));

    if (!result.parsed.ok() || !result.parsed.root) {
        std::cerr << "Test setup failed during parsing:\n" << copy << '\n';

        for (const pebble::Diagnostic& diagnostic : result.parsed.diagnostics) {
            std::cerr << "  " << diagnostic.message << '\n';
        }

        std::exit(EXIT_FAILURE);
    }

    return pebble::TypeChecker{}.check(*result.parsed.root);
}

bool containsMessage(
    const pebble::TypeCheckResult& result,
    std::string_view fragment
) {
    for (const pebble::Diagnostic& diagnostic : result.diagnostics) {
        if (diagnostic.message.find(fragment) != std::string::npos) {
            return true;
        }
    }

    return false;
}

void testImportKeywordsAreLexed() {
    Parsed result = parse("import package math\n");

    require(result.lexed.ok(), "import statement should lex");
    require(result.lexed.tokens.size() == 4,
            "import statement should produce three tokens and an end marker");
    require(result.lexed.tokens[0].kind == pebble::TokenKind::Import,
            "'import' should be an import token");
    require(result.lexed.tokens[1].kind == pebble::TokenKind::Package,
            "'package' should be a package token");
    require(result.lexed.tokens[2].kind == pebble::TokenKind::Identifier,
            "the package name should be an identifier");
    require(result.lexed.tokens[2].lexeme == "math",
            "the package name should be preserved");
}

void testImportDeclarationIsParsed() {
    Parsed result = parse("import package math\nfn main() { print(1); }\n");

    require(result.parsed.ok(), "import statement should parse");
    require(result.parsed.root->children.size() == 2,
            "the program should hold the import and the function");

    const pebble::Node& import = *result.parsed.root->children[0];

    require(import.kind == pebble::NodeKind::ImportDeclaration,
            "the import should be represented in the AST");
    require(import.text == "math", "the import should name the package");
}

void testMemberExpressionIsParsed() {
    Parsed result = parse(
        "import package math\n"
        "fn main() { let value = math.randint(1, 10); }\n"
    );

    require(result.parsed.ok(), "package call should parse");

    const pebble::Node& body = *result.parsed.root->children[1]->children[0];
    const pebble::Node& declaration = *body.children[0];
    const pebble::Node& call = *declaration.children[0];

    require(call.kind == pebble::NodeKind::CallExpression,
            "the call should be represented in the AST");

    const pebble::Node& member = *call.children[0];

    require(member.kind == pebble::NodeKind::MemberExpression,
            "the callee should be a member expression");
    require(member.text == "randint", "the member should name the function");
    require(member.children.size() == 1,
            "the member should reference its package");
    require(member.children[0]->kind == pebble::NodeKind::NameExpression,
            "the package should be referenced by name");
    require(member.children[0]->text == "math",
            "the package name should be preserved");
    require(call.children.size() == 3,
            "the call should carry both arguments");
}

void testValidPackageCallTypeChecks() {
    const auto result = check(
        "import package math\n"
        "fn main() {\n"
        "    let value = math.randint(1, 10);\n"
        "    print(value);\n"
        "}\n"
    );

    require(result.ok(), "a valid package call should pass type checking");
}

void testUnknownPackageIsRejected() {
    const auto result = check(
        "import package physics\n"
        "fn main() { print(1); }\n"
    );

    require(!result.ok(), "an unknown package should be rejected");
    require(containsMessage(result, "Unknown package 'physics'"),
            "the diagnostic should name the unknown package");
}

void testPackageUseWithoutImportIsRejected() {
    const auto result = check(
        "fn main() { let value = math.randint(1, 10); print(value); }\n"
    );

    require(!result.ok(), "using a package without importing it is rejected");
    require(containsMessage(result, "Unknown package 'math'"),
            "the diagnostic should report the missing package");
}

void testDuplicateImportIsRejected() {
    const auto result = check(
        "import package math\n"
        "import package math\n"
        "fn main() { print(1); }\n"
    );

    require(!result.ok(), "importing a package twice should be rejected");
    require(containsMessage(result, "imported more than once"),
            "the diagnostic should report the duplicate import");
}

void testUnknownPackageFunctionIsRejected() {
    const auto result = check(
        "import package math\n"
        "fn main() { let value = math.cbrt(2); print(value); }\n"
    );

    require(!result.ok(), "an unknown package function should be rejected");
    require(containsMessage(result, "has no function 'cbrt'"),
            "the diagnostic should name the missing function");
}

void testWrongPackageArgumentCountIsRejected() {
    const auto result = check(
        "import package math\n"
        "fn main() { let value = math.randint(1); print(value); }\n"
    );

    require(!result.ok(), "a wrong argument count should be rejected");
    require(containsMessage(result, "expects 2 argument(s), but got 1"),
            "the diagnostic should report the argument count");
}

void testWrongPackageArgumentTypeIsRejected() {
    const auto result = check(
        "import package math\n"
        "fn main() { let value = math.randint(1.5, 10); print(value); }\n"
    );

    require(!result.ok(), "a wrong argument type should be rejected");
    require(containsMessage(result, "expects 'Int', but got 'Float'"),
            "the diagnostic should report the argument type");
}

void testPackageFunctionValueIsRejected() {
    const auto result = check(
        "import package math\n"
        "fn main() { let value = math.randint; print(value); }\n"
    );

    require(!result.ok(), "using a package function as a value is rejected");
    require(containsMessage(result, "can only be used in a direct call"),
            "the diagnostic should explain the restriction");
}

void testImportInsideFunctionIsRejected() {
    const auto result = check(
        "fn main() { import package math; print(1); }\n"
    );

    require(!result.ok(), "importing inside a function should be rejected");
    require(containsMessage(result, "only allowed at the top level"),
            "the diagnostic should explain the restriction");
}

void testPackageRuntimeIsGenerated() {
    Parsed result = parse(
        "import package math\n"
        "fn main() { let value = math.randint(1, 10); print(value); }\n"
    );

    require(result.parsed.ok(), "package program should parse");

    const auto checked =
        pebble::TypeChecker{}.check(*result.parsed.root);

    require(checked.ok(), "package program should pass type checking");

    const std::string generated =
        pebble::CodeGenerator{}.generate(*result.parsed.root);

    require(generated.find("pebble_math_randint(1, 10)") != std::string::npos,
            "the package call should be emitted");
    require(generated.find("static long long pebble_math_randint(") !=
            std::string::npos,
            "the package runtime should be emitted");
    require(generated.find("pebble_rng_next") != std::string::npos,
            "the random source should be emitted");
}

void testFloatPackageFunctionsTypeCheck() {
    const auto result = check(
        "import package math\n"
        "fn main() {\n"
        "    let root = math.sqrt(2.0);\n"
        "    let lower = math.floor(1.5);\n"
        "    let upper = math.ceil(1.5);\n"
        "    let rounded = math.round(1.5);\n"
        "    let fraction = math.random();\n"
        "    let circle = math.pi();\n"
        "    print(root, lower, upper, rounded, fraction, circle);\n"
        "}\n"
    );

    require(result.ok(), "the float package functions should type check");
}

void testIntArgumentsWidenToFloat() {
    const auto result = check(
        "import package math\n"
        "fn main() { print(math.sqrt(9)); }\n"
    );

    require(result.ok(), "an Int argument should widen to a Float");
}

void testFloatPackageRuntimeIsGenerated() {
    Parsed result = parse(
        "import package math\n"
        "fn main() { print(math.sqrt(2.0), math.floor(1.5), math.ceil(1.5), "
        "math.round(1.5), math.random(), math.pi()); }\n"
    );

    require(result.parsed.ok(), "package program should parse");

    const std::string generated =
        pebble::CodeGenerator{}.generate(*result.parsed.root);

    for (const std::string_view symbol : {
             "pebble_math_sqrt",
             "pebble_math_floor",
             "pebble_math_ceil",
             "pebble_math_round",
             "pebble_math_random",
             "pebble_math_pi"}) {
        require(generated.find(symbol) != std::string::npos,
                "the package runtime should be emitted");
    }

    require(generated.find("std::sqrt") != std::string::npos,
            "the square root should use the standard library");
}

void testRandomSourceIsSeededFromTheEnvironment() {
    Parsed result = parse(
        "import package math\n"
        "fn main() { print(math.random()); }\n"
    );

    require(result.parsed.ok(), "package program should parse");

    const std::string generated =
        pebble::CodeGenerator{}.generate(*result.parsed.root);

    require(generated.find("pebble_rng_seed") != std::string::npos,
            "the random source should be seeded from the environment");
    require(generated.find("pebble_rng_ready") != std::string::npos,
            "the random source should only seed itself once");
}

void testUnusedPackageRuntimeIsOmitted() {
    Parsed result = parse(
        "import package math\n"
        "fn main() { print(1); }\n"
    );

    require(result.parsed.ok(), "package program should parse");

    const std::string generated =
        pebble::CodeGenerator{}.generate(*result.parsed.root);

    require(generated.find("pebble_math_randint") == std::string::npos,
            "an unused package function should not be emitted");
    require(generated.find("pebble_rng_state") == std::string::npos,
            "an unused random source should not be emitted");
}

void testStringPackageFunctionsTypeCheck() {
    const auto result = check(
        "import package string\n"
        "fn main() {\n"
        "    let size = string.length(\"hello\");\n"
        "    let part = string.substring(\"hello\", 1, 3);\n"
        "    let head = string.left(\"hello\", 2);\n"
        "    let tail = string.right(\"hello\", 2);\n"
        "    let loud = string.upper(\"hello\");\n"
        "    let quiet = string.lower(\"HELLO\");\n"
        "    let trimmed = string.trim(\"  hello  \");\n"
        "    let has = string.contains(\"hello\", \"ell\");\n"
        "    let first = string.startswith(\"hello\", \"he\");\n"
        "    let last = string.endswith(\"hello\", \"lo\");\n"
        "    let index = string.indexof(\"hello\", \"l\");\n"
        "    let swapped = string.replace(\"a-b\", \"-\", \"+\");\n"
        "    let many = string.repeat(\"ab\", 2);\n"
        "    let number = string.toint(\"42\");\n"
        "    let fraction = string.tofloat(\"2.5\");\n"
        "    print(size, part, head, tail, loud, quiet, trimmed);\n"
        "    print(has, first, last, index, swapped, many, number, fraction);\n"
        "}\n"
    );

    require(result.ok(), "the string package functions should type check");
}

void testStringPackageRuntimeIsGenerated() {
    Parsed result = parse(
        "import package string\n"
        "fn main() { print(string.length(\"hello\"), string.upper(\"hello\")); }\n"
    );

    require(result.parsed.ok(), "package program should parse");

    const std::string generated =
        pebble::CodeGenerator{}.generate(*result.parsed.root);

    require(generated.find("pebble_string_length(std::string(\"hello\"))") !=
            std::string::npos,
            "the package call should be emitted");
    require(generated.find("static long long pebble_string_length(") !=
            std::string::npos,
            "the package runtime should be emitted");
    require(generated.find("static std::string pebble_string_upper(") !=
            std::string::npos,
            "the package runtime should be emitted");
    require(generated.find("pebble_string_replace") == std::string::npos,
            "an unused package function should not be emitted");
}

void testStringPackageRejectsWrongTypes() {
    const auto result = check(
        "import package string\n"
        "fn main() { print(string.length(42)); }\n"
    );

    require(!result.ok(), "a wrong argument type should fail");
    require(
        containsMessage(result, "expects 'String', but got 'Int'"),
        "the diagnostic should report the argument type"
    );
}

void testStringPackageRejectsUnknownFunction() {
    const auto result = check(
        "import package string\n"
        "fn main() { print(string.split(\"a,b\", \",\")); }\n"
    );

    require(!result.ok(), "an unknown function should fail");
    require(
        containsMessage(result, "has no function 'split'"),
        "the diagnostic should name the missing function"
    );
}

void testTimePackageFunctionsTypeCheck() {
    const auto result = check(
        "import package time\n"
        "fn main() {\n"
        "    let now = time.now();\n"
        "    let ticks = time.ticks();\n"
        "    let delta = time.delta();\n"
        "    time.sleep(0.5);\n"
        "    let stamp = time.unix();\n"
        "    print(now, ticks, delta, stamp);\n"
        "}\n"
    );

    require(result.ok(), "the time package functions should type check");
}

void testTimePackageRuntimeIsGenerated() {
    Parsed result = parse(
        "import package time\n"
        "fn main() {\n"
        "    print(time.now(), time.ticks(), time.delta(), time.unix());\n"
        "    time.sleep(0.5);\n"
        "}\n"
    );

    require(result.parsed.ok(), "time program should parse");

    const std::string generated =
        pebble::CodeGenerator{}.generate(*result.parsed.root);

    for (const std::string_view symbol : {
             "pebble_time_now",
             "pebble_time_ticks",
             "pebble_time_delta",
             "pebble_time_sleep",
             "pebble_time_unix"}) {
        require(generated.find(symbol) != std::string::npos,
                "the time package runtime should be emitted");
    }

    require(generated.find("std::chrono::steady_clock") != std::string::npos,
            "the clock should be a monotonic clock");
    require(generated.find("pebble_time_origin") != std::string::npos,
            "the clock should start at the start of the program");
}

void testUnusedTimeRuntimeIsOmitted() {
    Parsed result = parse(
        "import package time\n"
        "fn main() { print(1); }\n"
    );

    require(result.parsed.ok(), "time program should parse");

    const std::string generated =
        pebble::CodeGenerator{}.generate(*result.parsed.root);

    require(generated.find("pebble_time_now") == std::string::npos,
            "an unused package function should not be emitted");
    require(generated.find("pebble_time_origin") == std::string::npos,
            "an unused clock should not be emitted");
}

void testTimePackageRejectsWrongArgumentCount() {
    const auto result = check(
        "import package time\n"
        "fn main() { time.sleep(); }\n"
    );

    require(!result.ok(), "a wrong argument count should be rejected");
    require(containsMessage(result, "expects 1 argument(s), but got 0"),
            "the diagnostic should report the argument count");
}

void testTimePackageRejectsUnknownFunction() {
    const auto result = check(
        "import package time\n"
        "fn main() { print(time.frame()); }\n"
    );

    require(!result.ok(), "an unknown package function should be rejected");
    require(containsMessage(result, "has no function 'frame'"),
            "the diagnostic should name the missing function");
}

void testFilesystemPackageFunctionsTypeCheck() {
    const auto result = check(
        "import package filesystem\n"
        "fn main() {\n"
        "    let contents = filesystem.read(\"note.txt\");\n"
        "    filesystem.write(\"note.txt\", contents);\n"
        "    let there = filesystem.exists(\"note.txt\");\n"
        "    let names = filesystem.list(\"notes\");\n"
        "    filesystem.mkdir(\"notes\");\n"
        "    print(contents, there, names);\n"
        "}\n"
    );

    require(result.ok(), "the filesystem package functions should type check");
}

void testFilesystemPackageRuntimeIsGenerated() {
    Parsed result = parse(
        "import package filesystem\n"
        "fn main() {\n"
        "    filesystem.write(\"note.txt\", \"hello\");\n"
        "    print(filesystem.read(\"note.txt\"));\n"
        "    print(filesystem.exists(\"note.txt\"));\n"
        "    print(filesystem.list(\"notes\"));\n"
        "    filesystem.mkdir(\"notes\");\n"
        "}\n"
    );

    require(result.parsed.ok(), "filesystem program should parse");

    const std::string generated =
        pebble::CodeGenerator{}.generate(*result.parsed.root);

    for (const std::string_view symbol : {
             "pebble_filesystem_read",
             "pebble_filesystem_write",
             "pebble_filesystem_exists",
             "pebble_filesystem_list",
             "pebble_filesystem_mkdir"}) {
        require(generated.find(symbol) != std::string::npos,
                "the filesystem package runtime should be emitted");
    }

    require(generated.find("std::filesystem") != std::string::npos,
            "the filesystem should use the library of the system");
}

void testUnusedFilesystemRuntimeIsOmitted() {
    Parsed result = parse(
        "import package filesystem\n"
        "fn main() { print(1); }\n"
    );

    require(result.parsed.ok(), "filesystem program should parse");

    const std::string generated =
        pebble::CodeGenerator{}.generate(*result.parsed.root);

    require(generated.find("pebble_filesystem_read") == std::string::npos,
            "an unused package function should not be emitted");
    require(generated.find("pebble_filesystem_error") == std::string::npos,
            "an unused helper should not be emitted");
}

void testFilesystemPackageRejectsWrongArgumentCount() {
    const auto result = check(
        "import package filesystem\n"
        "fn main() { print(filesystem.read()); }\n"
    );

    require(!result.ok(), "a wrong argument count should be rejected");
    require(containsMessage(result, "expects 1 argument(s), but got 0"),
            "the diagnostic should report the argument count");
}

void testFilesystemPackageRejectsWrongArgumentType() {
    const auto result = check(
        "import package filesystem\n"
        "fn main() { filesystem.write(\"note.txt\", 42); }\n"
    );

    require(!result.ok(), "a wrong argument type should be rejected");
    require(containsMessage(result, "expects 'String', but got 'Int'"),
            "the diagnostic should report the argument type");
}

void testFilesystemPackageRejectsUnknownFunction() {
    const auto result = check(
        "import package filesystem\n"
        "fn main() { print(filesystem.remove(\"note.txt\")); }\n"
    );

    require(!result.ok(), "an unknown package function should be rejected");
    require(containsMessage(result, "has no function 'remove'"),
            "the diagnostic should name the missing function");
}

void testIntegerLiteralRangeIsChecked() {
    const auto result = check(
        "fn main() { let value = 99999999999999999999; print(value); }\n"
    );

    require(!result.ok(), "an out of range literal should be rejected");
    require(containsMessage(result, "out of range"),
            "the diagnostic should report the literal range");
}

void testMinimumIntegerLiteralIsAccepted() {
    const auto result = check(
        "fn main() { let value = -9223372036854775808; print(value); }\n"
    );

    require(result.ok(), "the smallest Int literal should be accepted");
}

void testPackageRegistryIsConsistent() {
    const auto& packages = pebble::builtinPackages();

    require(!packages.empty(), "the registry should not be empty");

    const pebble::Package* math = pebble::findBuiltinPackage("math");

    require(math != nullptr, "the math package should be registered");
    require(pebble::findBuiltinPackage("physics") == nullptr,
            "an unknown package should not resolve");

    const pebble::PackageFunction* randint =
        pebble::findPackageFunction(*math, "randint");

    require(randint != nullptr, "math.randint should be exported");
    require(randint->parameters.size() == 2,
            "math.randint should take two parameters");
    require(randint->parameters[0] == "Int" && randint->parameters[1] == "Int",
            "math.randint should take integer parameters");
    require(randint->returnType == "Int",
            "math.randint should return an integer");
    require(pebble::findPackageFunction(*math, "cbrt") == nullptr,
            "a missing export should not resolve");

    const pebble::Package* text = pebble::findBuiltinPackage("string");

    require(text != nullptr, "the string package should be registered");

    const pebble::PackageFunction* length =
        pebble::findPackageFunction(*text, "length");

    require(length != nullptr, "string.length should be exported");
    require(length->parameters.size() == 1,
            "string.length should take one parameter");
    require(length->parameters[0] == "String",
            "string.length should take a string parameter");
    require(length->returnType == "Int",
            "string.length should return an integer");
    require(pebble::findPackageFunction(*text, "split") == nullptr,
            "a missing export should not resolve");

    const pebble::Package* clock = pebble::findBuiltinPackage("time");

    require(clock != nullptr, "the time package should be registered");

    const pebble::PackageFunction* now =
        pebble::findPackageFunction(*clock, "now");

    require(now != nullptr, "time.now should be exported");
    require(now->parameters.empty(), "time.now should take no parameter");
    require(now->returnType == "Float",
            "time.now should return a float");

    const pebble::PackageFunction* sleep =
        pebble::findPackageFunction(*clock, "sleep");

    require(sleep != nullptr, "time.sleep should be exported");
    require(sleep->parameters.size() == 1,
            "time.sleep should take one parameter");
    require(sleep->parameters[0] == "Float",
            "time.sleep should take a float parameter");
    require(sleep->returnType == "Void",
            "time.sleep should not return a value");

    const pebble::PackageFunction* ticks =
        pebble::findPackageFunction(*clock, "ticks");

    require(ticks != nullptr, "time.ticks should be exported");
    require(ticks->returnType == "Int",
            "time.ticks should return an integer");

    require(pebble::findPackageFunction(*clock, "frame") == nullptr,
            "a missing export should not resolve");

    const pebble::Package* files = pebble::findBuiltinPackage("filesystem");

    require(files != nullptr, "the filesystem package should be registered");

    const pebble::PackageFunction* read =
        pebble::findPackageFunction(*files, "read");

    require(read != nullptr, "filesystem.read should be exported");
    require(read->parameters.size() == 1,
            "filesystem.read should take one parameter");
    require(read->parameters[0] == "String",
            "filesystem.read should take a string parameter");
    require(read->returnType == "String",
            "filesystem.read should return a string");

    const pebble::PackageFunction* write =
        pebble::findPackageFunction(*files, "write");

    require(write != nullptr, "filesystem.write should be exported");
    require(write->parameters.size() == 2,
            "filesystem.write should take two parameters");
    require(write->parameters[0] == "String" &&
            write->parameters[1] == "String",
            "filesystem.write should take string parameters");
    require(write->returnType == "Void",
            "filesystem.write should not return a value");

    const pebble::PackageFunction* exists =
        pebble::findPackageFunction(*files, "exists");

    require(exists != nullptr, "filesystem.exists should be exported");
    require(exists->returnType == "Bool",
            "filesystem.exists should return a boolean");

    const pebble::PackageFunction* list =
        pebble::findPackageFunction(*files, "list");

    require(list != nullptr, "filesystem.list should be exported");
    require(list->returnType == "String",
            "filesystem.list should return a string");

    const pebble::PackageFunction* mkdir =
        pebble::findPackageFunction(*files, "mkdir");

    require(mkdir != nullptr, "filesystem.mkdir should be exported");
    require(mkdir->returnType == "Void",
            "filesystem.mkdir should not return a value");

    require(pebble::findPackageFunction(*files, "remove") == nullptr,
            "a missing export should not resolve");
}

void testOpenGLPackageSurfaceIsTypedAndRegistered() {
    const pebble::Package* gl = pebble::findBuiltinPackage("OpenGL");
    require(gl != nullptr, "OpenGL package should be registered");
    const auto checkFunction = [gl](const char* name, std::size_t count,
                                    const char* result) {
        const pebble::PackageFunction* function =
            pebble::findPackageFunction(*gl, name);
        require(function != nullptr, "OpenGL API function should be registered");
        require(function->parameters.size() == count,
                "OpenGL API signature should have the documented arity");
        require(function->returnType == result,
                "OpenGL API function should have the documented return type");
        require(function->runtimeSymbol.starts_with("pebble_gl_"),
                "OpenGL API functions should bind to native runtime symbols");
    };
    checkFunction("Initialize", 0, "Bool");
    checkFunction("CreateProgram", 2, "Int");
    checkFunction("UniformMat4", 17, "Void");
    checkFunction("UploadArrayBuffer", 1, "Void");
    checkFunction("VertexAttribute", 4, "Void");
    checkFunction("DrawElements", 1, "Void");
    checkFunction("UploadTextureRgba", 3, "Void");
}

void testOpenGLTypeCheckingAndInvalidCalls() {
    const auto valid = check(
        "import package OpenGL\n"
        "fn main() {\n"
        "  let ready = OpenGL:Initialize()\n"
        "  let program = OpenGL:CreateProgram(\"vertex\", \"fragment\")\n"
        "  let location = OpenGL:UniformLocation(program, \"uTime\")\n"
        "  OpenGL:UniformFloat(location, 0.5)\n"
        "  OpenGL:UniformVec3(location, 0.0, 1.0, 2.0)\n"
        "  OpenGL:ClearColor(0.0, 0.0, 0.0, 1.0)\n"
        "  let texture = OpenGL:CreateTexture()\n"
        "  OpenGL:BindTexture(texture, 0)\n"
        "  OpenGL:UploadTextureRgba(1, 1, \"255,255,255,255\")\n"
        "  OpenGL:SetTextureSampling(\"nearest\", \"repeat\")\n"
        "  OpenGL:DeleteTexture(texture)\n"
        "  OpenGL:Shutdown()\n"
        "  print(ready)\n"
        "}\n"
    );
    require(valid.ok(), "valid OpenGL calls should type check");

    const auto wrongArgument = check(
        "import package OpenGL\n"
        "fn main() { OpenGL:Viewport(800, \"600\") }\n"
    );
    require(!wrongArgument.ok(), "OpenGL argument types should be checked");
    require(containsMessage(wrongArgument, "expects 'Int', but got 'String'"),
            "OpenGL type errors should report the expected argument type");

    const auto wrongName = check(
        "import package OpenGL\n"
        "fn main() { OpenGL:DrawCube() }\n"
    );
    require(!wrongName.ok(), "unimplemented OpenGL functions should be rejected");
    require(containsMessage(wrongName, "has no function 'DrawCube'"),
            "unknown OpenGL operations should name the missing function");
}

}

int main() {
    testImportKeywordsAreLexed();
    testImportDeclarationIsParsed();
    testMemberExpressionIsParsed();
    testValidPackageCallTypeChecks();
    testUnknownPackageIsRejected();
    testPackageUseWithoutImportIsRejected();
    testDuplicateImportIsRejected();
    testUnknownPackageFunctionIsRejected();
    testWrongPackageArgumentCountIsRejected();
    testWrongPackageArgumentTypeIsRejected();
    testPackageFunctionValueIsRejected();
    testImportInsideFunctionIsRejected();
    testPackageRuntimeIsGenerated();
    testFloatPackageFunctionsTypeCheck();
    testIntArgumentsWidenToFloat();
    testFloatPackageRuntimeIsGenerated();
    testRandomSourceIsSeededFromTheEnvironment();
    testUnusedPackageRuntimeIsOmitted();
    testStringPackageFunctionsTypeCheck();
    testStringPackageRuntimeIsGenerated();
    testStringPackageRejectsWrongTypes();
    testStringPackageRejectsUnknownFunction();
    testTimePackageFunctionsTypeCheck();
    testTimePackageRuntimeIsGenerated();
    testUnusedTimeRuntimeIsOmitted();
    testTimePackageRejectsWrongArgumentCount();
    testTimePackageRejectsUnknownFunction();
    testFilesystemPackageFunctionsTypeCheck();
    testFilesystemPackageRuntimeIsGenerated();
    testUnusedFilesystemRuntimeIsOmitted();
    testFilesystemPackageRejectsWrongArgumentCount();
    testFilesystemPackageRejectsWrongArgumentType();
    testFilesystemPackageRejectsUnknownFunction();
    testIntegerLiteralRangeIsChecked();
    testMinimumIntegerLiteralIsAccepted();
    testPackageRegistryIsConsistent();
    testOpenGLPackageSurfaceIsTypedAndRegistered();
    testOpenGLTypeCheckingAndInvalidCalls();

    std::cout << "Package tests passed.\n";
    return EXIT_SUCCESS;
}
