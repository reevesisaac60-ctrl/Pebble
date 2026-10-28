#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "math/vec3.hpp"
#include "renderer/mesh.hpp"

// A minimal JSON value type sufficient for parsing glTF 2.0 documents.
// Supports objects, arrays, strings, numbers, booleans and null.
class JsonValue {
public:
    using Object = std::unordered_map<std::string, JsonValue>;
    using Array = std::vector<JsonValue>;

    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue() : m_type(Type::Null) {}
    explicit JsonValue(bool v) : m_type(Type::Bool), m_bool(v) {}
    explicit JsonValue(double v) : m_type(Type::Number), m_number(v) {}
    explicit JsonValue(std::string v)
        : m_type(Type::String), m_string(std::move(v)) {}
    explicit JsonValue(Array v) : m_type(Type::Array), m_array(std::move(v)) {}
    explicit JsonValue(Object v) : m_type(Type::Object), m_object(std::move(v)) {}

    Type type() const { return m_type; }
    bool isNull() const { return m_type == Type::Null; }
    bool isObject() const { return m_type == Type::Object; }
    bool isArray() const { return m_type == Type::Array; }
    bool isString() const { return m_type == Type::String; }
    bool isNumber() const { return m_type == Type::Number; }
    bool isBool() const { return m_type == Type::Bool; }

    bool asBool() const { return m_bool; }
    double asNumber() const { return m_number; }
    const std::string& asString() const { return m_string; }
    const Array& asArray() const { return m_array; }
    const Object& asObject() const { return m_object; }

    bool has(const std::string& key) const {
        return m_type == Type::Object && m_object.find(key) != m_object.end();
    }
    const JsonValue* find(const std::string& key) const {
        if (m_type != Type::Object) {
            return nullptr;
        }
        auto it = m_object.find(key);
        if (it == m_object.end()) {
            return nullptr;
        }
        return &it->second;
    }

private:
    Type m_type = Type::Null;
    bool m_bool = false;
    double m_number = 0.0;
    std::string m_string;
    Array m_array;
    Object m_object;
};

// Minimal recursive-descent JSON parser. Returns a JsonValue or an empty
// JsonValue (null type) with an error message on failure.
class JsonParser {
public:
    static JsonValue parse(const std::string& text, std::string* error = nullptr);

private:
    struct State {
        const char* pos;
        const char* end;
    };

    static JsonValue parseValue(State& s, std::string* error);
    static JsonValue parseObject(State& s, std::string* error);
    static JsonValue parseArray(State& s, std::string* error);
    static JsonValue parseString(State& s, std::string* error);
    static JsonValue parseNumber(State& s, std::string* error);
    static JsonValue parseLiteral(State& s, std::string* error);
    static void skipWhitespace(State& s);
    static bool peekChar(State& s, char c);
    static char nextChar(State& s);
};

// Result of importing a glTF file: one MeshData per mesh primitive, with a
// human-readable name. Callers upload each MeshData to the GPU via
// Mesh::fromData().
struct ImportedMesh {
    std::string name;
    MeshData data;
};

// A single glTF node with its local transform and mesh reference. Multiple
// nodes may reference the same mesh (instancing). parentIndex is the index
// into the nodes array (-1 for root nodes).
struct ImportedNode {
    std::string name;
    std::string meshName;  // empty when the node does not reference a mesh
    Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 rotation = Vec3(0.0f, 0.0f, 0.0f); // Euler angles, degrees
    Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
    int parentIndex = -1;
};

// ImportResult: either a list of imported meshes or an error message.
struct ImportResult {
    bool success = false;
    std::string error;
    std::vector<ImportedMesh> meshes;
    std::vector<ImportedNode> nodes; // glTF node hierarchy and transforms
};

// ModelImporter loads glTF 2.0 files (.glb binary or .gltf JSON + external
// resources) and extracts vertex data (positions, normals, texture
// coordinates, triangle indices) as CPU-side MeshData structures.
//
// The importer does not create OpenGL resources. It produces MeshData that
// callers can upload via Mesh::fromData(name, data).
//
// Coordinate system: glTF 2.0 uses a right-handed, Y-up coordinate system,
// which matches Pebble's convention. No axis swapping is performed. If a
// future Blender export uses Z-up, a conversion step would need to be added
// here (swap Y and Z, negate one axis to preserve handedness).
class ModelImporter {
public:
    ModelImporter() = default;
    ~ModelImporter() = default;

    ModelImporter(const ModelImporter&) = delete;
    ModelImporter& operator=(const ModelImporter&) = delete;

    // Import a glTF file from disk. Supports .glb (embedded binary buffer)
    // and .gltf (JSON referencing external .bin and image files). The
    // `baseDir` is used to resolve external resource paths relative to the
    // glTF file's location.
    ImportResult importFile(const std::string& filePath);

    // Import glTF data from a memory buffer (useful for embedded assets).
    // For .glb files, pass the entire .glb file bytes.
    ImportResult importFromMemory(const std::vector<uint8_t>& glbData);

private:
    // Per-buffer-view metadata used when resolving accessor data.
    struct BufferViewInfo {
        size_t bufferIndex = 0;
        size_t byteOffset = 0;
        size_t byteLength = 0;
    };

    ImportResult importGlb(const std::vector<uint8_t>& data,
                           const std::string& baseDir);
    ImportResult importGltfJson(const JsonValue& json,
                                const std::string& baseDir,
                                const std::vector<uint8_t>& binChunk = {});

    // Parse glTF nodes, scenes, and hierarchy into ImportedNode entries.
    // Returns the index of the default scene's first root node, or -1.
    bool importNodes(const JsonValue& gltfJson,
                     std::vector<ImportedNode>& outNodes) const;

    bool extractPrimitive(const JsonValue& primitiveJson,
                          const JsonValue& gltfJson,
                          const std::vector<std::vector<uint8_t>>& bufferData,
                          const std::vector<BufferViewInfo>& bufferViewInfos,
                          ImportedMesh& out);

    bool readAccessor(int accessorIndex, const JsonValue& gltfJson,
                      const std::vector<std::vector<uint8_t>>& bufferData,
                      const std::vector<BufferViewInfo>& bufferViewInfos,
                      std::vector<float>& out);

    bool readAccessorIndices(int accessorIndex, const JsonValue& gltfJson,
                             const std::vector<std::vector<uint8_t>>& bufferData,
                             const std::vector<BufferViewInfo>& bufferViewInfos,
                             std::vector<unsigned int>& out);

    std::string resolvePath(const std::string& path,
                            const std::string& baseDir) const;

    static std::string readFileBytes(const std::string& path,
                                     std::string* error);
};
