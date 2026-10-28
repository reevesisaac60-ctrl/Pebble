#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "scene/node.hpp"

// Simple POD container for material data used during scene serialization.
// Kept separate from renderer::Material to avoid a scene-to-renderer
// dependency.
struct SerializedMaterial {
    std::string name;
    float baseColor[3] = {0.8f, 0.8f, 0.8f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    bool hasTexture = false;
    std::string texturePath;
};

class Camera3D;

// The scene owns every node in the graph. Parent/child relationships are
// stored as NodeId references plus non-owning raw parent pointers on each
// node; the Scene keeps them consistent on creation, reparenting and
// destruction.
class Scene {
public:
    explicit Scene(std::string name = "Untitled");
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = default;
    Scene& operator=(Scene&&) = default;

    const std::string& name() const { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

    // Create a node of the given type ("Node3D", "Part", "Camera3D",
    // "DirectionalLight", "PointLight", "WorldEnvironment"). Returns a null
    // id ({0}) if the type is unknown.
    NodeId createNode(std::string type, std::string name = "");

    // Destroy a node and, recursively, its entire subtree. Returns false if
    // the node does not exist.
    bool destroyNode(NodeId id);

    bool has(NodeId id) const;
    Node* find(NodeId id);
    const Node* find(NodeId id) const;

    // Returns the first Camera3D node in the scene, or nullptr when no camera
    // exists. Used by the renderer to pick the active game camera.
    const Camera3D* activeCamera() const;

    std::vector<NodeId> nodeIds() const;
    // Nodes with no parent (the visible roots of the hierarchy).
    std::vector<NodeId> rootNodeIds() const;

    // Reparent childId under parentId. Passing a null id ({0}) reparents to
    // the scene root. Returns false on invalid ids, a self-parent, or a
    // cycle.
    bool setParent(NodeId childId, NodeId parentId);

    bool empty() const { return m_nodes.empty(); }
    void clear();

    // Serialization. Format version 3 supports materials; version 2 (no
    // materials) and version 1 (legacy flat entities) are migrated on load.
    bool save(const std::string& path) const;
    bool load(const std::string& path);

private:
    static constexpr uint32_t kFormatVersion = 3;
    static constexpr uint32_t kMaxNodes = 100000;
    static constexpr uint32_t kMaxStringSize = 10000;

    std::string m_name;
    std::vector<std::unique_ptr<Node>> m_nodes;
    std::unordered_map<NodeId, Node*> m_nodeById;
    uint64_t m_nextId = 1;

    // Materials collected for serialization (set by the caller before save,
    // populated by load).
    std::vector<SerializedMaterial> m_serializedMaterials;

    Node* findImpl(NodeId id);
    const Node* findImpl(NodeId id) const;
    NodeId allocateId();

    void detachFromParent(Node* node);
    void destroySubtree(Node* node);
    bool wouldCreateCycle(NodeId childId, NodeId parentId) const;

    // Serialization helpers
    static bool writeString(std::ofstream& out, const std::string& s);
    static bool readString(std::ifstream& in, std::string& s);
    static bool writeUint64(std::ofstream& out, uint64_t value);
    static bool readUint64(std::ifstream& in, uint64_t& value);
    static bool writeUint32(std::ofstream& out, uint32_t value);
    static bool readUint32(std::ifstream& in, uint32_t& value);
    static bool writeFloat(std::ofstream& out, float value);
    static bool readFloat(std::ifstream& in, float& value);

    bool saveV2(std::ofstream& out) const;
    bool saveV3(std::ofstream& out) const;
    bool loadV2(std::ifstream& in);
    bool loadV3(std::ifstream& in);
    bool loadV1(std::ifstream& in);  // legacy flat format, migrated to v2
    Node* createNodeByType(const std::string& type, NodeId id, std::string name);

public:
    // Materials for serialization. Call setSerializedMaterials() before
    // save() to persist material properties; call getSerializedMaterials()
    // after load() to retrieve them.
    void setSerializedMaterials(const std::vector<SerializedMaterial>& materials);
    const std::vector<SerializedMaterial>& getSerializedMaterials() const {
        return m_serializedMaterials;
    }
    std::vector<SerializedMaterial>& getSerializedMaterials() {
        return m_serializedMaterials;
    }
};
