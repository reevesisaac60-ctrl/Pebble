#include "pebble/codegen.hpp"
#include "pebble/package.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pebble {
namespace {

std::string cppType(std::string_view type) {
    if (type == "Int") {
        return "long long";
    }

    if (type == "Float") {
        return "double";
    }

    if (type == "Bool") {
        return "bool";
    }

    if (type == "String") {
        return "std::string";
    }

    if (type == "Null") {
        return "const char *";
    }

    if (type == "Void") {
        return "void";
    }

    throw std::runtime_error(
        "Cannot generate C++ for Pebble type '" + std::string(type) + "'."
    );
}

std::string variableName(std::string_view name) {
    return "pebble_var_" + std::string(name);
}

std::string functionName(std::string_view name) {
    std::string result = "pebble_fn_";

    for (const char character : name) {
        result += (character == '.') ? '_' : character;
    }

    return result;
}

const Node* findChild(const Node& node, NodeKind kind) {
    for (const auto& child : node.children) {
        if (child->kind == kind) {
            return child.get();
        }
    }

    return nullptr;
}

std::string declaredType(const Node& node) {
    const Node* type = findChild(node, NodeKind::TypeName);
    return type == nullptr ? std::string{} : type->text;
}

const Node* initializer(const Node& node) {
    for (const auto& child : node.children) {
        if (child->kind != NodeKind::TypeName) {
            return child.get();
        }
    }

    return nullptr;
}

std::string functionReturnType(const Node& function) {
    const Node* type = findChild(function, NodeKind::TypeName);
    return type == nullptr ? "Void" : type->text;
}

std::string parameterType(const Node& parameter) {
    const Node* type = findChild(parameter, NodeKind::TypeName);

    if (type == nullptr) {
        throw std::runtime_error(
            "Cannot generate C++ for a parameter without a type."
        );
    }

    return type->text;
}

bool isMinimumIntegerLiteral(const Node& node) {
    if (node.kind != NodeKind::UnaryExpression || node.text != "-" ||
        node.children.size() != 1) {
        return false;
    }

    const Node& operand = *node.children.front();

    return operand.kind == NodeKind::LiteralExpression &&
        operand.text == "9223372036854775808";
}

std::string cppStringBody(const std::string& text) {
    std::string result;

    for (const char character : text) {
        const unsigned char byte = static_cast<unsigned char>(character);

        if (character == '\\' || character == '"') {
            result += '\\';
            result += character;
        } else if (byte >= 0x20 && byte < 0x7F && character != '?') {
            result += character;
        } else {
            result += '\\';
            result += static_cast<char>('0' + ((byte >> 6) & 7));
            result += static_cast<char>('0' + ((byte >> 3) & 7));
            result += static_cast<char>('0' + (byte & 7));
        }
    }

    return result;
}

class Generator {
public:
    explicit Generator(const CodegenOptions& options) : options_(options) {}

    std::string generate(const Node& program) {
        if (program.kind != NodeKind::Program) {
            throw std::runtime_error("Expected a Pebble program AST.");
        }

        program_ = &program;
        collectImports(program);
        collectPackageUsage(program);

        output_ = buildPrelude();

        for (const auto& child : program.children) {
            if (child->kind == NodeKind::ImportDeclaration) {
                continue;
            }

            if (child->kind != NodeKind::Function) {
                throw std::runtime_error(
                    "Unsupported top-level syntax node."
                );
            }

            emitFunctionPrototype(*child);
        }

        output_ += "\n";

        for (const auto& child : program.children) {
            if (child->kind != NodeKind::Function) {
                continue;
            }

            emitFunction(*child);
            output_ += "\n";
        }

        return output_;
    }

private:
    std::string output_;
    std::size_t indent_ = 0;
    bool inMain_ = false;
    [[nodiscard]] std::string emitResourceLookup() const {
        std::string result =
            "static const char* const pebble_resource_roots[] = {\n";

        for (const std::string& root : options_.resourceRoots) {
            result += "    \"" + cppStringBody(root) + "\",\n";
        }

        result +=
            "    nullptr\n"
            "};\n"
            "\n"
            "static std::vector<std::filesystem::path>\n"
            "pebble_resource_search_roots() {\n"
            "    std::vector<std::filesystem::path> roots;\n"
            "    const char* extra = std::getenv(\"PEBBLE_RESOURCE_PATH\");\n"
            "\n"
            "    if (extra != nullptr) {\n"
            "#if defined(_WIN32)\n"
            "        const char separator = ';';\n"
            "#else\n"
            "        const char separator = ':';\n"
            "#endif\n"
            "        const std::string list = extra;\n"
            "        std::size_t begin = 0;\n"
            "\n"
            "        while (begin <= list.size()) {\n"
            "            std::size_t end = list.find(separator, begin);\n"
            "\n"
            "            if (end == std::string::npos) {\n"
            "                end = list.size();\n"
            "            }\n"
            "\n"
            "            if (end > begin) {\n"
            "                roots.emplace_back(list.substr(begin, end - begin));\n"
            "            }\n"
            "\n"
            "            begin = end + 1;\n"
            "        }\n"
            "    }\n"
            "\n"
            "    for (const char* const* root = pebble_resource_roots;\n"
            "         *root != nullptr; ++root) {\n"
            "        roots.emplace_back(*root);\n"
            "    }\n"
            "\n"
            "    return roots;\n"
            "}\n"
            "\n"
            "static bool pebble_filesystem_open_resource(\n"
            "    const std::string& path,\n"
            "    std::ifstream& file\n"
            ") {\n"
            "    const std::filesystem::path relative(path);\n"
            "\n"
            "    if (relative.is_absolute() || relative.has_root_name()) {\n"
            "        return false;\n"
            "    }\n"
            "\n"
            "    for (const std::filesystem::path& root :\n"
            "         pebble_resource_search_roots()) {\n"
            "        std::error_code code;\n"
            "        const std::filesystem::path candidate = root / relative;\n"
            "\n"
            "        if (!std::filesystem::is_regular_file(candidate, code)) {\n"
            "            continue;\n"
            "        }\n"
            "\n"
            "        file.open(candidate, std::ios::binary);\n"
            "\n"
            "        if (file) {\n"
            "            return true;\n"
            "        }\n"
            "\n"
            "        file.clear();\n"
            "    }\n"
            "\n"
            "    return false;\n"
            "}\n"
            "\n";

        return result;
    }

    const Node* program_ = nullptr;
    CodegenOptions options_;
    std::string currentPackage_;
    std::vector<const Package*> imports_;
    std::vector<const PackageFunction*> packageFunctions_;

    void collectImports(const Node& program) {
        for (const auto& child : program.children) {
            if (child->kind != NodeKind::ImportDeclaration) {
                continue;
            }

            if (child->local) {
                continue;
            }

            const Package* package = findBuiltinPackage(child->text);

            if (package != nullptr) {
                imports_.push_back(package);
            }
        }
    }

    void collectPackageUsage(const Node& node) {
        if (node.kind == NodeKind::CallExpression &&
            !node.children.empty() &&
            node.children.front()->kind == NodeKind::MemberExpression &&
            !node.children.front()->children.empty()) {
            const Node& base = *node.children.front()->children.front();

            if (base.kind == NodeKind::NameExpression) {
                const Package* package = findImportedPackage(base.text);

                if (package != nullptr) {
                    const PackageFunction* function =
                        findPackageFunction(*package, node.children.front()->text);

                    if (function != nullptr) {
                        addPackageFunction(function);
                    }
                }
            }
        }

        for (const auto& child : node.children) {
            collectPackageUsage(*child);
        }
    }

    void addPackageFunction(const PackageFunction* function) {
        for (const PackageFunction* existing : packageFunctions_) {
            if (existing->runtimeSymbol == function->runtimeSymbol) {
                return;
            }
        }

        packageFunctions_.push_back(function);
    }

    [[nodiscard]] const Package* findImportedPackage(
        const std::string& name
    ) const {
        for (const Package* package : imports_) {
            if (package->name == name) {
                return package;
            }
        }

        return nullptr;
    }

    [[nodiscard]] std::string packageOf(const std::string& name) const {
        const std::size_t separator = name.find('.');

        if (separator == std::string::npos) {
            return {};
        }

        return name.substr(0, separator);
    }

    [[nodiscard]] const Node* findProgramFunction(
        const std::string& name
    ) const {
        for (const auto& child : program_->children) {
            if (child->kind == NodeKind::Function && child->text == name) {
                return child.get();
            }
        }

        return nullptr;
    }

    [[nodiscard]] std::string packageCallTarget(
        const Node& callee
    ) const {
        if (callee.children.empty() ||
            callee.children.front()->kind != NodeKind::NameExpression) {
            return {};
        }

        const std::string& packageName = callee.children.front()->text;

        if (findImportedPackage(packageName) == nullptr) {
            const Node* function =
                findProgramFunction(packageName + "." + callee.text);

            return function == nullptr ? std::string{} : functionName(function->text);
        }

        const PackageFunction* function =
            findPackageFunction(*findImportedPackage(packageName), callee.text);

        return function == nullptr ? std::string{} : function->runtimeSymbol;
    }

