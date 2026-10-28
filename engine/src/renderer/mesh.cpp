#include "renderer/mesh.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include <epoxy/gl.h>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Vertex: position(3) + normal(3) + texCoord(2) = 8 floats.
constexpr int kVertexStride = 8;
constexpr int kPositionOffset = 0;
constexpr int kNormalOffset = 3;
constexpr int kTexCoordOffset = 6;

// Build an index list from a flat index function.
inline int vertexIndex(int row, int col, int cols) {
    return row * cols + col;
}

// Compute a normalized normal from three vertices (for flat-shaded faces).
inline Vec3 faceNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    return cross(b - a, c - a).normalized();
}

// Append a vertex (position, normal, texCoord) to the vertex buffer.
inline void appendVertex(std::vector<float>& vertices, const Vec3& position,
                         const Vec3& normal, float u, float v) {
    vertices.push_back(position.x);
    vertices.push_back(position.y);
    vertices.push_back(position.z);
    vertices.push_back(normal.x);
    vertices.push_back(normal.y);
    vertices.push_back(normal.z);
    vertices.push_back(u);
    vertices.push_back(v);
}

// Upload mesh data to GPU buffers. Requires an OpenGL context.
Mesh uploadMesh(const std::string& name, const MeshData& data) {
    Mesh mesh(name);
    mesh.vertexCount = data.vertexCount;
    mesh.indexCount = data.indexCount;

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.vertices.size() * sizeof(float)),
                 data.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.indices.size() * sizeof(unsigned int)),
                 data.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          kVertexStride * sizeof(float),
                          reinterpret_cast<const void*>(kPositionOffset * sizeof(float)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          kVertexStride * sizeof(float),
                          reinterpret_cast<const void*>(kNormalOffset * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                          kVertexStride * sizeof(float),
                          reinterpret_cast<const void*>(kTexCoordOffset * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    mesh.ready = true;
    return mesh;
}

} // namespace

Mesh::~Mesh() {
    release();
}

Mesh::Mesh(Mesh&& other) noexcept
    : name(std::move(other.name)),
      vao(other.vao),
      vbo(other.vbo),
      ebo(other.ebo),
      indexCount(other.indexCount),
      vertexCount(other.vertexCount),
      ready(other.ready) {
    other.vao = 0;
    other.vbo = 0;
    other.ebo = 0;
    other.indexCount = 0;
    other.vertexCount = 0;
    other.ready = false;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        release();
        name = std::move(other.name);
        vao = other.vao;
        vbo = other.vbo;
        ebo = other.ebo;
        indexCount = other.indexCount;
        vertexCount = other.vertexCount;
        ready = other.ready;
        other.vao = 0;
        other.vbo = 0;
        other.ebo = 0;
        other.indexCount = 0;
        other.vertexCount = 0;
        other.ready = false;
    }
    return *this;
}

void Mesh::release() {
    if (ebo != 0) {
        glDeleteBuffers(1, &ebo);
        ebo = 0;
    }
    if (vbo != 0) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    indexCount = 0;
    vertexCount = 0;
    ready = false;
}

void Mesh::bind() const {
    glBindVertexArray(vao);
}

Mesh Mesh::fromData(const std::string& name, const MeshData& data) {
    return uploadMesh(name, data);
}

