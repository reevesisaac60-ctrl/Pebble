#include "ui/opengl_renderer.hpp"
#include "ui/font_atlas.hpp"

#include <epoxy/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace pebble::ui {
namespace {
constexpr const char* kVertexShader = R"GLSL(#version 330 core
layout(location=0) in vec2 aPosition;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
uniform vec2 uViewport;
out vec2 vUV;
out vec4 vColor;
void main(){ vec2 p=vec2(aPosition.x/uViewport.x*2.0-1.0,1.0-aPosition.y/uViewport.y*2.0); gl_Position=vec4(p,0.0,1.0); vUV=aUV; vColor=aColor; }
)GLSL";
constexpr const char* kFragmentShader = R"GLSL(#version 330 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTexture;
uniform int uMode;
out vec4 FragColor;
void main(){ if(vUV.x<0.0) FragColor=vColor; else if(uMode==1) FragColor=vec4(vColor.rgb,vColor.a*texture(uTexture,vUV).r); else FragColor=texture(uTexture,vUV)*vColor; }
)GLSL";
GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) { glDeleteShader(shader); return 0; }
    return shader;
}
}

OpenGLRenderer::OpenGLRenderer() = default;

void OpenGLRenderer::saveGLState() {
    glGetIntegerv(GL_CURRENT_PROGRAM, &m_savedState.program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &m_savedState.vertexArray);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &m_savedState.arrayBuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &m_savedState.activeTexture);
    glGetIntegerv(GL_VIEWPORT, m_savedState.viewport);
    glGetIntegerv(GL_SCISSOR_BOX, m_savedState.scissorBox);
    glGetIntegerv(GL_BLEND_SRC_RGB, &m_savedState.blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &m_savedState.blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &m_savedState.blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &m_savedState.blendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &m_savedState.blendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &m_savedState.blendEquationAlpha);
    m_savedState.blendEnabled = glIsEnabled(GL_BLEND);
    m_savedState.cullEnabled = glIsEnabled(GL_CULL_FACE);
    m_savedState.depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    m_savedState.scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_savedState.texture0Binding);
    glActiveTexture(static_cast<GLenum>(m_savedState.activeTexture));
    m_savedState.valid = true;
}

void OpenGLRenderer::restoreGLState() {
    if (!m_savedState.valid) return;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_savedState.texture0Binding));
    glActiveTexture(static_cast<GLenum>(m_savedState.activeTexture));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(m_savedState.arrayBuffer));
    glBindVertexArray(static_cast<GLuint>(m_savedState.vertexArray));
    glUseProgram(static_cast<GLuint>(m_savedState.program));
    glViewport(m_savedState.viewport[0], m_savedState.viewport[1],
               m_savedState.viewport[2], m_savedState.viewport[3]);
    glScissor(m_savedState.scissorBox[0], m_savedState.scissorBox[1],
              m_savedState.scissorBox[2], m_savedState.scissorBox[3]);
    glBlendFuncSeparate(static_cast<GLenum>(m_savedState.blendSrcRgb),
                        static_cast<GLenum>(m_savedState.blendDstRgb),
                        static_cast<GLenum>(m_savedState.blendSrcAlpha),
                        static_cast<GLenum>(m_savedState.blendDstAlpha));
    glBlendEquationSeparate(static_cast<GLenum>(m_savedState.blendEquationRgb),
                            static_cast<GLenum>(m_savedState.blendEquationAlpha));
    (m_savedState.blendEnabled == GL_TRUE) ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
    (m_savedState.cullEnabled == GL_TRUE) ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
    (m_savedState.depthEnabled == GL_TRUE) ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
    (m_savedState.scissorEnabled == GL_TRUE) ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
    m_savedState.valid = false;
}
OpenGLRenderer::~OpenGLRenderer() {
    if (m_fontTexture) glDeleteTextures(1, &m_fontTexture);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_program) glDeleteProgram(m_program);
}