    [[nodiscard]] const PackageFunction* resolvePackageFunction(
        const Node& callee
    ) const {
        if (callee.children.empty() ||
            callee.children.front()->kind != NodeKind::NameExpression) {
            return nullptr;
        }

        const Package* package =
            findImportedPackage(callee.children.front()->text);

        if (package == nullptr) {
            return nullptr;
        }

        return findPackageFunction(*package, callee.text);
    }

    [[nodiscard]] std::string buildPrelude() const {
        std::string prelude =
            "#include <cerrno>\n"
            "#include <cctype>\n"
            "#include <chrono>\n"
            "#include <cmath>\n"
            "#include <cstdint>\n"
            "#include <GLFW/glfw3.h>\n"
            "#include <cstdlib>\n"
            "#include <iostream>\n"
            "#include <string>\n"
            "#include <thread>\n"
            "#include <utility>\n"
            "\n";

        if (usesRuntimeSymbol("pebble_filesystem_read") ||
            usesRuntimeSymbol("pebble_filesystem_write") ||
            usesRuntimeSymbol("pebble_filesystem_exists") ||
            usesRuntimeSymbol("pebble_filesystem_list") ||
            usesRuntimeSymbol("pebble_filesystem_mkdir")) {
            prelude +=
                "#include <filesystem>\n"
                "#include <fstream>\n"
                "#include <iterator>\n"
                "#include <vector>\n"
                "\n";
        }

        for (const PackageFunction* function : packageFunctions_) {
            if (function->runtimeSymbol.starts_with("pebble_gl_")) {
                prelude +=
                    "#include <cstddef>\n"
                    "#include <sstream>\n"
                    "#include <thread>\n"
                    "#include <unordered_map>\n"
                    "#include <vector>\n"
                    "\n";
                break;
            }
        }

        prelude +=
            "inline void pebble_print_value(bool value) {\n"
            "    std::cout << (value ? \"true\" : \"false\");\n"
            "}\n"
            "\n"
            "inline void pebble_print_value(const char* value) {\n"
            "    std::cout << (value == nullptr ? \"null\" : value);\n"
            "}\n"
            "\n"
            "template <typename T>\n"
            "void pebble_print_value(const T& value) {\n"
            "    std::cout << value;\n"
            "}\n"
            "\n"
            "template <typename... Args>\n"
            "void pebble_print(const Args&... args) {\n"
            "    bool first = true;\n"
            "    ((std::cout << (std::exchange(first, false) ? \"\" : \" \"), "
            "pebble_print_value(args)), ...);\n"
            "    std::cout << '\\n';\n"
            "}\n"
            "\n";

        prelude += packageRuntime();
        return prelude;
    }