// ---------------------------------------------------------------------------
// Cube data.
// ---------------------------------------------------------------------------
MeshData Mesh::createCubeData(const Vec3& size) {
    MeshData data;

    const float sx = size.x * 0.5f;
    const float sy = size.y * 0.5f;
    const float sz = size.z * 0.5f;

    // Positions for the 8 corners.
    const Vec3 corners[8] = {
        Vec3(-sx, -sy, -sz), // 0: left-bottom-back
        Vec3( sx, -sy, -sz), // 1: right-bottom-back
        Vec3( sx,  sy, -sz), // 2: right-top-back
        Vec3(-sx,  sy, -sz), // 3: left-top-back
        Vec3(-sx, -sy,  sz), // 4: left-bottom-front
        Vec3( sx, -sy,  sz), // 5: right-bottom-front
        Vec3( sx,  sy,  sz), // 6: right-top-front
        Vec3(-sx,  sy,  sz), // 7: left-top-front
    };

    // Face definitions: 4 corner indices + normal.
    struct Face {
        int v[4];
        Vec3 normal;
    };
    const Face faces[6] = {
        {{0, 1, 2, 3}, Vec3(0.0f, 0.0f, -1.0f)}, // back
        {{4, 5, 6, 7}, Vec3(0.0f, 0.0f, 1.0f)},  // front
        {{0, 4, 7, 3}, Vec3(-1.0f, 0.0f, 0.0f)}, // left
        {{1, 5, 6, 2}, Vec3(1.0f, 0.0f, 0.0f)},  // right
        {{3, 2, 6, 7}, Vec3(0.0f, 1.0f, 0.0f)},  // top
        {{0, 1, 5, 4}, Vec3(0.0f, -1.0f, 0.0f)}, // bottom
    };

    // UVs per face corner.
    const float uvs[4][2] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},
    };

    data.vertices.reserve(6 * 4 * kVertexStride);
    data.indices.reserve(6 * 6);

    for (int f = 0; f < 6; ++f) {
        const Face& face = faces[f];
        const int base = data.vertexCount;
        for (int c = 0; c < 4; ++c) {
            const Vec3& p = corners[face.v[c]];
            appendVertex(data.vertices, p, face.normal, uvs[c][0], uvs[c][1]);
        }
        data.vertexCount += 4;

        // Two triangles per face.
        data.indices.push_back(static_cast<unsigned int>(base + 0));
        data.indices.push_back(static_cast<unsigned int>(base + 1));
        data.indices.push_back(static_cast<unsigned int>(base + 2));
        data.indices.push_back(static_cast<unsigned int>(base + 0));
        data.indices.push_back(static_cast<unsigned int>(base + 2));
        data.indices.push_back(static_cast<unsigned int>(base + 3));
    }
    data.indexCount = static_cast<int>(data.indices.size());

    return data;
}

Mesh Mesh::createCube(const std::string& name, const Vec3& size) {
    return Mesh::fromData(name, Mesh::createCubeData(size));
}

// ---------------------------------------------------------------------------
// UV sphere data.
// ---------------------------------------------------------------------------
MeshData Mesh::createSphereData(float radius, int sectors, int stacks) {
    MeshData data;

    if (radius <= 0.0f) radius = 0.1f;
    if (sectors < 3) sectors = 3;
    if (stacks < 2) stacks = 2;

    const int cols = sectors + 1;
    const int rows = stacks + 1;

    data.vertices.reserve(rows * cols * kVertexStride);

    for (int i = 0; i < rows; ++i) {
        const float lat = kPi * static_cast<float>(i) / static_cast<float>(stacks);
        const float sinLat = std::sin(lat);
        const float cosLat = std::cos(lat);
        for (int j = 0; j < cols; ++j) {
            const float lon = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sectors);
            const float sinLon = std::sin(lon);
            const float cosLon = std::cos(lon);

            const Vec3 normal(sinLat * cosLon, cosLat, sinLat * sinLon);
            const Vec3 position = normal * radius;
            const float u = static_cast<float>(j) / static_cast<float>(sectors);
            const float v = static_cast<float>(i) / static_cast<float>(stacks);

            appendVertex(data.vertices, position, normal, u, v);
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < sectors; ++j) {
            const int first = vertexIndex(i, j, cols);
            const int second = vertexIndex(i + 1, j, cols);
            data.indices.push_back(static_cast<unsigned int>(first));
            data.indices.push_back(static_cast<unsigned int>(second));
            data.indices.push_back(static_cast<unsigned int>(first + 1));

            data.indices.push_back(static_cast<unsigned int>(second));
            data.indices.push_back(static_cast<unsigned int>(second + 1));
            data.indices.push_back(static_cast<unsigned int>(first + 1));
        }
    }

    data.vertexCount = static_cast<int>(data.vertices.size() / kVertexStride);
    data.indexCount = static_cast<int>(data.indices.size());

    return data;
}

