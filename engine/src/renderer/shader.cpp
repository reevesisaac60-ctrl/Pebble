#include "renderer/shader.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Reads a text file into a string. Prints an error to stderr and returns an
// empty string when the file cannot be opened.
std::string Shader::loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::fprintf(stderr, "Shader: cannot open file '%s'\n", path.c_str());
        return "";
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

// Compiles a single shader object. Returns 0 on failure.
GLuint Shader::compileShader(GLenum type, const std::string& source) {
    if (source.empty()) {
        return 0;
    }

    const GLuint shaderId = glCreateShader(type);
    if (shaderId == 0) {
        std::fprintf(stderr, "Shader: glCreateShader failed\n");
        return 0;
    }

    const char* sourcePtr = source.c_str();
    glShaderSource(shaderId, 1, &sourcePtr, nullptr);
    glCompileShader(shaderId);

    GLint compileStatus = 0;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus == GL_FALSE) {
        GLint logLength = 0;
        glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> log(logLength > 0 ? static_cast<size_t>(logLength) : 1);
        glGetShaderInfoLog(shaderId, static_cast<GLsizei>(log.size()), nullptr, log.data());

        const char* shaderType = (type == GL_VERTEX_SHADER)   ? "vertex"
                                 : (type == GL_FRAGMENT_SHADER) ? "fragment"
                                                               : "compute";
        std::fprintf(stderr, "Shader: %s shader compilation failed:\n%s\n",
                     shaderType, log.data());
        glDeleteShader(shaderId);
        return 0;
    }

    return shaderId;
}

Shader::Shader() = default;

Shader::~Shader() {
    if (m_programId != 0) {
        glDeleteProgram(m_programId);
    }
}

bool Shader::load(const std::string& vertexPath, const std::string& fragmentPath) {
    const std::string vertexSource = loadFile(vertexPath);
    const std::string fragmentSource = loadFile(fragmentPath);
    if (vertexSource.empty() || fragmentSource.empty()) {
        std::fprintf(stderr, "Shader: missing source for '%s' or '%s'\n",
                     vertexPath.c_str(), fragmentPath.c_str());
        return false;
    }

    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) glDeleteShader(vertexShader);
        if (fragmentShader != 0) glDeleteShader(fragmentShader);
        return false;
    }

    m_programId = glCreateProgram();
    if (m_programId == 0) {
        std::fprintf(stderr, "Shader: glCreateProgram failed\n");
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    glAttachShader(m_programId, vertexShader);
    glAttachShader(m_programId, fragmentShader);
    glLinkProgram(m_programId);

    GLint linkStatus = 0;
    glGetProgramiv(m_programId, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE) {
        GLint logLength = 0;
        glGetProgramiv(m_programId, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> log(logLength > 0 ? static_cast<size_t>(logLength) : 1);
        glGetProgramInfoLog(m_programId, static_cast<GLsizei>(log.size()), nullptr, log.data());

        std::fprintf(stderr, "Shader: program linking failed:\n%s\n", log.data());

        glDeleteProgram(m_programId);
        m_programId = 0;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    // Temporary shader objects are no longer needed once linked.
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    std::fprintf(stdout, "Shader: linked program from '%s' and '%s'\n",
                 vertexPath.c_str(), fragmentPath.c_str());
    return true;
}

void Shader::use() const {
    glUseProgram(m_programId);
}

void Shader::setUniformMatrix4fv(const std::string& name, const float* value) const {
    const GLint location = glGetUniformLocation(m_programId, name.c_str());
    if (location == -1) {
        return;
    }
    glUniformMatrix4fv(location, 1, GL_FALSE, value);
}

void Shader::setUniform3f(const std::string& name, float x, float y, float z) const {
    const GLint location = glGetUniformLocation(m_programId, name.c_str());
    if (location == -1) {
        return;
    }
    glUniform3f(location, x, y, z);
}

void Shader::setUniform1f(const std::string& name, float value) const {
    const GLint location = glGetUniformLocation(m_programId, name.c_str());
    if (location == -1) {
        return;
    }
    glUniform1f(location, value);
}

void Shader::setUniform1i(const std::string& name, int value) const {
    const GLint location = glGetUniformLocation(m_programId, name.c_str());
    if (location == -1) {
        return;
    }
    glUniform1i(location, value);
}
