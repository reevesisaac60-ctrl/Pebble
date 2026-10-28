#pragma once

#include <epoxy/gl.h>

#include <string>

// A minimal OpenGL shader program: vertex + fragment shaders loaded from
// files, compiled, linked, and released with RAII. Compilation and linking
// failures are reported with the shader compiler's info log.
class Shader {
public:
    Shader();
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Load, compile and link the program from the two shader files.
    // Returns false on any failure (a message is already printed to stderr).
    bool load(const std::string& vertexPath, const std::string& fragmentPath);

    void use() const;
    GLuint id() const { return m_programId; }
    bool isValid() const { return m_programId != 0; }

    void setUniformMatrix4fv(const std::string& name, const float* value) const;
    void setUniform3f(const std::string& name, float x, float y, float z) const;
    void setUniform1f(const std::string& name, float value) const;
    void setUniform1i(const std::string& name, int value) const;

private:
    GLuint m_programId = 0;

    static std::string loadFile(const std::string& path);
    static GLuint compileShader(GLenum type, const std::string& source);
};