Mesh Mesh::createSphere(const std::string& name, float radius, int sectors,
                        int stacks) {
    return Mesh::fromData(name, Mesh::createSphereData(radius, sectors, stacks));
}

// ---------------------------------------------------------------------------
// Plane data.
// ---------------------------------------------------------------------------
MeshData Mesh::createPlaneData(float width, float height,
                               int segmentsX, int segmentsY) {
    MeshData data;

    const float sx = width * 0.5f;
    const float sy = height * 0.5f;
    if (segmentsX < 1) segmentsX = 1;
    if (segmentsY < 1) segmentsY = 1;

    const int cols = segmentsX + 1;
    const int rows = segmentsY + 1;

    data.vertices.reserve(rows * cols * kVertexStride);

    for (int i = 0; i < rows; ++i) {
        const float v = static_cast<float>(i) / static_cast<float>(segmentsY);
        const float y = (v - 0.5f) * 2.0f * sy;
        for (int j = 0; j < cols; ++j) {
            const float u = static_cast<float>(j) / static_cast<float>(segmentsX);
            const float x = (u - 0.5f) * 2.0f * sx;

            appendVertex(data.vertices, Vec3(x, y, 0.0f),
                         Vec3(0.0f, 1.0f, 0.0f), u, v);
        }
    }

    for (int i = 0; i < segmentsY; ++i) {
        for (int j = 0; j < segmentsX; ++j) {
            const int first = vertexIndex(i, j, cols);
            const int second = vertexIndex(i + 1, j, cols);
            data.indices.push_back(static_cast<unsigned int>(first));
            data.indices.push_back(static_cast<unsigned int>(second));
            data.indices.push_back(static_cast<unsigned int>(first + 1));

            data.indices.push_back(static_cast<unsigned int>(second));
            data.indices.push_back(static_cast<unsigned int>(second + 1));
            data.indices.push_back(static_cast<unsigned int>(first + 1));
        }
    }

    data.vertexCount = static_cast<int>(data.vertices.size() / kVertexStride);
    data.indexCount = static_cast<int>(data.indices.size());

    return data;
}

Mesh Mesh::createPlane(const std::string& name, float width, float height,
                       int segmentsX, int segmentsY) {
    return Mesh::fromData(name, Mesh::createPlaneData(width, height, segmentsX, segmentsY));
}

