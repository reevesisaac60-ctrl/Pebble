#include "renderer/renderer.hpp"

#include "scene/camera3d.hpp"
#include "scene/directional_light.hpp"
#include "scene/node3d.hpp"
#include "scene/part.hpp"
#include "scene/point_light.hpp"
#include "scene/scene.hpp"
#include "scene/world_environment.hpp"

#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <vector>
#include <string>

#include <epoxy/gl.h>

#include "renderer/model_importer.hpp"

namespace {

constexpr char kDefaultMeshName[] = "cube";
constexpr char kDefaultMaterialName[] = "default";

// A simple 1x1 white pixel texture used as a fallback when a material
// references a texture that cannot be loaded.
GLuint createFallbackWhiteTexture() {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    const unsigned char whitePixel[4] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

// Load a PPM (P6 binary) image into an OpenGL 2D texture. Returns the texture
// ID, or 0 on failure.
GLuint loadPPMTexture(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::fprintf(stderr, "Texture: cannot open '%s'\n", path.c_str());
        return 0;
    }

    std::string magic;
    file >> magic;
    if (magic != "P6") {
        std::fprintf(stderr, "Texture: '%s' is not a P6 PPM image\n", path.c_str());
        return 0;
    }

    auto skipWhitespaceAndComments = [&file]() {
        int c;
        while ((c = file.peek()) != EOF) {
            if (c == '#') {
                file.ignore(std::numeric_limits<int>::max(), '\n');
            } else if (std::isspace(c)) {
                file.ignore(1);
            } else {
                break;
            }
        }
    };
    skipWhitespaceAndComments();

    int width = 0, height = 0;
    file >> width >> height;
    skipWhitespaceAndComments();

    int maxVal = 0;
    file >> maxVal;
    skipWhitespaceAndComments();

    if (width <= 0 || height <= 0 || maxVal != 255) {
        std::fprintf(stderr, "Texture: invalid PPM dimensions/maxVal in '%s'\n",
                     path.c_str());
        return 0;
    }

    file.ignore(1);

    std::vector<unsigned char> rgb(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
    file.read(reinterpret_cast<char*>(rgb.data()),
              static_cast<std::streamsize>(rgb.size()));
    if (!file.good()) {
        std::fprintf(stderr, "Texture: failed to read pixel data from '%s'\n",
                     path.c_str());
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, rgb.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    std::fprintf(stdout, "Texture: loaded '%s' (%dx%d)\n", path.c_str(),
                 width, height);
    return texture;
}

} // namespace

Renderer::Renderer() = default;

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init(const std::string& assetsDir) {
    m_assetsDir = assetsDir;

    if (!m_shader.load(m_assetsDir + "/cube.vert", m_assetsDir + "/cube.frag")) {
        std::fprintf(stderr, "Renderer: failed to load the cube shader\n");
        return false;
    }

    // Create default meshes for all primitive types.
    m_meshManager.registerMesh(Mesh::createCube(kDefaultMeshName));
    m_meshManager.registerMesh(Mesh::createSphere("sphere"));
    m_meshManager.registerMesh(Mesh::createPlane("plane"));
    m_meshManager.registerMesh(Mesh::createCylinder("cylinder"));
    m_meshManager.registerMesh(Mesh::createCone("cone"));
    m_meshManager.registerMesh(Mesh::createCapsule("capsule"));

    // Create a default material.
    m_materialManager.registerMaterial(Material(kDefaultMaterialName));

    if (!createFramebuffer(1, 1)) {
        std::fprintf(stderr, "Renderer: failed to create the offscreen framebuffer\n");
        return false;
    }

    // Create a fallback white texture once (reused for untextured materials
    // and failed texture loads, instead of being recreated every frame).
    m_fallbackTexture = createFallbackWhiteTexture();

    if (!m_sky.init(m_assetsDir)) {
        std::fprintf(stderr, "Renderer: failed to initialize sky renderer\n");
        return false;
    }

    std::fprintf(stdout,
                 "Renderer: initialized with %zu mesh(es), %zu material(s)\n",
                 m_meshManager.names().size(),
                 m_materialManager.names().size());
    return true;
}

int Renderer::loadGlbModels(const std::string& modelsDir,
                            std::vector<ImportedNode>& outNodes) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(modelsDir, ec) || !fs::is_directory(modelsDir, ec)) {
        std::fprintf(stderr,
                     "Renderer: models directory does not exist: '%s'\n",
                     modelsDir.c_str());
        return 0;
    }

    ModelImporter importer;
    int loaded = 0;

    // Collect .glb and .gltf files (sorted for deterministic order).
    std::vector<std::string> modelFiles;
    for (fs::directory_iterator it(modelsDir, ec), end;
         it != end && !ec;
         it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        if (ec) continue;
        std::string ext = it->path().extension().generic_string();
        if (ext == ".glb" || ext == ".gltf") {
            modelFiles.push_back(it->path().generic_string());
        }
    }
    std::sort(modelFiles.begin(), modelFiles.end());