bool OpenGLRenderer::initialize() {
    if (m_program) return true;
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vs || !fs) { if (vs) glDeleteShader(vs); if (fs) glDeleteShader(fs); return false; }
    m_program = glCreateProgram();
    glAttachShader(m_program, vs); glAttachShader(m_program, fs); glLinkProgram(m_program);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint linked = GL_FALSE; glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) { glDeleteProgram(m_program); m_program = 0; return false; }
    saveGLState();
    m_viewportLocation = glGetUniformLocation(m_program, "uViewport");
    m_textureLocation = glGetUniformLocation(m_program, "uTexture");
    m_modeLocation = glGetUniformLocation(m_program, "uMode");
    glGenVertexArrays(1, &m_vao); glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao); glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, r)));
    glBindVertexArray(0);

    GLint previousTexture = 0;
    GLint previousUnpackAlignment = 4;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    std::vector<std::uint8_t> pixels(detail::kFontAtlasPixels.begin(), detail::kFontAtlasPixels.end());
    glGenTextures(1, &m_fontTexture); glBindTexture(GL_TEXTURE_2D, m_fontTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, detail::kFontAtlasWidth, detail::kFontAtlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    restoreGLState();
    return true;
}

void OpenGLRenderer::beginFrame(const DisplayMetrics& metrics) {
    if (!initialized()) return;
    saveGLState();
    m_metrics = metrics; m_vertices.clear(); m_clips.clear(); m_pendingTexture = 0; m_frameOpen = true;
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST);
    glUseProgram(m_program); glUniform2f(m_viewportLocation, static_cast<float>(std::max(1, metrics.windowWidth)), static_cast<float>(std::max(1, metrics.windowHeight)));
    glUniform1i(m_textureLocation, 0); glBindVertexArray(m_vao);
}
void OpenGLRenderer::endFrame() { if (!m_frameOpen) return; flush(m_pendingTexture); m_frameOpen = false; restoreGLState(); }
Rect OpenGLRenderer::currentClip() const { return m_clips.empty() ? Rect{0,0,static_cast<float>(m_metrics.windowWidth),static_cast<float>(m_metrics.windowHeight)} : m_clips.back().rect; }
void OpenGLRenderer::pushClip(Rect rect) { flush(0); if (!m_clips.empty()) { const Rect p=currentClip(); const float x=std::max(rect.x,p.x), y=std::max(rect.y,p.y); rect={x,y,std::max(0.0F,std::min(rect.x+rect.width,p.x+p.width)-x),std::max(0.0F,std::min(rect.y+rect.height,p.y+p.height)-y)}; } m_clips.push_back({rect}); }
void OpenGLRenderer::popClip() { flush(0); if (!m_clips.empty()) m_clips.pop_back(); }
void OpenGLRenderer::quad(Rect r, Rect uv, Color c, std::uint32_t textureId) {
    if (!m_frameOpen || r.width <= 0 || r.height <= 0) return;
    if (!m_vertices.empty() && m_pendingTexture != textureId) flush(m_pendingTexture);
    const float x1=r.x,y1=r.y,x2=r.x+r.width,y2=r.y+r.height;
    const float u1=uv.x,v1=uv.y,u2=uv.x+uv.width,v2=uv.y+uv.height;
    const Vertex a{x1,y1,u1,v1,c.r,c.g,c.b,c.a}, b{x2,y1,u2,v1,c.r,c.g,c.b,c.a}, d{x1,y2,u1,v2,c.r,c.g,c.b,c.a}, e{x2,y2,u2,v2,c.r,c.g,c.b,c.a};
    m_vertices.insert(m_vertices.end(),{a,b,d,b,e,d});
    // Texture id is encoded by an immediate flush on next operation; all current
    // vertices in this implementation are submitted using the requested texture.
    m_pendingTexture = textureId;
}
void OpenGLRenderer::flush(std::uint32_t textureId) {
    if (m_vertices.empty()) return;
    (void)textureId;
    textureId = m_pendingTexture;
    glBindBuffer(GL_ARRAY_BUFFER,m_vbo); glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(m_vertices.size()*sizeof(Vertex)),m_vertices.data(),GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,textureId ? textureId : m_fontTexture);
    const Rect clip=currentClip(); const float sx=m_metrics.scaleX(),sy=m_metrics.scaleY();
    const GLint scissorX = static_cast<GLint>(std::floor(clip.x * sx));
    const GLint scissorY = static_cast<GLint>(std::floor((static_cast<float>(m_metrics.windowHeight) - (clip.y + clip.height)) * sy));
    const GLsizei scissorW = static_cast<GLsizei>(std::max(0.0F, std::ceil(clip.width * sx)));
    const GLsizei scissorH = static_cast<GLsizei>(std::max(0.0F, std::ceil(clip.height * sy)));
    glEnable(GL_SCISSOR_TEST); glScissor(scissorX, scissorY, scissorW, scissorH);
    glUniform1i(m_modeLocation, textureId == m_fontTexture ? 1 : 0);
    glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(m_vertices.size())); m_vertices.clear(); m_pendingTexture = 0;
}
void OpenGLRenderer::fillRect(Rect r, Color c, float radius) {
    if (radius <= 0.0F) { quad(r, {-1, 0, 0, 0}, c, 0); return; }
    if (!m_frameOpen || r.width <= 0.0F || r.height <= 0.0F) return;
    if (!m_vertices.empty() && m_pendingTexture != 0U) flush(m_pendingTexture);
    radius = std::clamp(radius, 0.0F, std::min(r.width, r.height) * 0.5F);
    constexpr int segmentsPerCorner = 8;
    constexpr float pi = 3.14159265358979323846F;
    const std::array<std::pair<float, float>, 4> centers{{
        {r.x + r.width - radius, r.y + radius},
        {r.x + r.width - radius, r.y + r.height - radius},
        {r.x + radius, r.y + r.height - radius},
        {r.x + radius, r.y + radius}
    }};
    const std::array<float, 4> starts{{-pi * 0.5F, 0.0F, pi * 0.5F, pi}};
    const Vertex center{r.x + r.width * 0.5F, r.y + r.height * 0.5F, -1, 0, c.r, c.g, c.b, c.a};
    auto vertexAt = [&](float x, float y) { return Vertex{x, y, -1, 0, c.r, c.g, c.b, c.a}; };
    for (int corner = 0; corner < 4; ++corner) {
        for (int i = 0; i < segmentsPerCorner; ++i) {
            const float a0 = starts[static_cast<std::size_t>(corner)] + (pi * 0.5F) * static_cast<float>(i) / static_cast<float>(segmentsPerCorner);
            const float a1 = starts[static_cast<std::size_t>(corner)] + (pi * 0.5F) * static_cast<float>(i + 1) / static_cast<float>(segmentsPerCorner);
            const auto [cx, cy] = centers[static_cast<std::size_t>(corner)];
            m_vertices.push_back(center);
            m_vertices.push_back(vertexAt(cx + std::cos(a0) * radius, cy + std::sin(a0) * radius));
            m_vertices.push_back(vertexAt(cx + std::cos(a1) * radius, cy + std::sin(a1) * radius));
        }
    }
    m_pendingTexture = 0;
}
void OpenGLRenderer::strokeRect(Rect r, Color c, float t, float radius) {
    if (t <= 0.0F || r.width <= 0.0F || r.height <= 0.0F) return;
    if (radius <= 0.0F) {
        fillRect({r.x, r.y, r.width, t}, c);
        fillRect({r.x, r.y + r.height - t, r.width, t}, c);
        fillRect({r.x, r.y, t, r.height}, c);
        fillRect({r.x + r.width - t, r.y, t, r.height}, c);
        return;
    }
    constexpr int segments = 12;
    constexpr float pi = 3.14159265358979323846F;
    const float rad = std::clamp(radius, 0.0F, std::min(r.width, r.height) * 0.5F);
    const std::array<std::pair<float, float>, 4> centers{{{r.x+r.width-rad,r.y+rad},{r.x+r.width-rad,r.y+r.height-rad},{r.x+rad,r.y+r.height-rad},{r.x+rad,r.y+rad}}};
    const std::array<float, 4> starts{{-pi*0.5F,0.0F,pi*0.5F,pi}};
    for (int corner=0; corner<4; ++corner) {
        for (int i=0; i<segments; ++i) {
            const float a0=starts[static_cast<std::size_t>(corner)]+pi*0.5F*static_cast<float>(i)/static_cast<float>(segments);
            const float a1=starts[static_cast<std::size_t>(corner)]+pi*0.5F*static_cast<float>(i+1)/static_cast<float>(segments);
            const auto [cx,cy]=centers[static_cast<std::size_t>(corner)];
            line(cx+std::cos(a0)*rad,cy+std::sin(a0)*rad,cx+std::cos(a1)*rad,cy+std::sin(a1)*rad,c,t);
        }
    }
    line(r.x+rad,r.y,r.x+r.width-rad,r.y,c,t);
    line(r.x+rad,r.y+r.height,r.x+r.width-rad,r.y+r.height,c,t);
    line(r.x,r.y+rad,r.x,r.y+r.height-rad,c,t);
    line(r.x+r.width,r.y+rad,r.x+r.width,r.y+r.height-rad,c,t);
}
void OpenGLRenderer::line(float x1,float y1,float x2,float y2,Color c,float t) {
    const float dx=x2-x1, dy=y2-y1, len=std::sqrt(dx*dx+dy*dy);
    if (!m_frameOpen || len <= 0.0F || t <= 0.0F) return;
    if (!m_vertices.empty() && m_pendingTexture != 0U) flush(m_pendingTexture);
    const float ox=-dy/len*t*0.5F, oy=dx/len*t*0.5F;
    const Vertex a{x1+ox,y1+oy,-1,0,c.r,c.g,c.b,c.a};
    const Vertex b{x2+ox,y2+oy,-1,0,c.r,c.g,c.b,c.a};
    const Vertex d{x1-ox,y1-oy,-1,0,c.r,c.g,c.b,c.a};
    const Vertex e{x2-ox,y2-oy,-1,0,c.r,c.g,c.b,c.a};
    m_vertices.insert(m_vertices.end(),{a,b,d,b,e,d});
    m_pendingTexture=0;
}
Size OpenGLRenderer::measureText(std::string_view s,float size) const { return {static_cast<float>(s.size())*size*0.60F,size*1.25F}; }
void OpenGLRenderer::text(float x,float y,std::string_view s,Color c,float size) {
    if (!m_frameOpen) return;
    const float scale=size/static_cast<float>(detail::kFontCellHeight); const float cw=static_cast<float>(detail::kFontCellWidth)*scale,ch=static_cast<float>(detail::kFontCellHeight)*scale;
    for (unsigned char chByte : s) { unsigned int code=chByte; if(code<32U||code>126U) code= '?'; const unsigned int idx=code-32U; const unsigned int col=idx%static_cast<unsigned int>(detail::kFontColumns),row=idx/static_cast<unsigned int>(detail::kFontColumns); const float u=static_cast<float>(col*detail::kFontCellWidth)/static_cast<float>(detail::kFontAtlasWidth),v=static_cast<float>(row*detail::kFontCellHeight)/static_cast<float>(detail::kFontAtlasHeight); const float uw=static_cast<float>(detail::kFontCellWidth)/static_cast<float>(detail::kFontAtlasWidth),vh=static_cast<float>(detail::kFontCellHeight)/static_cast<float>(detail::kFontAtlasHeight); quad({x,y,cw,ch},{u,v,uw,vh},c,m_fontTexture); x+=cw*0.60F; }
}
void OpenGLRenderer::texture(std::uint32_t id,Rect r,Color c) { quad(r,{0,0,1,1},c,id); }

} // namespace pebble::ui