    [[nodiscard]] bool usesRuntimeSymbol(
        const std::string& symbol
    ) const {
        for (const PackageFunction* function : packageFunctions_) {
            if (function->runtimeSymbol == symbol) {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] std::string packageRuntime() const {
        std::string runtime;

        if (usesRuntimeSymbol("pebble_math_randint") ||
            usesRuntimeSymbol("pebble_math_random") ||
            usesRuntimeSymbol("pebble_math_randomseed")) {
            runtime +=
                "static std::uint64_t pebble_rng_state = 0;\n"
                "static bool pebble_rng_ready = false;\n"
                "\n"
                "static std::uint64_t pebble_rng_seed() {\n"
                "    const auto ticks = "
                "std::chrono::high_resolution_clock::now()\n"
                "        .time_since_epoch().count();\n"
                "    std::uint64_t seed = static_cast<std::uint64_t>(ticks);\n"
                "    seed ^= static_cast<std::uint64_t>(\n"
                "        reinterpret_cast<std::uintptr_t>(&pebble_rng_state));\n"
                "    return seed;\n"
                "}\n"
                "\n"
                "static inline std::uint64_t pebble_rng_next() {\n"
                "    if (!pebble_rng_ready) {\n"
                "        pebble_rng_state = pebble_rng_seed();\n"
                "        pebble_rng_ready = true;\n"
                "    }\n"
                "\n"
                "    std::uint64_t z = "
                "(pebble_rng_state += UINT64_C(0x9E3779B97F4A7C15));\n"
                "    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);\n"
                "    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);\n"
                "    return z ^ (z >> 31);\n"
                "}\n"
                "\n";
        }

        if (usesRuntimeSymbol("pebble_time_now") ||
            usesRuntimeSymbol("pebble_time_ticks")) {
            runtime +=
                "static const std::chrono::steady_clock::time_point "
                "pebble_time_origin = std::chrono::steady_clock::now();\n"
                "\n";
        }

        if (usesRuntimeSymbol("pebble_filesystem_read") ||
            usesRuntimeSymbol("pebble_filesystem_write") ||
            usesRuntimeSymbol("pebble_filesystem_exists") ||
            usesRuntimeSymbol("pebble_filesystem_list") ||
            usesRuntimeSymbol("pebble_filesystem_mkdir")) {
            runtime +=
                "static void pebble_filesystem_error(\n"
                "    const std::string& message\n"
                ") {\n"
                "    std::cerr << \"Pebble runtime error: \" << message << "
                "'\\n';\n"
                "    std::exit(1);\n"
                "}\n"
                "\n";
        }

        if (usesRuntimeSymbol("pebble_filesystem_read")) {
            runtime += emitResourceLookup();
        }

        bool usesOpenGL = false;
        for (const PackageFunction* function : packageFunctions_) {
            if (function->runtimeSymbol.starts_with("pebble_gl_")) {
                usesOpenGL = true;
                break;
            }
        }
        if (usesOpenGL) {
            runtime += R"PBS(
using PebbleGLenum = unsigned int;
using PebbleGLuint = unsigned int;
using PebbleGLint = int;
using PebbleGLsizei = int;
using PebbleGLboolean = unsigned char;
using PebbleGLsizeiptr = std::ptrdiff_t;
using PebbleGLfloat = float;
using PebbleGLchar = char;
template <typename T> static T pebble_gl_proc(const char* name) {
    return reinterpret_cast<T>(glfwGetProcAddress(name));
}
#define PEBBLE_GL_PROC(name, result, args) using PebbleGL_##name = result (*) args; static PebbleGL_##name p##name = nullptr
PEBBLE_GL_PROC(glCreateShader, PebbleGLuint, (PebbleGLenum));
PEBBLE_GL_PROC(glShaderSource, void, (PebbleGLuint, PebbleGLsizei, const PebbleGLchar* const*, const PebbleGLint*));
PEBBLE_GL_PROC(glCompileShader, void, (PebbleGLuint));
PEBBLE_GL_PROC(glGetShaderiv, void, (PebbleGLuint, PebbleGLenum, PebbleGLint*));
PEBBLE_GL_PROC(glGetShaderInfoLog, void, (PebbleGLuint, PebbleGLsizei, PebbleGLsizei*, PebbleGLchar*));
PEBBLE_GL_PROC(glDeleteShader, void, (PebbleGLuint));
PEBBLE_GL_PROC(glCreateProgram, PebbleGLuint, ());
PEBBLE_GL_PROC(glAttachShader, void, (PebbleGLuint, PebbleGLuint));
PEBBLE_GL_PROC(glLinkProgram, void, (PebbleGLuint));
PEBBLE_GL_PROC(glGetProgramiv, void, (PebbleGLuint, PebbleGLenum, PebbleGLint*));
PEBBLE_GL_PROC(glGetProgramInfoLog, void, (PebbleGLuint, PebbleGLsizei, PebbleGLsizei*, PebbleGLchar*));
PEBBLE_GL_PROC(glDeleteProgram, void, (PebbleGLuint));
PEBBLE_GL_PROC(glUseProgram, void, (PebbleGLuint));
PEBBLE_GL_PROC(glGetUniformLocation, PebbleGLint, (PebbleGLuint, const PebbleGLchar*));
PEBBLE_GL_PROC(glUniform1i, void, (PebbleGLint, PebbleGLint));
PEBBLE_GL_PROC(glUniform1f, void, (PebbleGLint, PebbleGLfloat));
PEBBLE_GL_PROC(glUniform2f, void, (PebbleGLint, PebbleGLfloat, PebbleGLfloat));
PEBBLE_GL_PROC(glUniform3f, void, (PebbleGLint, PebbleGLfloat, PebbleGLfloat, PebbleGLfloat));
PEBBLE_GL_PROC(glUniform4f, void, (PebbleGLint, PebbleGLfloat, PebbleGLfloat, PebbleGLfloat, PebbleGLfloat));
PEBBLE_GL_PROC(glUniformMatrix4fv, void, (PebbleGLint, PebbleGLsizei, PebbleGLboolean, const PebbleGLfloat*));
PEBBLE_GL_PROC(glGenVertexArrays, void, (PebbleGLsizei, PebbleGLuint*));
PEBBLE_GL_PROC(glBindVertexArray, void, (PebbleGLuint));
PEBBLE_GL_PROC(glDeleteVertexArrays, void, (PebbleGLsizei, const PebbleGLuint*));
PEBBLE_GL_PROC(glGenBuffers, void, (PebbleGLsizei, PebbleGLuint*));
PEBBLE_GL_PROC(glBindBuffer, void, (PebbleGLenum, PebbleGLuint));
PEBBLE_GL_PROC(glBufferData, void, (PebbleGLenum, PebbleGLsizeiptr, const void*, PebbleGLenum));
PEBBLE_GL_PROC(glDeleteBuffers, void, (PebbleGLsizei, const PebbleGLuint*));
PEBBLE_GL_PROC(glEnableVertexAttribArray, void, (PebbleGLuint));
PEBBLE_GL_PROC(glVertexAttribPointer, void, (PebbleGLuint, PebbleGLint, PebbleGLenum, PebbleGLboolean, PebbleGLsizei, const void*));
PEBBLE_GL_PROC(glDrawElements, void, (PebbleGLenum, PebbleGLsizei, PebbleGLenum, const void*));
PEBBLE_GL_PROC(glDrawArrays, void, (PebbleGLenum, PebbleGLint, PebbleGLsizei));
PEBBLE_GL_PROC(glEnable, void, (PebbleGLenum));
PEBBLE_GL_PROC(glViewport, void, (PebbleGLint, PebbleGLint, PebbleGLsizei, PebbleGLsizei));
PEBBLE_GL_PROC(glClearColor, void, (PebbleGLfloat, PebbleGLfloat, PebbleGLfloat, PebbleGLfloat));
PEBBLE_GL_PROC(glClear, void, (unsigned int));
PEBBLE_GL_PROC(glGenTextures, void, (PebbleGLsizei, PebbleGLuint*));
PEBBLE_GL_PROC(glActiveTexture, void, (PebbleGLenum));
PEBBLE_GL_PROC(glBindTexture, void, (PebbleGLenum, PebbleGLuint));
PEBBLE_GL_PROC(glTexImage2D, void, (PebbleGLenum, PebbleGLint, PebbleGLint, PebbleGLsizei, PebbleGLsizei, PebbleGLint, PebbleGLenum, PebbleGLenum, const void*));
PEBBLE_GL_PROC(glTexParameteri, void, (PebbleGLenum, PebbleGLenum, PebbleGLint));
PEBBLE_GL_PROC(glDeleteTextures, void, (PebbleGLsizei, const PebbleGLuint*));
#undef PEBBLE_GL_PROC
static constexpr PebbleGLenum PEBBLE_GL_VERTEX_SHADER = 0x8B31U;
static constexpr PebbleGLenum PEBBLE_GL_FRAGMENT_SHADER = 0x8B30U;
static constexpr PebbleGLenum PEBBLE_GL_COMPILE_STATUS = 0x8B81U;
static constexpr PebbleGLenum PEBBLE_GL_LINK_STATUS = 0x8B82U;
static constexpr PebbleGLenum PEBBLE_GL_INFO_LOG_LENGTH = 0x8B84U;
static constexpr PebbleGLenum PEBBLE_GL_ARRAY_BUFFER = 0x8892U;
static constexpr PebbleGLenum PEBBLE_GL_ELEMENT_ARRAY_BUFFER = 0x8893U;
static constexpr PebbleGLenum PEBBLE_GL_STATIC_DRAW = 0x88E4U;
static constexpr PebbleGLenum PEBBLE_GL_FLOAT = 0x1406U;
static constexpr PebbleGLenum PEBBLE_GL_UNSIGNED_INT = 0x1405U;
static constexpr PebbleGLenum PEBBLE_GL_TRIANGLES = 0x0004U;
static constexpr PebbleGLenum PEBBLE_GL_COLOR_BUFFER_BIT = 0x00004000U;
static constexpr PebbleGLenum PEBBLE_GL_DEPTH_BUFFER_BIT = 0x00000100U;
static constexpr PebbleGLenum PEBBLE_GL_DEPTH_TEST = 0x0B71U;
static constexpr PebbleGLenum PEBBLE_GL_TEXTURE_2D = 0x0DE1U;
static constexpr PebbleGLenum PEBBLE_GL_TEXTURE0 = 0x84C0U;
static constexpr PebbleGLenum PEBBLE_GL_RGBA = 0x1908U;
static constexpr PebbleGLenum PEBBLE_GL_UNSIGNED_BYTE = 0x1401U;
static constexpr PebbleGLenum PEBBLE_GL_TEXTURE_MIN_FILTER = 0x2801U;
static constexpr PebbleGLenum PEBBLE_GL_TEXTURE_MAG_FILTER = 0x2800U;
static constexpr PebbleGLenum PEBBLE_GL_TEXTURE_WRAP_S = 0x2802U;
static constexpr PebbleGLenum PEBBLE_GL_TEXTURE_WRAP_T = 0x2803U;
static constexpr PebbleGLint PEBBLE_GL_NEAREST = 0x2600;
static constexpr PebbleGLint PEBBLE_GL_LINEAR = 0x2601;
static constexpr PebbleGLint PEBBLE_GL_REPEAT = 0x2901;
static constexpr PebbleGLint PEBBLE_GL_CLAMP_TO_EDGE = 0x812F;
static bool pebble_gl_ready = false;
static std::thread::id pebble_gl_thread;
static long long pebble_gl_next_handle = 1;
static std::unordered_map<long long, PebbleGLuint> pebble_gl_programs;
static std::unordered_map<long long, PebbleGLuint> pebble_gl_arrays;
static std::unordered_map<long long, PebbleGLuint> pebble_gl_buffers;
static std::unordered_map<long long, std::pair<long long, PebbleGLint>> pebble_gl_uniforms;
static std::unordered_map<long long, PebbleGLuint> pebble_gl_textures;
static long long pebble_gl_bound_array = 0;
static long long pebble_gl_bound_buffer = 0;
static long long pebble_gl_bound_element = 0;
static std::unordered_map<long long, long long> pebble_gl_array_elements;
static long long pebble_gl_active_program = 0;
static long long pebble_gl_bound_texture = 0;
static void pebble_gl_fail(const std::string& message) {
    std::cerr << "Pebble OpenGL runtime error: " << message << '\n';
    std::exit(1);
}
static void pebble_gl_require() {
    if (!pebble_gl_ready || glfwGetCurrentContext() == nullptr)
        pebble_gl_fail("initialize OpenGL after creating a Window context");
    if (std::this_thread::get_id() != pebble_gl_thread)
        pebble_gl_fail("OpenGL calls must run on the context-owning thread");
}
static std::vector<float> pebble_gl_floats(const std::string& text) {
    std::vector<float> values;
    std::stringstream input(text);
    std::string item;
    while (std::getline(input, item, ',')) {
        const std::size_t first = item.find_first_not_of(" \t\r\n");
        const std::size_t last = item.find_last_not_of(" \t\r\n");
        if (first == std::string::npos) pebble_gl_fail("empty vertex-data element");
        const std::string token = item.substr(first, last - first + 1);
        std::size_t consumed = 0;
        float value = 0.0F;
        try { value = std::stof(token, &consumed); }
        catch (...) { pebble_gl_fail("vertex data must be comma-separated numbers"); }
        if (consumed != token.size()) pebble_gl_fail("invalid number in vertex data");
        values.push_back(value);
    }
    if (values.empty()) pebble_gl_fail("vertex data cannot be empty");
    return values;
}
static std::vector<unsigned int> pebble_gl_indices(const std::string& text) {
    std::vector<unsigned int> values;
    std::stringstream input(text);
    std::string item;
    while (std::getline(input, item, ',')) {
        const std::size_t first = item.find_first_not_of(" \t\r\n");
        const std::size_t last = item.find_last_not_of(" \t\r\n");
        if (first == std::string::npos) pebble_gl_fail("empty index-data element");
        const std::string token = item.substr(first, last - first + 1);
        std::size_t consumed = 0;
        unsigned long value = 0;
        try { value = std::stoul(token, &consumed); }
        catch (...) { pebble_gl_fail("index data must be comma-separated integers"); }
        if (consumed != token.size() || value > 0xffffffffUL)
            pebble_gl_fail("invalid index in element data");
        values.push_back(static_cast<unsigned int>(value));
    }
    if (values.empty()) pebble_gl_fail("index data cannot be empty");
    return values;
}
static PebbleGLuint pebble_gl_program(long long handle) {
    auto found = pebble_gl_programs.find(handle);
    if (found == pebble_gl_programs.end()) pebble_gl_fail("invalid program handle");
    return found->second;
}
static PebbleGLuint pebble_gl_array(long long handle) {
    auto found = pebble_gl_arrays.find(handle);
    if (found == pebble_gl_arrays.end()) pebble_gl_fail("invalid vertex-array handle");
    return found->second;
}
static PebbleGLuint pebble_gl_buffer(long long handle) {
    auto found = pebble_gl_buffers.find(handle);
    if (found == pebble_gl_buffers.end()) pebble_gl_fail("invalid buffer handle");
    return found->second;
}
static PebbleGLint pebble_gl_uniform(long long handle) {
    auto found = pebble_gl_uniforms.find(handle);
    if (found == pebble_gl_uniforms.end()) pebble_gl_fail("invalid uniform handle");
    if (found->second.first != pebble_gl_active_program)
        pebble_gl_fail("use the uniform's program before updating it");
    return found->second.second;
}
static PebbleGLuint pebble_gl_texture(long long handle) {
    auto found = pebble_gl_textures.find(handle);
    if (found == pebble_gl_textures.end()) pebble_gl_fail("invalid texture handle");
    return found->second;
}
static inline bool pebble_gl_initialize() {
    if (glfwGetCurrentContext() == nullptr) return false;
#define PEBBLE_GL_LOAD(name) p##name = pebble_gl_proc<PebbleGL_##name>(#name); if (p##name == nullptr) { std::cerr << "Pebble OpenGL runtime error: could not load " #name " from the current context\n"; return false; }
    PEBBLE_GL_LOAD(glCreateShader); PEBBLE_GL_LOAD(glShaderSource);
    PEBBLE_GL_LOAD(glCompileShader); PEBBLE_GL_LOAD(glGetShaderiv);
    PEBBLE_GL_LOAD(glGetShaderInfoLog); PEBBLE_GL_LOAD(glDeleteShader);
    PEBBLE_GL_LOAD(glCreateProgram); PEBBLE_GL_LOAD(glAttachShader);
    PEBBLE_GL_LOAD(glLinkProgram); PEBBLE_GL_LOAD(glGetProgramiv);
    PEBBLE_GL_LOAD(glGetProgramInfoLog); PEBBLE_GL_LOAD(glDeleteProgram);
    PEBBLE_GL_LOAD(glUseProgram); PEBBLE_GL_LOAD(glGetUniformLocation);
    PEBBLE_GL_LOAD(glUniform1i); PEBBLE_GL_LOAD(glUniform1f);
    PEBBLE_GL_LOAD(glUniform2f); PEBBLE_GL_LOAD(glUniform3f);
    PEBBLE_GL_LOAD(glUniform4f); PEBBLE_GL_LOAD(glUniformMatrix4fv);
    PEBBLE_GL_LOAD(glGenVertexArrays); PEBBLE_GL_LOAD(glBindVertexArray);
    PEBBLE_GL_LOAD(glDeleteVertexArrays); PEBBLE_GL_LOAD(glGenBuffers);
    PEBBLE_GL_LOAD(glBindBuffer); PEBBLE_GL_LOAD(glBufferData);
    PEBBLE_GL_LOAD(glDeleteBuffers); PEBBLE_GL_LOAD(glEnableVertexAttribArray);
    PEBBLE_GL_LOAD(glVertexAttribPointer); PEBBLE_GL_LOAD(glDrawElements);
    PEBBLE_GL_LOAD(glDrawArrays); PEBBLE_GL_LOAD(glEnable);
    PEBBLE_GL_LOAD(glViewport); PEBBLE_GL_LOAD(glClearColor); PEBBLE_GL_LOAD(glClear);
    PEBBLE_GL_LOAD(glGenTextures); PEBBLE_GL_LOAD(glActiveTexture);
    PEBBLE_GL_LOAD(glBindTexture); PEBBLE_GL_LOAD(glTexImage2D);
    PEBBLE_GL_LOAD(glTexParameteri); PEBBLE_GL_LOAD(glDeleteTextures);
#undef PEBBLE_GL_LOAD
    pebble_gl_thread = std::this_thread::get_id();
    pebble_gl_ready = true;
    return true;
}
static inline void pebble_gl_shutdown() {
    if (!pebble_gl_ready) return;
    pebble_gl_require();
    for (const auto& resource : pebble_gl_programs) pglDeleteProgram(resource.second);
    for (const auto& resource : pebble_gl_arrays) pglDeleteVertexArrays(1, &resource.second);
    for (const auto& resource : pebble_gl_buffers) pglDeleteBuffers(1, &resource.second);
    for (const auto& resource : pebble_gl_textures) pglDeleteTextures(1, &resource.second);
    pebble_gl_programs.clear(); pebble_gl_arrays.clear(); pebble_gl_buffers.clear(); pebble_gl_uniforms.clear(); pebble_gl_textures.clear();
    pebble_gl_bound_array = 0; pebble_gl_bound_buffer = 0; pebble_gl_bound_element = 0;
    pebble_gl_active_program = 0; pebble_gl_bound_texture = 0; pebble_gl_ready = false;
}
static inline void pebble_gl_viewport(long long width, long long height) {
    pebble_gl_require(); if (width <= 0 || height <= 0) pebble_gl_fail("viewport dimensions must be positive");
    pglViewport(0, 0, static_cast<int>(width), static_cast<int>(height));
}
static inline void pebble_gl_clear_color(double r, double g, double b, double a) {
    pebble_gl_require(); pglClearColor(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a));
}
static inline void pebble_gl_clear() { pebble_gl_require(); pglClear(PEBBLE_GL_COLOR_BUFFER_BIT | PEBBLE_GL_DEPTH_BUFFER_BIT); }
static inline void pebble_gl_enable_depth_test() { pebble_gl_require(); pglEnable(PEBBLE_GL_DEPTH_TEST); }
static inline PebbleGLuint pebble_gl_compile(PebbleGLenum kind, const std::string& source) {
    PebbleGLuint shader = pglCreateShader(kind); const char* text = source.c_str();
    pglShaderSource(shader, 1, &text, nullptr); pglCompileShader(shader);
    PebbleGLint ok = 0; pglGetShaderiv(shader, PEBBLE_GL_COMPILE_STATUS, &ok);
    if (ok == 0) { PebbleGLint length = 0; pglGetShaderiv(shader, PEBBLE_GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(length > 1 ? length : 1), '\0');
        pglGetShaderInfoLog(shader, length, nullptr, log.data()); pglDeleteShader(shader);
        pebble_gl_fail("shader compilation failed: " + log); }
    return shader;
}
static inline long long pebble_gl_create_program(const std::string& vertex, const std::string& fragment) {
    pebble_gl_require(); PebbleGLuint vs = pebble_gl_compile(PEBBLE_GL_VERTEX_SHADER, vertex);
    PebbleGLuint fs = 0; PebbleGLuint program = 0;
    try { fs = pebble_gl_compile(PEBBLE_GL_FRAGMENT_SHADER, fragment); }
    catch (...) { pglDeleteShader(vs); throw; }
    program = pglCreateProgram(); pglAttachShader(program, vs); pglAttachShader(program, fs); pglLinkProgram(program);
    pglDeleteShader(vs); pglDeleteShader(fs); PebbleGLint ok = 0; pglGetProgramiv(program, PEBBLE_GL_LINK_STATUS, &ok);
    if (ok == 0) { PebbleGLint length = 0; pglGetProgramiv(program, PEBBLE_GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(length > 1 ? length : 1), '\0');
        pglGetProgramInfoLog(program, length, nullptr, log.data()); pglDeleteProgram(program);
        pebble_gl_fail("program link failed: " + log); }
    const long long handle = pebble_gl_next_handle++; pebble_gl_programs.emplace(handle, program); return handle;
}
static inline void pebble_gl_use_program(long long handle) { pebble_gl_require(); pglUseProgram(pebble_gl_program(handle)); pebble_gl_active_program = handle; }
static inline void pebble_gl_delete_program(long long handle) { pebble_gl_require(); PebbleGLuint id = pebble_gl_program(handle); pglDeleteProgram(id); pebble_gl_programs.erase(handle); for (auto item = pebble_gl_uniforms.begin(); item != pebble_gl_uniforms.end();) { if (item->second.first == handle) item = pebble_gl_uniforms.erase(item); else ++item; } if (pebble_gl_active_program == handle) pebble_gl_active_program = 0; }
static inline long long pebble_gl_uniform_location(long long program, const std::string& name) { pebble_gl_require(); const PebbleGLint location = pglGetUniformLocation(pebble_gl_program(program), name.c_str()); const long long handle = pebble_gl_next_handle++; pebble_gl_uniforms.emplace(handle, std::make_pair(program, location)); return handle; }
static inline void pebble_gl_uniform_int(long long location, long long value) { pebble_gl_require(); const PebbleGLint native = pebble_gl_uniform(location); if (native >= 0) pglUniform1i(native, static_cast<int>(value)); }
static inline void pebble_gl_uniform_float(long long location, double value) { pebble_gl_require(); const PebbleGLint native = pebble_gl_uniform(location); if (native >= 0) pglUniform1f(native, static_cast<float>(value)); }
static inline void pebble_gl_uniform_vec2(long long l, double a, double b) { pebble_gl_require(); const PebbleGLint n = pebble_gl_uniform(l); if (n >= 0) pglUniform2f(n, static_cast<float>(a), static_cast<float>(b)); }
static inline void pebble_gl_uniform_vec3(long long l, double a, double b, double c) { pebble_gl_require(); const PebbleGLint n = pebble_gl_uniform(l); if (n >= 0) pglUniform3f(n, static_cast<float>(a), static_cast<float>(b), static_cast<float>(c)); }
static inline void pebble_gl_uniform_vec4(long long l, double a, double b, double c, double d) { pebble_gl_require(); const PebbleGLint n = pebble_gl_uniform(l); if (n >= 0) pglUniform4f(n, static_cast<float>(a), static_cast<float>(b), static_cast<float>(c), static_cast<float>(d)); }
static inline void pebble_gl_uniform_mat4(long long l, double v0, double v1, double v2, double v3, double v4, double v5, double v6, double v7, double v8, double v9, double v10, double v11, double v12, double v13, double v14, double v15) {
    const float v[16] = {static_cast<float>(v0), static_cast<float>(v1), static_cast<float>(v2), static_cast<float>(v3), static_cast<float>(v4), static_cast<float>(v5), static_cast<float>(v6), static_cast<float>(v7), static_cast<float>(v8), static_cast<float>(v9), static_cast<float>(v10), static_cast<float>(v11), static_cast<float>(v12), static_cast<float>(v13), static_cast<float>(v14), static_cast<float>(v15)};
    pebble_gl_require(); const PebbleGLint native = pebble_gl_uniform(l); if (native >= 0) pglUniformMatrix4fv(native, 1, 0, v);
}
static inline long long pebble_gl_create_vertex_array() { pebble_gl_require(); PebbleGLuint id = 0; pglGenVertexArrays(1, &id); long long h = pebble_gl_next_handle++; pebble_gl_arrays.emplace(h, id); return h; }
static inline void pebble_gl_bind_vertex_array(long long h) { pebble_gl_require(); pglBindVertexArray(pebble_gl_array(h)); pebble_gl_bound_array = h; pebble_gl_bound_element = pebble_gl_array_elements.count(h) ? pebble_gl_array_elements[h] : 0; }
static inline void pebble_gl_delete_vertex_array(long long h) { pebble_gl_require(); PebbleGLuint id = pebble_gl_array(h); pglDeleteVertexArrays(1, &id); pebble_gl_arrays.erase(h); pebble_gl_array_elements.erase(h); if (pebble_gl_bound_array == h) { pebble_gl_bound_array = 0; pebble_gl_bound_element = 0; } }
static inline long long pebble_gl_create_buffer() { pebble_gl_require(); PebbleGLuint id = 0; pglGenBuffers(1, &id); long long h = pebble_gl_next_handle++; pebble_gl_buffers.emplace(h, id); return h; }
static inline void pebble_gl_bind_array_buffer(long long h) { pebble_gl_require(); PebbleGLuint id = pebble_gl_buffer(h); pglBindBuffer(PEBBLE_GL_ARRAY_BUFFER, id); pebble_gl_bound_buffer = h; }
static inline void pebble_gl_bind_element_buffer(long long h) { pebble_gl_require(); if (pebble_gl_bound_array == 0) pebble_gl_fail("bind a vertex array before its element buffer"); pglBindBuffer(PEBBLE_GL_ELEMENT_ARRAY_BUFFER, pebble_gl_buffer(h)); pebble_gl_bound_element = h; pebble_gl_array_elements[pebble_gl_bound_array] = h; }
static inline void pebble_gl_upload_array_buffer(const std::string& data) { pebble_gl_require(); if (pebble_gl_bound_buffer == 0) pebble_gl_fail("bind an array buffer before uploading"); auto v = pebble_gl_floats(data); pglBufferData(PEBBLE_GL_ARRAY_BUFFER, static_cast<PebbleGLsizeiptr>(v.size() * sizeof(float)), v.data(), PEBBLE_GL_STATIC_DRAW); }
static inline void pebble_gl_upload_element_buffer(const std::string& data) { pebble_gl_require(); if (pebble_gl_bound_array == 0 || pebble_gl_bound_element == 0) pebble_gl_fail("bind an element buffer to a vertex array before uploading"); auto v = pebble_gl_indices(data); pglBufferData(PEBBLE_GL_ELEMENT_ARRAY_BUFFER, static_cast<PebbleGLsizeiptr>(v.size() * sizeof(unsigned int)), v.data(), PEBBLE_GL_STATIC_DRAW); }
static inline void pebble_gl_vertex_attribute(long long index, long long size, long long stride, long long offset) { pebble_gl_require(); if (pebble_gl_bound_array == 0 || pebble_gl_bound_buffer == 0) pebble_gl_fail("bind a vertex array and array buffer before configuring an attribute"); if (index < 0 || size < 1 || size > 4 || stride <= 0 || offset < 0) pebble_gl_fail("invalid vertex-attribute layout"); pglEnableVertexAttribArray(static_cast<unsigned int>(index)); pglVertexAttribPointer(static_cast<unsigned int>(index), static_cast<int>(size), PEBBLE_GL_FLOAT, 0, static_cast<int>(stride), reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset))); }
static inline void pebble_gl_draw_elements(long long count) { pebble_gl_require(); if (pebble_gl_active_program == 0 || pebble_gl_bound_array == 0 || pebble_gl_bound_element == 0) pebble_gl_fail("bind a program, vertex array, and element buffer before drawing"); if (count <= 0) pebble_gl_fail("draw count must be positive"); pglDrawElements(PEBBLE_GL_TRIANGLES, static_cast<int>(count), PEBBLE_GL_UNSIGNED_INT, nullptr); }
static inline void pebble_gl_draw_arrays(long long count) { pebble_gl_require(); if (pebble_gl_active_program == 0 || pebble_gl_bound_array == 0) pebble_gl_fail("bind a program and vertex array before drawing"); if (count <= 0) pebble_gl_fail("draw count must be positive"); pglDrawArrays(PEBBLE_GL_TRIANGLES, 0, static_cast<int>(count)); }
static inline void pebble_gl_delete_buffer(long long h) { pebble_gl_require(); PebbleGLuint id = pebble_gl_buffer(h); pglDeleteBuffers(1, &id); pebble_gl_buffers.erase(h); if (pebble_gl_bound_buffer == h) pebble_gl_bound_buffer = 0; if (pebble_gl_bound_element == h) pebble_gl_bound_element = 0; }
static inline long long pebble_gl_create_texture() { pebble_gl_require(); PebbleGLuint id = 0; pglGenTextures(1, &id); const long long handle = pebble_gl_next_handle++; pebble_gl_textures.emplace(handle, id); return handle; }
static inline void pebble_gl_bind_texture(long long handle, long long unit) { pebble_gl_require(); if (unit < 0 || unit >= 16) pebble_gl_fail("texture unit must be between 0 and 15"); pglActiveTexture(PEBBLE_GL_TEXTURE0 + static_cast<unsigned int>(unit)); pglBindTexture(PEBBLE_GL_TEXTURE_2D, pebble_gl_texture(handle)); pebble_gl_bound_texture = handle; }
static inline void pebble_gl_upload_texture_rgba(long long width, long long height, const std::string& data) {
    pebble_gl_require(); if (pebble_gl_bound_texture == 0) pebble_gl_fail("bind a texture before uploading pixels");
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384) pebble_gl_fail("texture dimensions must be between 1 and 16384");
    std::vector<unsigned char> pixels; std::stringstream input(data); std::string item;
    while (std::getline(input, item, ',')) { const std::size_t first = item.find_first_not_of(" \t\r\n"); const std::size_t last = item.find_last_not_of(" \t\r\n"); if (first == std::string::npos) pebble_gl_fail("empty texture pixel value"); const std::string token = item.substr(first, last - first + 1); std::size_t consumed = 0; unsigned long value = 0; try { value = std::stoul(token, &consumed); } catch (...) { pebble_gl_fail("RGBA texture data must contain byte values from 0 to 255"); } if (consumed != token.size() || value > 255UL) pebble_gl_fail("RGBA texture data must contain byte values from 0 to 255"); pixels.push_back(static_cast<unsigned char>(value)); }
    const std::size_t expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
    if (pixels.size() != expected) pebble_gl_fail("RGBA texture data length must equal width * height * 4");
    pglTexImage2D(PEBBLE_GL_TEXTURE_2D, 0, static_cast<int>(PEBBLE_GL_RGBA), static_cast<int>(width), static_cast<int>(height), 0, PEBBLE_GL_RGBA, PEBBLE_GL_UNSIGNED_BYTE, pixels.data());
}
static inline void pebble_gl_set_texture_sampling(const std::string& filter, const std::string& wrap) {
    pebble_gl_require(); if (pebble_gl_bound_texture == 0) pebble_gl_fail("bind a texture before setting its sampling");
    const PebbleGLint filtering = filter == "nearest" ? PEBBLE_GL_NEAREST : (filter == "linear" ? PEBBLE_GL_LINEAR : 0);
    const PebbleGLint wrapping = wrap == "repeat" ? PEBBLE_GL_REPEAT : (wrap == "clamp" ? PEBBLE_GL_CLAMP_TO_EDGE : 0);
    if (filtering == 0) pebble_gl_fail("texture filter must be 'nearest' or 'linear'");
    if (wrapping == 0) pebble_gl_fail("texture wrap must be 'repeat' or 'clamp'");
    pglTexParameteri(PEBBLE_GL_TEXTURE_2D, PEBBLE_GL_TEXTURE_MIN_FILTER, filtering); pglTexParameteri(PEBBLE_GL_TEXTURE_2D, PEBBLE_GL_TEXTURE_MAG_FILTER, filtering);
    pglTexParameteri(PEBBLE_GL_TEXTURE_2D, PEBBLE_GL_TEXTURE_WRAP_S, wrapping); pglTexParameteri(PEBBLE_GL_TEXTURE_2D, PEBBLE_GL_TEXTURE_WRAP_T, wrapping);
}
static inline void pebble_gl_delete_texture(long long handle) { pebble_gl_require(); PebbleGLuint id = pebble_gl_texture(handle); pglDeleteTextures(1, &id); pebble_gl_textures.erase(handle); if (pebble_gl_bound_texture == handle) pebble_gl_bound_texture = 0; }
)PBS";
        }

        for (const PackageFunction* function : packageFunctions_) {
            if (!function->runtimeSymbol.starts_with("pebble_gl_")) {
                runtime += runtimeImplementation(*function);
            }
        }

        return runtime;
    }

    [[nodiscard]] std::string runtimeImplementation(
        const PackageFunction& function
    ) const {
        if (function.runtimeSymbol == "pebble_math_randint") {
            return
                "static long long pebble_math_randint("
                "long long minimum, long long maximum) {\n"
                "    if (minimum > maximum) {\n"
                "        std::cerr << \"Pebble runtime error: math.randint "
                "requires min <= max (min=\" << minimum << \", max=\" "
                "<< maximum << \")\\n\";\n"
                "        std::exit(1);\n"
                "    }\n"
                "\n"
                "    const std::uint64_t range = "
                "static_cast<std::uint64_t>(maximum) - "
                "static_cast<std::uint64_t>(minimum);\n"
                "\n"
                "    if (range == UINT64_MAX) {\n"
                "        return static_cast<long long>("
                "static_cast<std::uint64_t>(minimum) + pebble_rng_next());\n"
                "    }\n"
                "\n"
                "    const std::uint64_t limit = range + UINT64_C(1);\n"
                "    const std::uint64_t threshold = "
                "(std::uint64_t{0} - limit) % limit;\n"
                "    std::uint64_t candidate = pebble_rng_next();\n"
                "\n"
                "    while (candidate < threshold) {\n"
                "        candidate = pebble_rng_next();\n"
                "    }\n"
                "\n"
                "    return static_cast<long long>("
                "static_cast<std::uint64_t>(minimum) + (candidate % limit));\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_abs") {
            return
                "static long long pebble_math_abs(long long value) {\n"
                "    if (value == INT64_MIN) {\n"
                "        std::cerr << \"Pebble runtime error: math.abs is "
                "undefined for -9223372036854775808\\n\";\n"
                "        std::exit(1);\n"
                "    }\n"
                "\n"
                "    return value < 0 ? -value : value;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_min") {
            return
                "static long long pebble_math_min("
                "long long left, long long right) {\n"
                "    return left < right ? left : right;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_max") {
            return
                "static long long pebble_math_max("
                "long long left, long long right) {\n"
                "    return left > right ? left : right;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_randomseed") {
            return
                "static void pebble_math_randomseed(long long seed) {\n"
                "    pebble_rng_state = "
                "static_cast<std::uint64_t>(seed) + "
                "UINT64_C(0x9E3779B97F4A7C15);\n"
                "    pebble_rng_ready = true;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_random") {
            return
                "static double pebble_math_random() {\n"
                "    return static_cast<double>(pebble_rng_next() >> 11) *\n"
                "        (1.0 / 9007199254740992.0);\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_sqrt") {
            return
                "static double pebble_math_sqrt(double value) {\n"
                "    if (value < 0.0) {\n"
                "        std::cerr << \"Pebble runtime error: math.sqrt is "
                "undefined for negative values\\n\";\n"
                "        std::exit(1);\n"
                "    }\n"
                "\n"
                "    return std::sqrt(value);\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_floor") {
            return
                "static double pebble_math_floor(double value) {\n"
                "    return std::floor(value);\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_ceil") {
            return
                "static double pebble_math_ceil(double value) {\n"
                "    return std::ceil(value);\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_round") {
            return
                "static double pebble_math_round(double value) {\n"
                "    return std::round(value);\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_pi") {
            return
                "static double pebble_math_pi() {\n"
                "    return 3.14159265358979323846;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_window_create") {
            return
                "static void pebble_window_create(const std::string& title, long long width, long long height) {\n"
                "    if (!glfwInit()) { std::cerr << \"Pebble runtime error: glfwInit failed\\n\"; std::exit(1); }\n"
                "    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);\n"
                "    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);\n"
                "    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);\n"
                "#ifdef __APPLE__\n"
                "    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);\n"
                "#endif\n"
                "    GLFWwindow* window = glfwCreateWindow((int)width, (int)height, title.c_str(), nullptr, nullptr);\n"
                "    if (!window) { std::cerr << \"Pebble runtime error: glfwCreateWindow failed\\n\"; glfwTerminate(); std::exit(1); }\n"
                "    glfwMakeContextCurrent(window);\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_window_should_close") {
            return
                "static bool pebble_window_should_close() {\n"
                "    GLFWwindow* w = glfwGetCurrentContext(); return w && glfwWindowShouldClose(w);\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_window_poll_events") {
            return
                "static void pebble_window_poll_events() {\n"
                "    glfwPollEvents();\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_window_swap_buffers") {
            return
                "static void pebble_window_swap_buffers() {\n"
                "    GLFWwindow* w = glfwGetCurrentContext(); if (w) glfwSwapBuffers(w);\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_window_close") {
            return
                "static void pebble_window_close() {\n"
                "    GLFWwindow* w = glfwGetCurrentContext(); if (w) glfwDestroyWindow(w);\n"
                "    glfwTerminate();\n"
                "}\n"
                "\n";
        }
        if (function.runtimeSymbol == "pebble_input_is_key_down") {
            return
                "static bool pebble_input_is_key_down(const std::string& key) {\n"
                "    GLFWwindow* w = glfwGetCurrentContext();\n"
                "    if (!w) return false;\n"
                "    int glfwKey = GLFW_KEY_UNKNOWN;\n"
                "    if (key == \"W\") glfwKey = GLFW_KEY_W;\n"
                "    else if (key == \"A\") glfwKey = GLFW_KEY_A;\n"
                "    else if (key == \"S\") glfwKey = GLFW_KEY_S;\n"
                "    else if (key == \"D\") glfwKey = GLFW_KEY_D;\n"
                "    else if (key == \"SPACE\") glfwKey = GLFW_KEY_SPACE;\n"
                "    else if (key == \"ESCAPE\") glfwKey = GLFW_KEY_ESCAPE;\n"
                "    if (glfwKey == GLFW_KEY_UNKNOWN) return false;\n"
                "    return glfwGetKey(w, glfwKey) == GLFW_PRESS;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_input_is_mouse_button_down") {
            return
                "static bool pebble_input_is_mouse_button_down(long long button) {\n"
                "    GLFWwindow* w = glfwGetCurrentContext();\n"
                "    if (!w) return false;\n"
                "    int glfwButton = (button == 0) ? GLFW_MOUSE_BUTTON_LEFT : (button == 1 ? GLFW_MOUSE_BUTTON_RIGHT : GLFW_MOUSE_BUTTON_MIDDLE);\n"
                "    return glfwGetMouseButton(w, glfwButton) == GLFW_PRESS;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_input_mouse_x") {
            return
                "static double pebble_input_mouse_x() {\n"
                "    GLFWwindow* w = glfwGetCurrentContext();\n"
                "    if (!w) return 0.0;\n"
                "    double x, y; glfwGetCursorPos(w, &x, &y); return x;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_input_mouse_y") {
            return
                "static double pebble_input_mouse_y() {\n"
                "    GLFWwindow* w = glfwGetCurrentContext();\n"
                "    if (!w) return 0.0;\n"
                "    double x, y; glfwGetCursorPos(w, &x, &y); return y;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_radians") {
            return
                "static double pebble_math_radians(double value) {\n"
                "    return value * 3.14159265358979323846 / 180.0;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_sin") {
            return
                "static double pebble_math_sin(double value) {\n"
                "    return std::sin(value);\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_cos") {
            return
                "static double pebble_math_cos(double value) {\n"
                "    return std::cos(value);\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_math_tan") {
            return
                "static double pebble_math_tan(double value) {\n"
                "    return std::tan(value);\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_log_print") {
            return
                "static void pebble_log_print(const std::string& value) {\n"
                "    std::cout << value << '\\n';\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_log_warning") {
            return
                "static void pebble_log_warning(const std::string& value) {\n"
                "    std::cerr << \"Pebble warning: \" << value << '\\n';\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_log_error") {
            return
                "static void pebble_log_error(const std::string& value) {\n"
                "    std::cerr << \"Pebble error: \" << value << '\\n';\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_time_now") {
            return
                "static double pebble_time_now() {\n"
                "    const auto elapsed =\n"
                "        std::chrono::steady_clock::now() - pebble_time_origin;\n"
                "    return std::chrono::duration<double>(elapsed).count();\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_time_ticks") {
            return
                "static long long pebble_time_ticks() {\n"
                "    const auto elapsed =\n"
                "        std::chrono::steady_clock::now() - pebble_time_origin;\n"
                "    return static_cast<long long>(\n"
                "        std::chrono::duration_cast<\n"
                "            std::chrono::nanoseconds>(elapsed).count());\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_time_delta") {
            return
                "static double pebble_time_delta() {\n"
                "    static std::chrono::steady_clock::time_point last =\n"
                "        std::chrono::steady_clock::now();\n"
                "    static bool seen = false;\n"
                "    const auto now = std::chrono::steady_clock::now();\n"
                "    const double seconds = seen\n"
                "        ? std::chrono::duration<double>(now - last).count()\n"
                "        : 0.0;\n"
                "    last = now;\n"
                "    seen = true;\n"
                "    return seconds;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_time_sleep") {
            return
                "static void pebble_time_sleep(double seconds) {\n"
                "    if (seconds < 0.0) {\n"
                "        std::cerr << \"Pebble runtime error: time.sleep needs a "
                "non negative duration (duration=\" << seconds << \")\\n\";\n"
                "        std::exit(1);\n"
                "    }\n"
                "\n"
                "    std::this_thread::sleep_for(\n"
                "        std::chrono::duration<double>(seconds)\n"
                "    );\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_time_unix") {
            return
                "static long long pebble_time_unix() {\n"
                "    const auto elapsed =\n"
                "        std::chrono::system_clock::now().time_since_epoch();\n"
                "    return static_cast<long long>(\n"
                "        std::chrono::duration_cast<\n"
                "            std::chrono::seconds>(elapsed).count());\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_filesystem_read") {
            return
                "static std::string pebble_filesystem_read(\n"
                "    const std::string& path\n"
                ") {\n"
                "    if (path.empty()) {\n"
                "        pebble_filesystem_error(\"filesystem.read needs a "
                "path\");\n"
                "    }\n"
                "\n"
                "    std::error_code code;\n"
                "\n"
                "    if (std::filesystem::is_directory(path, code)) {\n"
                "        pebble_filesystem_error(\n"
                "            \"filesystem.read cannot read '\" + path + \"'\"\n"
                "        );\n"
                "    }\n"
                "\n"
                "    std::ifstream file(path, std::ios::binary);\n"
                "\n"
                "    if (!file && !pebble_filesystem_open_resource(path, file)) {\n"
                "        pebble_filesystem_error(\n"
                "            \"filesystem.read cannot read '\" + path + \"'\"\n"
                "        );\n"
                "    }\n"
                "\n"
                "    std::string contents(\n"
                "        (std::istreambuf_iterator<char>(file)),\n"
                "        std::istreambuf_iterator<char>()\n"
                "    );\n"
                "\n"
                "    if (file.bad()) {\n"
                "        pebble_filesystem_error(\n"
                "            \"filesystem.read cannot read '\" + path + \"'\"\n"
                "        );\n"
                "    }\n"
                "\n"
                "    return contents;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_filesystem_write") {
            return
                "static void pebble_filesystem_write(\n"
                "    const std::string& path,\n"
                "    const std::string& contents\n"
                ") {\n"
                "    if (path.empty()) {\n"
                "        pebble_filesystem_error(\"filesystem.write needs a "
                "path\");\n"
                "    }\n"
                "\n"
                "    std::ofstream file(\n"
                "        path,\n"
                "        std::ios::binary | std::ios::trunc\n"
                "    );\n"
                "\n"
                "    if (!file) {\n"
                "        pebble_filesystem_error(\n"
                "            \"filesystem.write cannot write '\" + path + \"'\"\n"
                "        );\n"
                "    }\n"
                "\n"
                "    file.write(\n"
                "        contents.data(),\n"
                "        static_cast<std::streamsize>(contents.size())\n"
                "    );\n"
                "\n"
                "    if (!file) {\n"
                "        pebble_filesystem_error(\n"
                "            \"filesystem.write cannot write '\" + path + \"'\"\n"
                "        );\n"
                "    }\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_filesystem_exists") {
            return
                "static bool pebble_filesystem_exists(const std::string& path) {\n"
                "    if (path.empty()) {\n"
                "        pebble_filesystem_error(\"filesystem.exists needs a "
                "path\");\n"
                "    }\n"
                "\n"
                "    std::error_code code;\n"
                "    return std::filesystem::exists(path, code);\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_filesystem_list") {
            return
                "static std::string pebble_filesystem_list(\n"
                "    const std::string& path\n"
                ") {\n"
                "    if (path.empty()) {\n"
                "        pebble_filesystem_error(\"filesystem.list needs a "
                "path\");\n"
                "    }\n"
                "\n"
                "    std::error_code code;\n"
                "\n"
                "    if (!std::filesystem::is_directory(path, code)) {\n"
                "        pebble_filesystem_error(\n"
                "            \"filesystem.list cannot list '\" + path + \"'\"\n"
                "        );\n"
                "    }\n"
                "\n"
                "    std::vector<std::string> names;\n"
                "    std::filesystem::directory_iterator entry(path, code);\n"
                "    const std::filesystem::directory_iterator end;\n"
                "\n"
                "    for (; entry != end; entry.increment(code)) {\n"
                "        names.push_back(entry->path().filename().string());\n"
                "    }\n"
                "\n"
                "    if (code) {\n"
                "        pebble_filesystem_error(\n"
                "            \"filesystem.list cannot list '\" + path + \"'\"\n"
                "        );\n"
                "    }\n"
                "\n"
                "    std::sort(names.begin(), names.end());\n"
                "\n"
                "    std::string listing;\n"
                "\n"
                "    for (const std::string& name : names) {\n"
                "        listing += name;\n"
                "        listing += '\\n';\n"
                "    }\n"
                "\n"
                "    return listing;\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_filesystem_mkdir") {
            return
                "static void pebble_filesystem_mkdir(const std::string& path) {\n"
                "    if (path.empty()) {\n"
                "        pebble_filesystem_error(\"filesystem.mkdir needs a "
                "path\");\n"
                "    }\n"
                "\n"
                "    std::error_code code;\n"
                "\n"
                "    if (std::filesystem::exists(path, code)) {\n"
                "        pebble_filesystem_error(\n"
                "            \"filesystem.mkdir cannot create '\" + path + \"'\"\n"
                "        );\n"
                "    }\n"
                "\n"
                "    std::filesystem::create_directories(path, code);\n"
                "\n"
                "    if (code) {\n"
                "        pebble_filesystem_error(\n"
                "            \"filesystem.mkdir cannot create '\" + path + \"'\"\n"
                "        );\n"
                "    }\n"
                "}\n"
                "\n";
        }

        if (function.runtimeSymbol == "pebble_string_length") {
            return R"PBS(static long long pebble_string_length(const std::string& text) {
    return static_cast<long long>(text.size());
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_substring") {
            return R"PBS(static std::string pebble_string_substring(
    const std::string& text, long long start, long long count) {
    if (start < 0 || start > static_cast<long long>(text.size())) {
        std::cerr << "Pebble runtime error: string.substring is out of range (start="
                  << start << ", length=" << static_cast<long long>(text.size())
                  << ")\n";
        std::exit(1);
    }

    if (count < 0) {
        std::cerr << "Pebble runtime error: string.substring needs a non negative count (count="
                  << count << ")\n";
        std::exit(1);
    }

    return text.substr(
        static_cast<std::size_t>(start), static_cast<std::size_t>(count)
    );
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_left") {
            return R"PBS(static std::string pebble_string_left(const std::string& text, long long count) {
    if (count < 0) {
        std::cerr << "Pebble runtime error: string.left needs a non negative count (count="
                  << count << ")\n";
        std::exit(1);
    }

    return text.substr(0, static_cast<std::size_t>(count));
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_right") {
            return R"PBS(static std::string pebble_string_right(const std::string& text, long long count) {
    if (count < 0) {
        std::cerr << "Pebble runtime error: string.right needs a non negative count (count="
                  << count << ")\n";
        std::exit(1);
    }

    const std::size_t size = text.size();

    if (static_cast<std::size_t>(count) >= size) {
        return text;
    }

    return text.substr(size - static_cast<std::size_t>(count));
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_upper") {
            return R"PBS(static std::string pebble_string_upper(const std::string& text) {
    std::string result = text;

    for (char& character : result) {
        character = static_cast<char>(
            std::toupper(static_cast<unsigned char>(character))
        );
    }

    return result;
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_lower") {
            return R"PBS(static std::string pebble_string_lower(const std::string& text) {
    std::string result = text;

    for (char& character : result) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))
        );
    }

    return result;
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_trim") {
            return R"PBS(static bool pebble_string_is_space(char character) {
    return character == ' ' || character == '\t' || character == '\n' ||
           character == '\r' || character == '\f' || character == '\v';
}

