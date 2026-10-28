#pragma once

#include <epoxy/gl.h>

#include <string>

#include "math/mat4.hpp"
#include "math/vec3.hpp"
#include "renderer/shader.hpp"

struct RendererCamera;
struct RendererEnvironment;

// Renders a background sky behind ordinary scene geometry. Supports two modes:
//   - procedural: a gradient between skyColor (zenith) and horizonColor,
//                 scaled by skyIntensity, rotated by environmentRotation.
//   - textured:   a cubemap sampled from skyboxTexture.
//
// The sky is drawn with depth writes disabled and depth func GL_LEQUAL so
// that scene geometry correctly occludes it. The camera translation is
// stripped from the view matrix so the sky appears infinitely far away and
// does not appear to move when the camera translates.
class Sky {
public:
    Sky();
    ~Sky();

    Sky(const Sky&) = delete;
    Sky& operator=(const Sky&) = delete;

    bool init(const std::string& assetsDir);
    void shutdown();

    // Load a cubemap from 6 PPM files in a directory. Returns the OpenGL
    // cubemap texture ID, or 0 on failure.
    GLuint loadCubemap(const std::string& directory) const;

    // Render the sky. Returns false when there is nothing to render
    // (e.g. no valid shader).
    bool render(const RendererCamera& camera,
                const RendererEnvironment& environment);

private:
    bool loadProceduralShaders();
    bool loadCubemapShaders();

    std::string m_assetsDir;

    // Procedural sky (gradient).
    Shader m_proceduralShader;
    GLuint m_proceduralVAO = 0;
    GLuint m_proceduralVBO = 0;
    GLuint m_proceduralEBO = 0;

    // Cubemap skybox.
    Shader m_cubemapShader;
    GLuint m_cubemapVAO = 0;
    GLuint m_cubemapVBO = 0;
    GLuint m_cubemapEBO = 0;

    bool m_proceduralReady = false;
    bool m_cubemapReady = false;

    // Skybox cube vertex data: position(3) only.
    static constexpr int kStride = 3 * sizeof(float);
    static constexpr int kPositionOffset = 0;
};