    for (const auto& filePath : modelFiles) {
        ImportResult result = importer.importFile(filePath);
        if (!result.success) {
            std::fprintf(stderr,
                         "Renderer: failed to import '%s': %s\n",
                         filePath.c_str(), result.error.c_str());
            continue;
        }
        for (const auto& imported : result.meshes) {
            Mesh mesh = Mesh::fromData(imported.name, imported.data);
            if (mesh.ready) {
                m_meshManager.registerMesh(std::move(mesh));
                ++loaded;
                std::fprintf(stdout,
                             "Renderer: loaded model '%s' from '%s' "
                             "(%d vertices, %d indices)\n",
                             imported.name.c_str(), filePath.c_str(),
                             imported.data.vertexCount,
                             imported.data.indexCount);
            } else {
                std::fprintf(stderr,
                             "Renderer: failed to upload mesh '%s' from '%s'\n",
                             imported.name.c_str(), filePath.c_str());
            }
        }
        // Append imported node data (names, transforms, hierarchy).
        outNodes.insert(outNodes.end(), result.nodes.begin(),
                        result.nodes.end());
    }

    std::fprintf(stdout,
                 "Renderer: loaded %d model(s) from '%s'\n",
                 loaded, modelsDir.c_str());
    return loaded;
}

bool Renderer::loadGlbModel(const std::string& filePath,
                            const std::string& name) {
    ModelImporter importer;
    ImportResult result = importer.importFile(filePath);
    if (!result.success || result.meshes.empty()) {
        std::fprintf(stderr,
                     "Renderer: failed to load model '%s': %s\n",
                     filePath.c_str(),
                     result.error.empty() ? "no meshes" : result.error.c_str());
        return false;
    }
    // Use the first mesh, registered under the requested name.
    ImportedMesh& first = result.meshes[0];
    Mesh importedMesh = Mesh::fromData(name, first.data);
    if (!importedMesh.ready) {
        std::fprintf(stderr, "Renderer: failed to upload model '%s'\n",
                     filePath.c_str());
        return false;
    }
    m_meshManager.registerMesh(std::move(importedMesh));
    std::fprintf(stdout,
                 "Renderer: loaded model '%s' as '%s' (%d vertices, %d "
                 "indices)\n",
                 filePath.c_str(), name.c_str(),
                 first.data.vertexCount, first.data.indexCount);
    return true;
}

void Renderer::shutdown() {
    m_sky.shutdown();
    destroyFramebuffer();
    m_meshManager.releaseAll();
    m_materialManager.clear();

    // Release cached textures and the fallback texture.
    for (const auto& cached : m_textureCache) {
        if (cached.second != 0) {
            glDeleteTextures(1, &cached.second);
        }
    }
    m_textureCache.clear();
    if (m_fallbackTexture != 0) {
        glDeleteTextures(1, &m_fallbackTexture);
        m_fallbackTexture = 0;
    }
}

GLuint Renderer::loadSkyboxCubemap(const std::string& directory) const {
    return m_sky.loadCubemap(directory);
}

std::string Renderer::resolveTexturePath(
    const std::string& relativePath) const {
    return m_assetsDir + "/" + relativePath;
}

void Renderer::resize(int width, int height) {
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;

    if (width == m_fboWidth && height == m_fboHeight) {
        return;
    }

    destroyFramebuffer();
    createFramebuffer(width, height);
}

bool Renderer::createFramebuffer(int width, int height) {
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           m_colorTexture, 0);

    glGenRenderbuffers(1, &m_depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, m_depthRenderbuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "Renderer: incomplete offscreen framebuffer\n");
        destroyFramebuffer();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    m_fboWidth = width;
    m_fboHeight = height;

    std::fprintf(stdout, "Renderer: framebuffer %dx%d\n", width, height);
    return true;
}

void Renderer::destroyFramebuffer() {
    if (m_fbo != 0) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_colorTexture != 0) {
        glDeleteTextures(1, &m_colorTexture);
        m_colorTexture = 0;
    }
    if (m_depthRenderbuffer != 0) {
        glDeleteRenderbuffers(1, &m_depthRenderbuffer);
        m_depthRenderbuffer = 0;
    }
    m_fboWidth = 0;
    m_fboHeight = 0;
}

void Renderer::bindPartUniforms(const Mat4& model) const {
    m_shader.setUniformMatrix4fv("uModel", model.pointer());
}

