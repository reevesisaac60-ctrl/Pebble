#pragma once

#include <epoxy/gl.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "math/vec3.hpp"

// Raw mesh data (positions, normals, texture coordinates, indices) generated
// without any OpenGL context. Used for testing and for CPU-side geometry
// inspection.
struct MeshData {
    std::vector<float> vertices;   // interleaved: position(3) + normal(3) + texCoord(2)
    std::vector<unsigned int> indices;
    int vertexCount = 0;
    int indexCount = 0;
};

// A GPU-resident mesh: vertex positions, normals, texture coordinates and
// indices, stored in a VAO/VBO/EBO. Meshes are identified by name and shared
// through MeshManager so multiple Part nodes can reference the same geometry
// without duplicating GPU resources.
//
// Vertex layout (8 floats per vertex, stride = 8 * sizeof(float)):
//   location 0: aPosition  (3 floats)
//   location 1: aNormal    (3 floats)
//   location 2: aTexCoord  (2 floats)
struct Mesh {
    std::string name;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    int indexCount = 0;
    int vertexCount = 0;
    bool ready = false;

    Mesh() = default;
    explicit Mesh(std::string meshName) : name(std::move(meshName)) {}
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void release();

    // Bind the VAO for drawing. Caller must have the correct shader active.
    void bind() const;

    // Create a GPU mesh from raw data. Requires an OpenGL context.
    static Mesh fromData(const std::string& name, const MeshData& data);

    // Primitive data generators (no OpenGL context required).
    static MeshData createCubeData(const Vec3& size = Vec3(1.0f, 1.0f, 1.0f));
    static MeshData createSphereData(float radius = 1.0f,
                                     int sectors = 32, int stacks = 16);
    static MeshData createPlaneData(float width = 1.0f, float height = 1.0f,
                                    int segmentsX = 1, int segmentsY = 1);
    static MeshData createCylinderData(float radius = 1.0f,
                                       float height = 1.0f, int sectors = 32);
    static MeshData createConeData(float radius = 1.0f,
                                   float height = 1.0f, int sectors = 32);
    static MeshData createCapsuleData(float radius = 0.5f, float height = 1.0f,
                                      int sectors = 16, int stacks = 8);

    // Primitive generators that create GPU meshes (require OpenGL context).
    static Mesh createCube(const std::string& name,
                           const Vec3& size = Vec3(1.0f, 1.0f, 1.0f));
    static Mesh createSphere(const std::string& name, float radius = 1.0f,
                             int sectors = 32, int stacks = 16);
    static Mesh createPlane(const std::string& name, float width = 1.0f,
                            float height = 1.0f, int segmentsX = 1,
                            int segmentsY = 1);
    static Mesh createCylinder(const std::string& name, float radius = 1.0f,
                               float height = 1.0f, int sectors = 32);
    static Mesh createCone(const std::string& name, float radius = 1.0f,
                           float height = 1.0f, int sectors = 32);
    static Mesh createCapsule(const std::string& name, float radius = 0.5f,
                              float height = 1.0f, int sectors = 16,
                              int stacks = 8);
};

// Manages shared mesh resources by name. The renderer owns one instance.
class MeshManager {
public:
    MeshManager() = default;
    ~MeshManager();

    MeshManager(const MeshManager&) = delete;
    MeshManager& operator=(const MeshManager&) = delete;

    void registerMesh(Mesh mesh);
    Mesh* find(const std::string& name);
    const Mesh* find(const std::string& name) const;

    const std::vector<std::string>& names() const { return m_names; }

    void releaseAll();

private:
    std::unordered_map<std::string, Mesh> m_meshes;
    std::vector<std::string> m_names;
};
