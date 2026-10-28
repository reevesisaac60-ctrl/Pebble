#include "renderer/sky.hpp"

#include "renderer/renderer.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <vector>

#include <epoxy/gl.h>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Skybox cube vertices (position only), centered at origin, unit size.
// Faces are wound so the camera sees the inside of the cube.
const float kSkyboxVertices[] = {
    // Back face (z = -1)
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    // Front face (z = +1)
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    // Left face (x = -1)
    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    // Right face (x = +1)
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
    // Top face (y = +1)
    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    // Bottom face (y = -1)
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,
};

const unsigned int kSkyboxIndices[] = {
    0,  1,  2,  0,  2,  3,   // back
    4,  5,  6,  4,  6,  7,   // front
    8,  9,  10, 8,  10, 11,  // left
    12, 13, 14, 12, 14, 15,  // right
    16, 17, 18, 16, 18, 19,  // top
    20, 21, 22, 20, 22, 23,  // bottom
};

constexpr int kSkyboxIndexCount =
    sizeof(kSkyboxIndices) / sizeof(kSkyboxIndices[0]);

// Build a view matrix with the camera translation stripped out so the sky
// appears infinitely far away. The rotation part is preserved so the sky
// rotates correctly as the camera looks around.
Mat4 viewWithoutTranslation(const Mat4& view) {
    Mat4 result = view;
    result(0, 3) = 0.0f;
    result(1, 3) = 0.0f;
    result(2, 3) = 0.0f;
    return result;
}

// Apply a Y-axis rotation (environment rotation) to a direction vector.
Vec3 rotateY(const Vec3& v, float degrees) {
    const float rad = degrees * 3.14159265358979323846f / 180.0f;
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    return Vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

// Load a PPM (P6 binary) image. Returns true on success, filling out
// width, height, and rgb data (3 bytes per pixel).
bool loadPPM(const std::string& path, int& width, int& height,
             std::vector<unsigned char>& rgb) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::string magic;
    file >> magic;
    if (magic != "P6") {
        return false;
    }

    // Skip whitespace and comments.
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

    file >> width >> height;
    skipWhitespaceAndComments();

    int maxVal = 0;
    file >> maxVal;
    skipWhitespaceAndComments();

    if (width <= 0 || height <= 0 || maxVal != 255) {
        return false;
    }

    // Read the single whitespace byte after maxVal.
    file.ignore(1);

    rgb.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
    file.read(reinterpret_cast<char*>(rgb.data()),
              static_cast<std::streamsize>(rgb.size()));
    return file.good();
}

} // namespace

Sky::Sky() = default;

Sky::~Sky() {
    shutdown();
}

bool Sky::init(const std::string& assetsDir) {
    m_assetsDir = assetsDir;
    return loadProceduralShaders() && loadCubemapShaders();
}

void Sky::shutdown() {
    if (m_proceduralVAO != 0) {
        glDeleteVertexArrays(1, &m_proceduralVAO);
        m_proceduralVAO = 0;
    }
    if (m_proceduralVBO != 0) {
        glDeleteBuffers(1, &m_proceduralVBO);
        m_proceduralVBO = 0;
    }
    if (m_proceduralEBO != 0) {
        glDeleteBuffers(1, &m_proceduralEBO);
        m_proceduralEBO = 0;
    }
    if (m_cubemapVAO != 0) {
        glDeleteVertexArrays(1, &m_cubemapVAO);
        m_cubemapVAO = 0;
    }
    if (m_cubemapVBO != 0) {
        glDeleteBuffers(1, &m_cubemapVBO);
        m_cubemapVBO = 0;
    }
    if (m_cubemapEBO != 0) {
        glDeleteBuffers(1, &m_cubemapEBO);
        m_cubemapEBO = 0;
    }
}

bool Sky::loadProceduralShaders() {
    if (!m_proceduralShader.load(m_assetsDir + "/sky_procedural.vert",
                                 m_assetsDir + "/sky_procedural.frag")) {
        std::fprintf(stderr,
                     "Sky: failed to load procedural sky shaders\n");
        return false;
    }

    // Build a unit cube for the procedural sky (rendered inside-out).
    glGenVertexArrays(1, &m_proceduralVAO);
    glGenBuffers(1, &m_proceduralVBO);
    glGenBuffers(1, &m_proceduralEBO);

    glBindVertexArray(m_proceduralVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_proceduralVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kSkyboxVertices),
                 kSkyboxVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_proceduralEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kSkyboxIndices),
                 kSkyboxIndices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride,
                          reinterpret_cast<const void*>(kPositionOffset));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m_proceduralReady = true;
    std::fprintf(stdout, "Sky: procedural sky ready\n");
    return true;
}