void Renderer::bindLightingUniforms(const RendererLighting& lighting) const {
    m_shader.setUniform3f("uDirLightDirection", lighting.direction.x,
                          lighting.direction.y, lighting.direction.z);
    m_shader.setUniform3f("uDirLightColor", lighting.color.x, lighting.color.y,
                          lighting.color.z);
    m_shader.setUniform1f("uDirLightIntensity", lighting.intensity);
    m_shader.setUniform1f("uAmbientIntensity", lighting.ambientIntensity);

    m_shader.setUniform1i("uPointLightCount", lighting.count);
    for (int i = 0; i < kMaxPointLights; ++i) {
        m_shader.setUniform3f("uPointLightPosition[" + std::to_string(i) + "]",
                              lighting.position[i].x, lighting.position[i].y,
                              lighting.position[i].z);
        m_shader.setUniform3f("uPointLightColor[" + std::to_string(i) + "]",
                              lighting.pointColor[i].x, lighting.pointColor[i].y,
                              lighting.pointColor[i].z);
        m_shader.setUniform1f("uPointLightIntensity[" + std::to_string(i) + "]",
                              lighting.pointIntensity[i]);
        m_shader.setUniform1f("uPointLightRange[" + std::to_string(i) + "]",
                              lighting.pointRange[i]);
    }
}

void Renderer::bindEnvironmentUniforms(
    const RendererEnvironment& environment) const {
    m_shader.setUniform3f("uFogColor", environment.fogColor.x,
                          environment.fogColor.y, environment.fogColor.z);
    m_shader.setUniform1f("uFogStart", environment.fogStart);
    m_shader.setUniform1f("uFogEnd", environment.fogEnd);
    m_shader.setUniform1i("uFogEnabled", environment.fogEnabled ? 1 : 0);
}

void Renderer::bindMaterialUniforms(const Material& material) const {
    m_shader.setUniform3f("uMaterialBaseColor", material.baseColor.x,
                          material.baseColor.y, material.baseColor.z);
    m_shader.setUniform1f("uMaterialMetallic", material.metallic);
    m_shader.setUniform1f("uMaterialRoughness", material.roughness);
    m_shader.setUniform1i("uMaterialHasTexture",
                          (material.hasTexture && !material.texturePath.empty()) ? 1 : 0);
}

void Renderer::render(const Scene& scene, const RendererCamera& camera,
                      const RendererLighting& lighting,
                      const RendererEnvironment& environment) {
    if (m_fboWidth <= 0 || m_fboHeight <= 0) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_fboWidth, m_fboHeight);

    glClearColor(0.04f, 0.045f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    const float aspect =
        static_cast<float>(m_fboWidth) / static_cast<float>(m_fboHeight);

    m_shader.use();
    m_shader.setUniformMatrix4fv("uView", camera.view.pointer());
    m_shader.setUniformMatrix4fv("uProjection", camera.projection.pointer());
    m_shader.setUniform3f("uViewPosition", camera.position.x,
                          camera.position.y, camera.position.z);

    bindLightingUniforms(lighting);
    bindEnvironmentUniforms(environment);

    // Draw the sky behind all scene geometry. Depth writes are disabled
    // inside Sky::render so geometry can occlude it.
    m_sky.render(camera, environment);

    // Sky::render unbinds the shader program; rebind it for the part draws.
    m_shader.use();

    glActiveTexture(GL_TEXTURE0);

    int partCount = 0;
    for (const NodeId id : scene.nodeIds()) {
        const Node* node = scene.find(id);
        if (node == nullptr) {
            continue;
        }
        const Part* part = dynamic_cast<const Part*>(node);
        if (part == nullptr) {
            continue;
        }

        // Look up the mesh; fall back to the default cube mesh.
        const Mesh* mesh = m_meshManager.find(part->meshName);
        if (mesh == nullptr || !mesh->ready) {
            mesh = m_meshManager.find(kDefaultMeshName);
        }
        if (mesh == nullptr || !mesh->ready) {
            continue;
        }

        // Look up the material; fall back to the default material.
        const Material* material = m_materialManager.find(part->materialName);
        if (material == nullptr) {
            material = m_materialManager.find(kDefaultMaterialName);
        }
        if (material == nullptr) {
            continue;
        }

        // Bind the mesh VAO.
        mesh->bind();

        // Bind material uniforms.
        bindMaterialUniforms(*material);

        // Handle texture binding.
        if (material->hasTexture && !material->texturePath.empty()) {
            // Look up the texture in the cache. Load it on first use and
            // cache by project-relative path so it is not re-decoded every
            // frame.
            GLuint texture = 0;
            auto cached = m_textureCache.find(material->texturePath);
            if (cached != m_textureCache.end()) {
                texture = cached->second;
            } else {
                std::string fullPath = resolveTexturePath(material->texturePath);
                texture = loadPPMTexture(fullPath);
                if (texture != 0) {
                    m_textureCache.emplace(material->texturePath, texture);
                }
            }
            if (texture != 0) {
                glBindTexture(GL_TEXTURE_2D, texture);
            } else {
                glBindTexture(GL_TEXTURE_2D, m_fallbackTexture);
            }
            m_shader.setUniform1i("uMaterialTexture", 0);
        } else {
            glBindTexture(GL_TEXTURE_2D, m_fallbackTexture);
            m_shader.setUniform1i("uMaterialTexture", 0);
        }

        m_shader.setUniformMatrix4fv("uModel",
                                     part->globalTransform().pointer());
        glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT,
                       nullptr);
        ++partCount;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    (void)partCount;
    (void)aspect;
}