// ---------------------------------------------------------------------------
// Cylinder data.
// ---------------------------------------------------------------------------
MeshData Mesh::createCylinderData(float radius, float height, int sectors) {
    MeshData data;

    if (radius <= 0.0f) radius = 0.1f;
    if (height <= 0.0f) height = 0.1f;
    if (sectors < 3) sectors = 3;

    const float halfHeight = height * 0.5f;
    const int cols = sectors + 1;

    data.vertices.reserve((2 * cols + 2) * kVertexStride);

    // Side vertices: two rings (bottom and top).
    for (int i = 0; i < 2; ++i) {
        const float y = (i == 0) ? -halfHeight : halfHeight;
        const float v = static_cast<float>(i);
        for (int j = 0; j < cols; ++j) {
            const float angle = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sectors);
            const float x = radius * std::cos(angle);
            const float z = radius * std::sin(angle);
            const float u = static_cast<float>(j) / static_cast<float>(sectors);

            const Vec3 normal(std::cos(angle), 0.0f, std::sin(angle));

            appendVertex(data.vertices, Vec3(x, y, z), normal, u, v);
        }
    }

    // Side indices.
    const int sideBase = 0;
    for (int j = 0; j < sectors; ++j) {
        const int bottomLeft = sideBase + j;
        const int bottomRight = sideBase + j + 1;
        const int topLeft = sideBase + cols + j;
        const int topRight = sideBase + cols + j + 1;

        data.indices.push_back(static_cast<unsigned int>(bottomLeft));
        data.indices.push_back(static_cast<unsigned int>(topLeft));
        data.indices.push_back(static_cast<unsigned int>(bottomRight));

        data.indices.push_back(static_cast<unsigned int>(topLeft));
        data.indices.push_back(static_cast<unsigned int>(topRight));
        data.indices.push_back(static_cast<unsigned int>(bottomRight));
    }

    // Center vertices for caps.
    const int bottomCenterIdx = data.vertexCount;
    appendVertex(data.vertices, Vec3(0.0f, -halfHeight, 0.0f),
                 Vec3(0.0f, -1.0f, 0.0f), 0.5f, 0.0f);

    const int topCenterIdx = data.vertexCount;
    appendVertex(data.vertices, Vec3(0.0f, halfHeight, 0.0f),
                 Vec3(0.0f, 1.0f, 0.0f), 0.5f, 1.0f);

    // Bottom cap: triangle fan around bottom center.
    for (int j = 0; j < sectors; ++j) {
        data.indices.push_back(static_cast<unsigned int>(bottomCenterIdx));
        data.indices.push_back(static_cast<unsigned int>((j + 1) % sectors));
        data.indices.push_back(static_cast<unsigned int>(j));
    }

    // Top cap: triangle fan around top center.
    for (int j = 0; j < sectors; ++j) {
        data.indices.push_back(static_cast<unsigned int>(topCenterIdx));
        data.indices.push_back(static_cast<unsigned int>(cols + j));
        data.indices.push_back(static_cast<unsigned int>(cols + (j + 1) % sectors));
    }

    data.vertexCount = static_cast<int>(data.vertices.size() / kVertexStride);
    data.indexCount = static_cast<int>(data.indices.size());

    return data;
}

Mesh Mesh::createCylinder(const std::string& name, float radius, float height,
                          int sectors) {
    return Mesh::fromData(name, Mesh::createCylinderData(radius, height, sectors));
}

// ---------------------------------------------------------------------------
// Cone data.
// ---------------------------------------------------------------------------
MeshData Mesh::createConeData(float radius, float height, int sectors) {
    MeshData data;

    if (radius <= 0.0f) radius = 0.1f;
    if (height <= 0.0f) height = 0.1f;
    if (sectors < 3) sectors = 3;

    const int cols = sectors + 1;

    data.vertices.reserve((2 + cols) * kVertexStride);

    // Apex vertex.
    const int apexIdx = 0;
    appendVertex(data.vertices, Vec3(0.0f, height, 0.0f),
                 Vec3(0.0f, 1.0f, 0.0f), 0.5f, 1.0f);

    // Base center vertex.
    const int baseCenterIdx = 1;
    appendVertex(data.vertices, Vec3(0.0f, 0.0f, 0.0f),
                 Vec3(0.0f, -1.0f, 0.0f), 0.5f, 0.0f);

    // Base ring vertices.
    int baseRingStart = 2;
    for (int j = 0; j < cols; ++j) {
        const float angle = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sectors);
        const float x = radius * std::cos(angle);
        const float z = radius * std::sin(angle);
        const float u = static_cast<float>(j) / static_cast<float>(sectors);

        const float sideNormY = radius / std::sqrt(radius * radius + height * height);
        const float sideNormRad = height / std::sqrt(radius * radius + height * height);

        appendVertex(data.vertices, Vec3(x, 0.0f, z),
                     Vec3(std::cos(angle) * sideNormRad, sideNormY,
                          std::sin(angle) * sideNormRad),
                     u, 0.0f);
    }

    // Side faces: apex -> ring[j] -> ring[j+1].
    for (int j = 0; j < sectors; ++j) {
        data.indices.push_back(static_cast<unsigned int>(apexIdx));
        data.indices.push_back(static_cast<unsigned int>(baseRingStart + j));
        data.indices.push_back(static_cast<unsigned int>(baseRingStart + j + 1));
    }

    // Base cap: triangle fan around base center.
    for (int j = 0; j < sectors; ++j) {
        data.indices.push_back(static_cast<unsigned int>(baseCenterIdx));
        data.indices.push_back(static_cast<unsigned int>(baseRingStart + j + 1));
        data.indices.push_back(static_cast<unsigned int>(baseRingStart + j));
    }

    data.vertexCount = static_cast<int>(data.vertices.size() / kVertexStride);
    data.indexCount = static_cast<int>(data.indices.size());

    return data;
}