bool Sky::loadCubemapShaders() {
    if (!m_cubemapShader.load(m_assetsDir + "/sky_cubemap.vert",
                              m_assetsDir + "/sky_cubemap.frag")) {
        std::fprintf(stderr,
                     "Sky: failed to load cubemap sky shaders\n");
        return false;
    }

    glGenVertexArrays(1, &m_cubemapVAO);
    glGenBuffers(1, &m_cubemapVBO);
    glGenBuffers(1, &m_cubemapEBO);

    glBindVertexArray(m_cubemapVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_cubemapVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kSkyboxVertices),
                 kSkyboxVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubemapEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kSkyboxIndices),
                 kSkyboxIndices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride,
                          reinterpret_cast<const void*>(kPositionOffset));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m_cubemapReady = true;
    std::fprintf(stdout, "Sky: cubemap sky ready\n");
    return true;
}

// Load a cubemap from 6 PPM files in a directory. Expected file names:
//   posx.ppm, negx.ppm, posy.ppm, negy.ppm, posz.ppm, negz.ppm
// Returns the OpenGL cubemap texture ID, or 0 on failure.
GLuint Sky::loadCubemap(const std::string& directory) const {
    const char* faces[6] = {
        "posx.ppm", "negx.ppm",
        "posy.ppm", "negy.ppm",
        "posz.ppm", "negz.ppm",
    };
    const GLenum targets[6] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
    };

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

    int width = 0, height = 0;
    std::vector<unsigned char> rgb;

    bool success = true;
    for (int i = 0; i < 6; ++i) {
        std::string path = directory + "/" + faces[i];
        if (!loadPPM(path, width, height, rgb)) {
            std::fprintf(stderr,
                         "Sky: failed to load cubemap face '%s'\n",
                         path.c_str());
            success = false;
            break;
        }
        glTexImage2D(targets[i], 0, GL_RGB, width, height, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, rgb.data());
    }

    if (success) {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER,
                        GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,
                        GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        std::fprintf(stdout, "Sky: cubemap loaded from '%s' (%dx%d)\n",
                     directory.c_str(), width, height);
    } else {
        glDeleteTextures(1, &texture);
        texture = 0;
    }

    return texture;
}

bool Sky::render(const RendererCamera& camera,
                 const RendererEnvironment& environment) {
    // Strip camera translation so the sky stays infinitely far away.
    Mat4 view = viewWithoutTranslation(camera.view);

    // Apply environment rotation around world Y to the view basis.
    if (environment.environmentRotation != 0.0f) {
        Vec3 col0(view(0, 0), view(1, 0), view(2, 0));
        Vec3 col1(view(0, 1), view(1, 1), view(2, 1));
        Vec3 col2(view(0, 2), view(1, 2), view(2, 2));
        col0 = rotateY(col0, environment.environmentRotation);
        col1 = rotateY(col1, environment.environmentRotation);
        col2 = rotateY(col2, environment.environmentRotation);
        view(0, 0) = col0.x;
        view(1, 0) = col0.y;
        view(2, 0) = col0.z;
        view(0, 1) = col1.x;
        view(1, 1) = col1.y;
        view(2, 1) = col1.z;
        view(0, 2) = col2.x;
        view(1, 2) = col2.y;
        view(2, 2) = col2.z;
    }

    // Ensure translation is zero (viewWithoutTranslation already did this,
    // but the rotation step above only touched columns 0-2).
    view(0, 3) = 0.0f;
    view(1, 3) = 0.0f;
    view(2, 3) = 0.0f;

    // Disable depth writes and use LEQUAL so geometry can occlude the sky.
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    if (environment.skyMode == 1 && environment.skyboxTexture != 0 &&
        m_cubemapReady) {
        // Textured skybox: sample the cubemap.
        m_cubemapShader.use();
        m_cubemapShader.setUniformMatrix4fv("uView",
                                            view.pointer());
        m_cubemapShader.setUniformMatrix4fv("uProjection",
                                            camera.projection.pointer());
        m_cubemapShader.setUniform1i("uSkybox", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, environment.skyboxTexture);

        glBindVertexArray(m_cubemapVAO);
        glDrawElements(GL_TRIANGLES, kSkyboxIndexCount, GL_UNSIGNED_INT,
                       nullptr);
        glBindVertexArray(0);

        glUseProgram(0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    } else if (m_proceduralReady) {
        // Procedural gradient sky.
        m_proceduralShader.use();
        m_proceduralShader.setUniformMatrix4fv("uView",
                                               view.pointer());
        m_proceduralShader.setUniformMatrix4fv("uProjection",
                                               camera.projection.pointer());
        m_proceduralShader.setUniform3f("uSkyColor",
                                        environment.skyColor.x,
                                        environment.skyColor.y,
                                        environment.skyColor.z);
        m_proceduralShader.setUniform3f("uHorizonColor",
                                        environment.horizonColor.x,
                                        environment.horizonColor.y,
                                        environment.horizonColor.z);
        m_proceduralShader.setUniform1f("uSkyIntensity",
                                        environment.skyIntensity);

        glBindVertexArray(m_proceduralVAO);
        glDrawElements(GL_TRIANGLES, kSkyboxIndexCount, GL_UNSIGNED_INT,
                       nullptr);
        glBindVertexArray(0);

        glUseProgram(0);
    } else {
        // No sky renderer available: restore state and skip.
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        return false;
    }

    // Restore default depth state.
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    return true;
}
