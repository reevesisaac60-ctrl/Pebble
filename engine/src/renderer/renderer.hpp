#pragma once

#include <epoxy/gl.h>

#include <cstddef>
#include <string>

#include "math/mat4.hpp"
#include "math/vec3.hpp"
#include "renderer/material.hpp"
#include "renderer/mesh.hpp"
#include "renderer/model_importer.hpp"
#include "renderer/shader.hpp"
#include "renderer/sky.hpp"

class Scene;

// Maximum point lights the shader supports per frame. The renderer clamps the
// scene's point-light count to this; extra point lights are ignored safely.
static constexpr int kMaxPointLights = 4;

// Renderer input: the camera and environment state used to render a scene.
// The renderer consumes these values; it does not own scene state.
struct RendererCamera {
    // View matrix (world-to-camera).
    Mat4 view = Mat4();
    // Projection matrix.
    Mat4 projection = Mat4();
    // Camera world-space position (used for specular and fog distance).
    Vec3 position = Vec3(0.0f, 0.0f, 3.0f);
    bool valid = false;
};

struct RendererLighting {
    // Directional light (single, always evaluated).
    Vec3 direction = Vec3(0.0f, 0.0f, -1.0f);
    Vec3 color = Vec3(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    float ambientIntensity = 0.2f;

    // Point lights (clamped to kMaxPointLights by the renderer).
    int count = 0;
    Vec3 position[kMaxPointLights] = {};
    Vec3 pointColor[kMaxPointLights] = {};
    float pointIntensity[kMaxPointLights] = {};
    float pointRange[kMaxPointLights] = {};
};

struct RendererEnvironment {
    Vec3 fogColor = Vec3(0.0f, 0.0f, 0.0f);
    float fogStart = 0.0f;
    float fogEnd = 100.0f;
    bool fogEnabled = false;

    // Sky rendering mode: 0 = procedural gradient sky, 1 = textured skybox.
    int skyMode = 0;

    // Procedural sky colors.
    Vec3 skyColor = Vec3(0.4f, 0.6f, 0.85f);
    Vec3 horizonColor = Vec3(0.8f, 0.85f, 0.9f);
    float skyIntensity = 1.0f;

    // Environment rotation around world Y, in degrees.
    float environmentRotation = 0.0f;

    // OpenGL texture ID for a loaded skybox (cubemap), 0 when none.
    GLuint skyboxTexture = 0;

    // Path to the skybox image directory (relative to assets), used when
    // skyMode == 1. Not a render uniform; used only for texture loading.
    std::string skyboxPath;
};

// Renders every Part node in the scene using its referenced mesh and
// material, lit by the scene's directional and point lights and fogged by
// the world environment. Scene data (nodes, transforms) lives in the scene;
// the renderer owns only the mesh/material/shader resources and framebuffer.
class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(const std::string& assetsDir);
    void shutdown();

    void resize(int width, int height);

    // Load glTF/GLB model assets from a directory. All .glb/.gltf files in
    // the directory are imported and registered as named meshes. Returns the
    // number of models successfully loaded. Imported node data (names,
    // transforms, hierarchy, mesh references) is appended to outNodes.
    int loadGlbModels(const std::string& modelsDir,
                      std::vector<ImportedNode>& outNodes);

    // Load a single glTF/GLB model file and register it under the given name.
    bool loadGlbModel(const std::string& filePath, const std::string& name);

    // Load a cubemap skybox from 6 PPM files in a directory. Returns the
    // OpenGL texture ID, or 0 on failure.
    GLuint loadSkyboxCubemap(const std::string& directory) const;

    // Resource access.
    MeshManager& meshManager() { return m_meshManager; }
    const MeshManager& meshManager() const { return m_meshManager; }
    MaterialManager& materialManager() { return m_materialManager; }
    const MaterialManager& materialManager() const { return m_materialManager; }

    // Resolve a project-relative texture path to an absolute filesystem path.
    std::string resolveTexturePath(const std::string& relativePath) const;

    // Render every Part node of the scene. The camera, lighting and
    // environment are supplied explicitly so the renderer stays decoupled
    // from scene ownership.
    void render(const Scene& scene, const RendererCamera& camera,
                const RendererLighting& lighting,
                const RendererEnvironment& environment);

    GLuint colorTexture() const { return m_colorTexture; }

    int width() const { return m_fboWidth; }
    int height() const { return m_fboHeight; }

private:
    bool createFramebuffer(int width, int height);
    void destroyFramebuffer();

    void bindPartUniforms(const Mat4& model) const;
    void bindLightingUniforms(const RendererLighting& lighting) const;
    void bindEnvironmentUniforms(const RendererEnvironment& environment) const;
    void bindMaterialUniforms(const Material& material) const;

    std::string m_assetsDir;
    Shader m_shader;
    Sky m_sky;
    MeshManager m_meshManager;
    MaterialManager m_materialManager;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;

    GLuint m_fbo = 0;
    GLuint m_colorTexture = 0;
    GLuint m_depthRenderbuffer = 0;
    int m_fboWidth = 0;
    int m_fboHeight = 0;

    // Fallback white texture (created once, released on shutdown).
    GLuint m_fallbackTexture = 0;

    // Texture cache: project-relative path -> OpenGL texture ID.
    std::unordered_map<std::string, GLuint> m_textureCache;

    // Vertex layout: position(3) + normal(3) + texCoord(2) = 8 floats.
    static constexpr int kStride = 8 * sizeof(float);
    static constexpr int kPositionOffset = 0;
    static constexpr int kNormalOffset = 3 * sizeof(float);
    static constexpr int kTexCoordOffset = 6 * sizeof(float);
};