Mesh Mesh::createCone(const std::string& name, float radius, float height,
                      int sectors) {
    return Mesh::fromData(name, Mesh::createConeData(radius, height, sectors));
}

// ---------------------------------------------------------------------------
// Capsule data.
// ---------------------------------------------------------------------------
MeshData Mesh::createCapsuleData(float radius, float height, int sectors,
                                 int stacks) {
    MeshData data;

    if (radius <= 0.0f) radius = 0.1f;
    if (height < 0.0f) height = 0.0f;
    if (sectors < 3) sectors = 3;
    if (stacks < 2) stacks = 2;

    const float cylinderHeight = height;
    const float halfCyl = cylinderHeight * 0.5f;
    const int cols = sectors + 1;

    data.vertices.reserve((2 * stacks * cols + 2 * cols) * kVertexStride);

    // Helper: add a UV sphere hemisphere.
    auto addHemisphere = [&](bool top) {
        const int startRow = top ? 0 : 1;
        const int endRow = top ? stacks : stacks + 1;
        const float sign = top ? 1.0f : -1.0f;

        for (int i = startRow; i <= endRow; ++i) {
            const float phi = top ?
                (kPi * 0.5f * (1.0f - static_cast<float>(i) / static_cast<float>(stacks))) :
                (-kPi * 0.5f * (1.0f - static_cast<float>(i) / static_cast<float>(stacks)));
            const float sinPhi = std::sin(phi);
            const float cosPhi = std::cos(phi);

            const float y = sign * (halfCyl + cosPhi * radius);
            const float r = sinPhi * radius;
            const float v = top ?
                (static_cast<float>(i) / static_cast<float>(stacks)) :
                (1.0f + static_cast<float>(i) / static_cast<float>(stacks));

            for (int j = 0; j < cols; ++j) {
                const float angle = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sectors);
                const float x = r * std::cos(angle);
                const float z = r * std::sin(angle);
                const float u = static_cast<float>(j) / static_cast<float>(sectors);

                Vec3 normal(std::cos(angle) * sinPhi, cosPhi, std::sin(angle) * sinPhi);

                appendVertex(data.vertices, Vec3(x, y, z), normal, u, v);
            }
        }
    };

    // Bottom hemisphere.
    int bottomHemiBase = static_cast<int>(data.vertices.size()) / kVertexStride;
    addHemisphere(false);

    // Cylinder side: two rings at y = +/- halfCyl.
    int cylBottomRingBase = static_cast<int>(data.vertices.size()) / kVertexStride;
    for (int j = 0; j < cols; ++j) {
        const float angle = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sectors);
        const float x = radius * std::cos(angle);
        const float z = radius * std::sin(angle);
        const float u = static_cast<float>(j) / static_cast<float>(sectors);

        appendVertex(data.vertices, Vec3(x, -halfCyl, z),
                     Vec3(std::cos(angle), 0.0f, std::sin(angle)), u, 0.0f);
    }
    int cylTopRingBase = static_cast<int>(data.vertices.size()) / kVertexStride;
    for (int j = 0; j < cols; ++j) {
        const float angle = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sectors);
        const float x = radius * std::cos(angle);
        const float z = radius * std::sin(angle);
        const float u = static_cast<float>(j) / static_cast<float>(sectors);

        appendVertex(data.vertices, Vec3(x, halfCyl, z),
                     Vec3(std::cos(angle), 0.0f, std::sin(angle)), u, 1.0f);
    }

    // Top hemisphere.
    int topHemiBase = static_cast<int>(data.vertices.size()) / kVertexStride;
    addHemisphere(true);

    // Generate indices.
    // Bottom hemisphere: stacks rings.
    for (int i = 0; i < stacks; ++i) {
        const int rowStart = bottomHemiBase + i * cols;
        const int nextRowStart = rowStart + cols;
        for (int j = 0; j < sectors; ++j) {
            data.indices.push_back(static_cast<unsigned int>(rowStart + j));
            data.indices.push_back(static_cast<unsigned int>(nextRowStart + j));
            data.indices.push_back(static_cast<unsigned int>(rowStart + j + 1));

            data.indices.push_back(static_cast<unsigned int>(nextRowStart + j));
            data.indices.push_back(static_cast<unsigned int>(nextRowStart + j + 1));
            data.indices.push_back(static_cast<unsigned int>(rowStart + j + 1));
        }
    }

    // Cylinder side.
    for (int j = 0; j < sectors; ++j) {
        const int bl = cylBottomRingBase + j;
        const int br = cylBottomRingBase + j + 1;
        const int tl = cylTopRingBase + j;
        const int tr = cylTopRingBase + j + 1;

        data.indices.push_back(static_cast<unsigned int>(bl));
        data.indices.push_back(static_cast<unsigned int>(tl));
        data.indices.push_back(static_cast<unsigned int>(br));

        data.indices.push_back(static_cast<unsigned int>(tl));
        data.indices.push_back(static_cast<unsigned int>(tr));
        data.indices.push_back(static_cast<unsigned int>(br));
    }

    // Top hemisphere.
    for (int i = 0; i < stacks; ++i) {
        const int rowStart = topHemiBase + i * cols;
        const int nextRowStart = rowStart + cols;
        for (int j = 0; j < sectors; ++j) {
            data.indices.push_back(static_cast<unsigned int>(rowStart + j));
            data.indices.push_back(static_cast<unsigned int>(rowStart + j + 1));
            data.indices.push_back(static_cast<unsigned int>(nextRowStart + j));

            data.indices.push_back(static_cast<unsigned int>(nextRowStart + j));
            data.indices.push_back(static_cast<unsigned int>(rowStart + j + 1));
            data.indices.push_back(static_cast<unsigned int>(nextRowStart + j + 1));
        }
    }

    data.vertexCount = static_cast<int>(data.vertices.size()) / kVertexStride;
    data.indexCount = static_cast<int>(data.indices.size());

    return data;
}

Mesh Mesh::createCapsule(const std::string& name, float radius, float height,
                         int sectors, int stacks) {
    return Mesh::fromData(name, Mesh::createCapsuleData(radius, height, sectors, stacks));
}

MeshManager::~MeshManager() {
    releaseAll();
}

void MeshManager::registerMesh(Mesh mesh) {
    if (mesh.name.empty()) {
        return;
    }
    std::string name = mesh.name;
    auto it = m_meshes.find(name);
    if (it != m_meshes.end()) {
        it->second.release();
        it->second = std::move(mesh);
        return;
    }
    m_meshes[name] = std::move(mesh);
    m_names.push_back(std::move(name));
}

Mesh* MeshManager::find(const std::string& name) {
    auto it = m_meshes.find(name);
    if (it == m_meshes.end()) {
        return nullptr;
    }
    return &it->second;
}

const Mesh* MeshManager::find(const std::string& name) const {
    auto it = m_meshes.find(name);
    if (it == m_meshes.end()) {
        return nullptr;
    }
    return &it->second;
}

void MeshManager::releaseAll() {
    for (auto& pair : m_meshes) {
        pair.second.release();
    }
    m_meshes.clear();
}
