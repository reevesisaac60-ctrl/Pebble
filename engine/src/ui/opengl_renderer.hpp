#pragma once

#include "ui/renderer.hpp"

#include <epoxy/gl.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace pebble::ui {

// OpenGL 3.3 renderer for PebbleUI. Construct and destroy it while the engine's
// OpenGL context is current. All input rectangles and text coordinates are in
// logical window units; framebuffer scaling is applied internally.
class OpenGLRenderer final : public Renderer {
public:
    OpenGLRenderer();
    ~OpenGLRenderer() override;
    OpenGLRenderer(const OpenGLRenderer&) = delete;
    OpenGLRenderer& operator=(const OpenGLRenderer&) = delete;

    bool initialize();
    bool initialized() const { return m_program != 0U; }
    void beginFrame(const DisplayMetrics& metrics) override;
    void endFrame() override;
    void pushClip(Rect rect) override;
    void popClip() override;
    void fillRect(Rect rect, Color color, float radius = 0.0F) override;
    void strokeRect(Rect rect, Color color, float thickness = 1.0F, float radius = 0.0F) override;
    void line(float x1, float y1, float x2, float y2, Color color, float thickness = 1.0F) override;
    Size measureText(std::string_view text, float logicalFontSize) const override;
    void text(float x, float y, std::string_view text, Color color, float logicalFontSize) override;
    void texture(std::uint32_t textureId, Rect rect, Color tint = {1, 1, 1, 1}) override;

private:
    struct Vertex { float x, y, u, v; float r, g, b, a; };
    struct Clip { Rect rect; };
    DisplayMetrics m_metrics{};
    std::vector<Vertex> m_vertices;
    std::vector<Clip> m_clips;
    std::uint32_t m_program = 0;
    std::uint32_t m_vao = 0;
    std::uint32_t m_vbo = 0;
    std::uint32_t m_fontTexture = 0;
    std::uint32_t m_pendingTexture = 0;
    int m_viewportLocation = -1;
    int m_textureLocation = -1;
    int m_modeLocation = -1;
    bool m_frameOpen = false;
    struct GLState {
        GLint program = 0;
        GLint vertexArray = 0;
        GLint arrayBuffer = 0;
        GLint activeTexture = GL_TEXTURE0;
        GLint texture0Binding = 0;
        GLint viewport[4]{};
        GLint scissorBox[4]{};
        GLint blendSrcRgb = GL_ONE;
        GLint blendDstRgb = GL_ZERO;
        GLint blendSrcAlpha = GL_ONE;
        GLint blendDstAlpha = GL_ZERO;
        GLint blendEquationRgb = GL_FUNC_ADD;
        GLint blendEquationAlpha = GL_FUNC_ADD;
        GLboolean blendEnabled = GL_FALSE;
        GLboolean cullEnabled = GL_FALSE;
        GLboolean depthEnabled = GL_FALSE;
        GLboolean scissorEnabled = GL_FALSE;
        bool valid = false;
    } m_savedState{};

    void saveGLState();
    void restoreGLState();
    void quad(Rect rect, Rect uv, Color color, std::uint32_t textureId);
    void flush(std::uint32_t textureId);
    Rect currentClip() const;
};

} // namespace pebble::ui
