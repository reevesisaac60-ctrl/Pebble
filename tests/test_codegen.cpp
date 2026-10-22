#include "pebble/codegen.hpp"
#include "pebble/lexer.hpp"
#include "pebble/parser.hpp"
#include "pebble/package.hpp"
#include "pebble/type_checker.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

}

int main() {
    const pebble::Source source{
        "codegen-test.peb",
        "fn add(a: Int, b: Int) -> Int {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "fn logical_and(a: Bool, b: Bool) -> Bool {\n"
        "    return a && b;\n"
        "}\n"
        "\n"
        "fn logical_or(a: Bool, b: Bool) -> Bool {\n"
        "    return a || b;\n"
        "}\n"
        "\n"
        "fn main() {\n"
        "    let name: String = \"Pebble\";\n"
        "   let greeting = \"Hello\";\n"
        "   let target = \" world\";\n"
        "   print(greeting + target);\n"
        "   print(\"Pebble \" + \"works\");\n"
        "    var score = add(20, 22);\n"
        "    score = score + 1;\n"
        "    print(name, score);\n"
        "    if score > 0 && true { print(\"positive\"); }\n"
        "    while false { print(\"unreachable\"); }\n"
        "}\n"
    };

    const pebble::LexResult lexed = pebble::Lexer{}.scan(source);
    require(lexed.ok(), "codegen sample should lex");

    const pebble::ParseResult parsed = pebble::Parser{}.parse(lexed);
    require(parsed.ok() && parsed.root != nullptr, "codegen sample should parse");

    const pebble::TypeCheckResult checked =
        pebble::TypeChecker{}.check(*parsed.root);
    require(checked.ok(), "codegen sample should pass type checking");

    const std::string generated =
        pebble::CodeGenerator{}.generate(*parsed.root);

    require(
        generated.find("pebble_fn_add(long long pebble_var_a, long long pebble_var_b)") !=
            std::string::npos,
        "generated code should declare typed functions"
    );

    require(
        generated.find("return (pebble_var_a + pebble_var_b);") !=
            std::string::npos,
        "generated code should preserve arithmetic and returns"
    );

    require(
        generated.find("pebble_fn_add(20, 22)") != std::string::npos,
        "generated code should preserve function calls"
    );

    require(
        generated.find("pebble_print(pebble_var_name, pebble_var_score)") !=
            std::string::npos,
        "generated code should map print calls"
  );

    require(
        generated.find("std::string(\"Hello\")") != std::string::npos,
        "string literals should generate std::string expressions"
    );

    require(
        generated.find("(pebble_var_greeting + pebble_var_target)") !=
            std::string::npos,
        "string variables should use string concatenation"
    );

    require(
        generated.find(
            "(std::string(\"Pebble \") + std::string(\"works\"))"
        ) != std::string::npos,
        "string literals should concatenate in generated C++"
    );

    require(
        generated.find("(pebble_var_a && pebble_var_b)") != std::string::npos,
        "generated code should preserve logical AND"
    );

    require(
        generated.find("(pebble_var_a || pebble_var_b)") != std::string::npos,
        "generated code should preserve logical OR"
    );

    require(
        generated.find("if (((pebble_var_score > 0) && true))") !=
            std::string::npos,
        "generated code should preserve if conditions"
    );

    require(
        generated.find("while (false)") != std::string::npos,
        "generated code should preserve while conditions"
    );

    const pebble::Source nullSource{
        "codegen-null.peb",
        "fn main() {\n"
        "    let empty = null;\n"
        "    print(empty, null, empty == null);\n"
        "}\n"
    };

    const pebble::LexResult nullLexed = pebble::Lexer{}.scan(nullSource);
    require(nullLexed.ok(), "null program should lex");

    const pebble::ParseResult nullParsed = pebble::Parser{}.parse(nullLexed);
    require(nullParsed.ok() && nullParsed.root != nullptr,
            "null program should parse");

    const pebble::TypeCheckResult nullChecked =
        pebble::TypeChecker{}.check(*nullParsed.root);
    require(nullChecked.ok(), "null program should pass type checking");

    const std::string nullGenerated =
        pebble::CodeGenerator{}.generate(*nullParsed.root);

    require(
        nullGenerated.find("static_cast<const char *>(nullptr)") !=
            std::string::npos,
        "the null literal should become a null string pointer"
    );

    require(
        nullGenerated.find("pebble_print_value(const char* value)") !=
            std::string::npos,
        "the print helper should know how to print a null value"
    );

    const pebble::Source glSource{
        "codegen-gl.peb",
        "import package OpenGL\n"
        "fn main() {\n"
        "    let ready = OpenGL:Initialize();\n"
        "    if ready { OpenGL:Clear(); }\n"
        "}\n"
    };
    const pebble::LexResult glLexed = pebble::Lexer{}.scan(glSource);
    require(glLexed.ok(), "OpenGL API sample should lex");
    const pebble::ParseResult glParsed = pebble::Parser{}.parse(glLexed);
    require(glParsed.ok() && glParsed.root != nullptr,
            "OpenGL API sample should parse");
    const pebble::TypeCheckResult glChecked =
        pebble::TypeChecker{}.check(*glParsed.root);
    require(glChecked.ok(), "OpenGL API sample should pass type checking");
    const std::string glGenerated =
        pebble::CodeGenerator{}.generate(*glParsed.root);
    require(glGenerated.find("pebble_gl_initialize()") != std::string::npos,
            "OpenGL package calls should resolve to native runtime functions");
    require(glGenerated.find("pebble_gl_clear()") != std::string::npos,
            "OpenGL clear should have a runtime implementation");
    require(generated.find("PEBBLE_GL_VERTEX_SHADER") == std::string::npos,
            "programs that do not import OpenGL should omit its runtime");
    require(glGenerated.find("shader compilation failed") != std::string::npos &&
                glGenerated.find("program link failed") != std::string::npos &&
                glGenerated.find("invalid buffer handle") != std::string::npos &&
                glGenerated.find("invalid texture handle") != std::string::npos &&
                glGenerated.find("RGBA texture data length must equal") != std::string::npos &&
                glGenerated.find("invalid uniform handle") != std::string::npos &&
                glGenerated.find("use the uniform's program before updating it") !=
                    std::string::npos,
            "generated OpenGL runtime should report shader and handle failures");
    const pebble::Package* glPackage = pebble::findBuiltinPackage("OpenGL");
    require(glPackage != nullptr, "OpenGL package metadata should exist");
    for (const pebble::PackageFunction& function : glPackage->functions) {
        require(glGenerated.find(function.runtimeSymbol + "(") != std::string::npos,
                "each declared OpenGL runtime symbol should have an implementation");
    }

    std::cout << "Code generator tests passed.\n";
    return EXIT_SUCCESS;
}