static std::string pebble_string_trim(const std::string& text) {
    std::size_t start = 0;
    std::size_t end = text.size();

    while (start < end && pebble_string_is_space(text[start])) {
        ++start;
    }

    while (end > start && pebble_string_is_space(text[end - 1])) {
        --end;
    }

    return text.substr(start, end - start);
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_contains") {
            return R"PBS(static bool pebble_string_contains(
    const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_startswith") {
            return R"PBS(static bool pebble_string_startswith(
    const std::string& text, const std::string& prefix) {
    return text.compare(0, prefix.size(), prefix) == 0;
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_endswith") {
            return R"PBS(static bool pebble_string_endswith(
    const std::string& text, const std::string& suffix) {
    if (suffix.size() > text.size()) {
        return false;
    }

    return text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_indexof") {
            return R"PBS(static long long pebble_string_indexof(
    const std::string& text, const std::string& needle) {
    const std::size_t position = text.find(needle);

    if (position == std::string::npos) {
        return -1;
    }

    return static_cast<long long>(position);
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_replace") {
            return R"PBS(static std::string pebble_string_replace(
    const std::string& text, const std::string& needle, const std::string& replacement) {
    if (needle.empty()) {
        return text;
    }

    std::string result;
    std::size_t start = 0;
    std::size_t position = text.find(needle, start);

    while (position != std::string::npos) {
        result.append(text, start, position - start);
        result.append(replacement);
        start = position + needle.size();
        position = text.find(needle, start);
    }

    result.append(text, start, std::string::npos);
    return result;
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_repeat") {
            return R"PBS(static std::string pebble_string_repeat(const std::string& text, long long count) {
    if (count < 0) {
        std::cerr << "Pebble runtime error: string.repeat needs a non negative count (count="
                  << count << ")\n";
        std::exit(1);
    }

    std::string result;
    result.reserve(text.size() * static_cast<std::size_t>(count));

    for (long long index = 0; index < count; ++index) {
        result.append(text);
    }

    return result;
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_toint") {
            return R"PBS(static long long pebble_string_toint(const std::string& text) {
    const char* begin = text.c_str();
    char* end = nullptr;

    errno = 0;

    const long long value = std::strtoll(begin, &end, 10);

    if (end == begin || *end != '\0' || errno == ERANGE) {
        std::cerr << "Pebble runtime error: string.toint cannot parse '" << text
                  << "'\n";
        std::exit(1);
    }

    return value;
}
)PBS";
        }

        if (function.runtimeSymbol == "pebble_string_tofloat") {
            return R"PBS(static double pebble_string_tofloat(const std::string& text) {
    const char* begin = text.c_str();
    char* end = nullptr;

    errno = 0;

    const double value = std::strtod(begin, &end);

    if (end == begin || *end != '\0' || errno == ERANGE) {
        std::cerr << "Pebble runtime error: string.tofloat cannot parse '" << text
                  << "'\n";
        std::exit(1);
    }

    return value;
}
)PBS";
        }

        throw std::runtime_error(
            "No runtime implementation for package function '" +
                function.runtimeSymbol + "'."
        );
    }

    void indentation() {
        for (std::size_t i = 0; i < indent_; ++i) {
            output_ += "    ";
        }
    }

    void emitFunctionPrototype(const Node& function) {
        const bool mainFunction = function.text == "main";

        if (mainFunction) {
            output_ += "int main();\n";
            return;
        }

        output_ += cppType(functionReturnType(function));
        output_ += " ";
        output_ += functionName(function.text);
        output_ += "(";

        bool first = true;

        for (const auto& child : function.children) {
            if (child->kind != NodeKind::Parameter) {
                continue;
            }

            if (!first) {
                output_ += ", ";
            }

            first = false;
            output_ += cppType(parameterType(*child));
            output_ += " ";
            output_ += variableName(child->text);
        }

        output_ += ");\n";
    }

    void emitFunction(const Node& function) {
        const bool mainFunction = function.text == "main";
        inMain_ = mainFunction;
        currentPackage_ = packageOf(function.text);

        if (mainFunction) {
            output_ += "int main() ";
        } else {
            output_ += cppType(functionReturnType(function));
            output_ += " ";
            output_ += functionName(function.text);
            output_ += "(";

            bool first = true;

            for (const auto& child : function.children) {
                if (child->kind != NodeKind::Parameter) {
                    continue;
                }

                if (!first) {
                    output_ += ", ";
                }

                first = false;
                output_ += "[[maybe_unused]] ";
                output_ += cppType(parameterType(*child));
                output_ += " ";
                output_ += variableName(child->text);
            }

            output_ += ") ";
        }

        const Node* body = findChild(function, NodeKind::Block);

        if (body == nullptr) {
            throw std::runtime_error(
                "Function '" + function.text + "' has no body."
            );
        }

        emitBlock(*body);
        inMain_ = false;
        currentPackage_.clear();
    }

    void emitBlock(const Node& block) {
        output_ += "{\n";
        ++indent_;

        for (const auto& statement : block.children) {
            emitStatement(*statement);
        }

        --indent_;
        indentation();
        output_ += "}";
    }

    void emitStatement(const Node& node) {
        indentation();

        switch (node.kind) {
        case NodeKind::Block:
            emitBlock(node);
            output_ += "\n";
            return;

        case NodeKind::LetDeclaration:
        case NodeKind::VarDeclaration: {
            const Node* value = initializer(node);

            if (value == nullptr) {
                throw std::runtime_error(
                    "Variable '" + node.text + "' has no initializer."
                );
            }

            const std::string annotation = declaredType(node);

            output_ += "[[maybe_unused]] ";

            if (node.kind == NodeKind::LetDeclaration) {
                output_ += "const ";
            }

            if (annotation.empty()) {
                output_ += "auto ";
            } else {
                output_ += cppType(annotation);
                output_ += " ";
            }

            output_ += variableName(node.text);
            output_ += " = ";
            output_ += emitExpression(*value);
            output_ += ";\n";
            return;
        }

        case NodeKind::Assignment:
            output_ += variableName(node.text);
            output_ += " = ";

            if (node.children.empty()) {
                throw std::runtime_error("Assignment has no value.");
            }

            output_ += emitExpression(*node.children.front());
            output_ += ";\n";
            return;

        case NodeKind::ExpressionStatement:
            if (node.children.empty()) {
                throw std::runtime_error(
                    "Expression statement has no expression."
                );
            }

            emitStatementExpression(*node.children.front());
            output_ += ";\n";
            return;

        case NodeKind::IfStatement: {
            if (node.children.size() < 2) {
                throw std::runtime_error("Malformed if statement.");
            }

            output_ += "if (";
            output_ += emitExpression(*node.children[0]);
            output_ += ") ";
            emitBlock(*node.children[1]);

            if (node.children.size() >= 3) {
                output_ += " else ";
                const Node& elseBranch = *node.children[2];

                if (elseBranch.kind == NodeKind::Block) {
                    emitBlock(elseBranch);
                } else if (elseBranch.kind == NodeKind::IfStatement) {
                    emitStatement(elseBranch);
                    return;
                } else {
                    throw std::runtime_error(
                        "Unsupported else branch node."
                    );
                }
            }

            output_ += "\n";
            return;
        }

        case NodeKind::WhileStatement:
            if (node.children.size() != 2) {
                throw std::runtime_error("Malformed while statement.");
            }

            output_ += "while (";
            output_ += emitExpression(*node.children[0]);
            output_ += ") ";
            emitBlock(*node.children[1]);
            output_ += "\n";
            return;

        case NodeKind::ReturnStatement:
            output_ += "return";

            if (!node.children.empty()) {
                output_ += " ";
                output_ += emitExpression(*node.children.front());
            } else if (inMain_) {
                output_ += " 0";
            }

            output_ += ";\n";
            return;

        default:
            throw std::runtime_error(
                std::string("Unsupported statement node: ") + std::string(nodeKindName(node.kind)) + "."
            );
        }
    }

    void emitStatementExpression(const Node& node) {
        if (node.kind == NodeKind::Assignment) {
            output_ += variableName(node.text);
            output_ += " = ";

            if (node.children.empty()) {
                throw std::runtime_error("Assignment has no value.");
            }

            output_ += emitExpression(*node.children.front());
            return;
        }

        output_ += emitExpression(node);
    }

    std::string emitExpression(const Node& node) {
        switch (node.kind) {
            case NodeKind::LiteralExpression:
            if (node.text == "null") {
                return "static_cast<const char *>(nullptr)";
            }
            if (node.text.size() >= 2 &&
                node.text.front() == '"' &&
                node.text.back() == '"') {
                return "std::string(" + node.text + ")";
            }

            return node.text;

        case NodeKind::NameExpression:
            return variableName(node.text);

        case NodeKind::UnaryExpression:
            if (node.children.size() != 1) {
                throw std::runtime_error(
                    "Malformed unary expression."
                );
            }

            if (isMinimumIntegerLiteral(node)) {
                return "(-9223372036854775807LL - 1)";
            }

            return "(" + node.text +
                emitExpression(*node.children.front()) + ")";

        case NodeKind::BinaryExpression:
            if (node.children.size() != 2) {
                throw std::runtime_error(
                    "Malformed binary expression."
                );
            }

            return "(" + emitExpression(*node.children[0]) + " " +
                node.text + " " + emitExpression(*node.children[1]) + ")";

        case NodeKind::CallExpression: {
            if (node.children.empty()) {
                throw std::runtime_error("Function call has no callee.");
            }

            const Node& callee = *node.children.front();

            std::string result;

            if (callee.kind == NodeKind::MemberExpression) {
                result = packageCallTarget(callee);

                if (result.empty()) {
                    throw std::runtime_error(
                        "Call to an unknown package function."
                    );
                }
            } else if (callee.kind == NodeKind::NameExpression) {
                if (callee.text == "print") {
                    result = "pebble_print";
                } else if (!currentPackage_.empty() &&
                           findProgramFunction(
                               currentPackage_ + "." + callee.text
                           ) != nullptr) {
                    result = functionName(currentPackage_ + "." + callee.text);
                } else {
                    result = functionName(callee.text);
                }
            } else {
                throw std::runtime_error(
                    "Only named function calls are supported."
                );
            }

            result += "(";

            for (std::size_t i = 1; i < node.children.size(); ++i) {
                if (i > 1) {
                    result += ", ";
                }

                result += emitExpression(*node.children[i]);
            }

            result += ")";
            return result;
        }

        case NodeKind::MemberExpression:
            throw std::runtime_error(
                "Package functions can only be used in a direct call."
            );

        default:
            throw std::runtime_error(
                std::string("Unsupported expression node: ") + std::string(nodeKindName(node.kind)) + "."
            );
        }
    }
};

}

std::string CodeGenerator::generate(const Node& program) const {
    return Generator{CodegenOptions{}}.generate(program);
}

std::string CodeGenerator::generate(
    const Node& program,
    const CodegenOptions& options
) const {
    return Generator{options}.generate(program);
}

}
