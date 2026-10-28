#include "scene/scene.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "scene/camera3d.hpp"
#include "scene/directional_light.hpp"
#include "scene/node3d.hpp"
#include "scene/part.hpp"
#include "scene/point_light.hpp"
#include "scene/world_environment.hpp"

namespace {

constexpr char kSceneFileHeader[] = "PEBBLE_SCENE";

struct PendingParent {
    NodeId child;
    NodeId parent;
};

} // namespace

Scene::Scene(std::string name) : m_name(std::move(name)) {}

void Scene::setSerializedMaterials(
    const std::vector<SerializedMaterial>& materials) {
    m_serializedMaterials = materials;
}

Scene::~Scene() {
    clear();
}

NodeId Scene::allocateId() {
    NodeId id{m_nextId};
    ++m_nextId;
    return id;
}

NodeId Scene::createNode(std::string type, std::string name) {
    if (name.empty()) {
        name = type;
    }
    const NodeId id = allocateId();
    Node* node = createNodeByType(type, id, std::move(name));
    if (node == nullptr) {
        return NodeId{0};
    }
    m_nodes.push_back(std::unique_ptr<Node>(node));
    m_nodeById[id] = node;
    return id;
}

bool Scene::destroyNode(NodeId id) {
    Node* node = findImpl(id);
    if (node == nullptr) {
        return false;
    }
    destroySubtree(node);
    return true;
}

void Scene::destroySubtree(Node* node) {
    // Destroy children first (cascade), then the node itself.
    while (!node->m_children.empty()) {
        NodeId childId = node->m_children.back();
        Node* child = findImpl(childId);
        if (child != nullptr) {
            destroySubtree(child);
        } else {
            node->m_children.pop_back();
        }
    }

    detachFromParent(node);

    m_nodeById.erase(node->id());
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (it->get() == node) {
            m_nodes.erase(it);
            break;
        }
    }
}

void Scene::detachFromParent(Node* node) {
    if (node->m_parent != nullptr) {
        node->m_parent->removeChild(node->id());
        node->m_parent = nullptr;
    }
}

bool Scene::has(NodeId id) const {
    return findImpl(id) != nullptr;
}

const Camera3D* Scene::activeCamera() const {
    for (const auto& node : m_nodes) {
        if (const Camera3D* camera = dynamic_cast<const Camera3D*>(node.get())) {
            return camera;
        }
    }
    return nullptr;
}

Node* Scene::find(NodeId id) {
    return findImpl(id);
}

const Node* Scene::find(NodeId id) const {
    return const_cast<Scene*>(this)->findImpl(id);
}

Node* Scene::findImpl(NodeId id) {
    auto it = m_nodeById.find(id);
    if (it == m_nodeById.end()) {
        return nullptr;
    }
    return it->second;
}

const Node* Scene::findImpl(NodeId id) const {
    return const_cast<Scene*>(this)->findImpl(id);
}

std::vector<NodeId> Scene::nodeIds() const {
    std::vector<NodeId> ids;
    ids.reserve(m_nodes.size());
    for (const auto& node : m_nodes) {
        ids.push_back(node->id());
    }
    return ids;
}

std::vector<NodeId> Scene::rootNodeIds() const {
    std::vector<NodeId> ids;
    for (const auto& node : m_nodes) {
        if (!node->hasParent()) {
            ids.push_back(node->id());
        }
    }
    return ids;
}

bool Scene::wouldCreateCycle(NodeId childId, NodeId parentId) const {
    if (childId.value == 0 || parentId.value == 0) {
        return false;
    }
    if (childId == parentId) {
        return true;
    }
    const Node* parent = findImpl(parentId);
    if (parent == nullptr) {
        return false;
    }
    return parent->isAncestorOf(childId);
}

bool Scene::setParent(NodeId childId, NodeId parentId) {
    if (childId.value == 0) {
        return false;
    }
    Node* child = findImpl(childId);
    if (child == nullptr) {
        return false;
    }

    if (parentId.value == 0) {
        if (child->m_parent == nullptr) {
            return true;
        }
        detachFromParent(child);
        return true;
    }

    Node* parent = findImpl(parentId);
    if (parent == nullptr) {
        return false;
    }
    if (child == parent) {
        return false;
    }
    if (wouldCreateCycle(childId, parentId)) {
        return false;
    }
    if (child->m_parent == parent) {
        return true;
    }

    detachFromParent(child);
    child->setParent(parent);
    parent->addChild(childId);
    return true;
}

void Scene::clear() {
    m_nodes.clear();
    m_nodeById.clear();
    m_nextId = 1;
}

bool Scene::writeString(std::ofstream& out, const std::string& s) {
    const uint32_t size = static_cast<uint32_t>(s.size());
    if (!writeUint32(out, size)) {
        return false;
    }
    if (size > 0) {
        out.write(s.data(), static_cast<std::streamsize>(size));
    }
    return out.good();
}

bool Scene::readString(std::ifstream& in, std::string& s) {
    uint32_t size = 0;
    if (!readUint32(in, size)) {
        return false;
    }
    if (size > kMaxStringSize) {
        return false;
    }
    s.assign(size, '\0');
    if (size > 0) {
        in.read(&s[0], static_cast<std::streamsize>(size));
    }
    return in.good();
}

bool Scene::writeUint64(std::ofstream& out, uint64_t value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
    return out.good();
}

bool Scene::readUint64(std::ifstream& in, uint64_t& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(uint64_t));
    return in.good();
}

bool Scene::writeUint32(std::ofstream& out, uint32_t value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(uint32_t));
    return out.good();
}

bool Scene::readUint32(std::ifstream& in, uint32_t& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(uint32_t));
    return in.good();
}

bool Scene::writeFloat(std::ofstream& out, float value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(float));
    return out.good();
}

bool Scene::readFloat(std::ifstream& in, float& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(float));
    return in.good();
}

bool Scene::save(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    out.write(kSceneFileHeader, sizeof(kSceneFileHeader) - 1);
    out.put('\0');
    if (!writeUint32(out, kFormatVersion)) {
        return false;
    }
    if (!writeString(out, m_name)) {
        return false;
    }
    return saveV3(out);
}

bool Scene::saveV2(std::ofstream& out) const {
    const std::vector<NodeId> ids = nodeIds();
    if (!writeUint32(out, static_cast<uint32_t>(ids.size()))) {
        return false;
    }
    for (const NodeId id : ids) {
        const Node* node = findImpl(id);
        if (node == nullptr) {
            continue;
        }
        if (!writeUint64(out, node->id().value)) {
            return false;
        }
        if (!writeString(out, node->type())) {
            return false;
        }
        if (!writeString(out, node->name())) {
            return false;
        }
        const uint64_t parentId = node->hasParent() ? node->parent()->id().value : 0;
        if (!writeUint64(out, parentId)) {
            return false;
        }
        node->serializeProperties(out);
        if (!out.good()) {
            return false;
        }
    }
    return true;
}

bool Scene::saveV3(std::ofstream& out) const {
    // Nodes (same as v2).
    const std::vector<NodeId> ids = nodeIds();
    if (!writeUint32(out, static_cast<uint32_t>(ids.size()))) {
        return false;
    }
    for (const NodeId id : ids) {
        const Node* node = findImpl(id);
        if (node == nullptr) {
            continue;
        }
        if (!writeUint64(out, node->id().value)) {
            return false;
        }
        if (!writeString(out, node->type())) {
            return false;
        }
        if (!writeString(out, node->name())) {
            return false;
        }
        const uint64_t parentId = node->hasParent() ? node->parent()->id().value : 0;
        if (!writeUint64(out, parentId)) {
            return false;
        }
        node->serializeProperties(out);
        if (!out.good()) {
            return false;
        }
    }

    // Materials section (new in v3).
    if (!writeUint32(out,
                     static_cast<uint32_t>(m_serializedMaterials.size()))) {
        return false;
    }
    for (const auto& mat : m_serializedMaterials) {
        if (!writeString(out, mat.name)) {
            return false;
        }
        if (!writeFloat(out, mat.baseColor[0]) ||
            !writeFloat(out, mat.baseColor[1]) ||
            !writeFloat(out, mat.baseColor[2])) {
            return false;
        }
        if (!writeFloat(out, mat.metallic) ||
            !writeFloat(out, mat.roughness)) {
            return false;
        }
        const uint8_t hasTexture = mat.hasTexture ? 1 : 0;
        out.write(reinterpret_cast<const char*>(&hasTexture), sizeof(uint8_t));
        if (!writeString(out, mat.texturePath)) {
            return false;
        }
        if (!out.good()) {
            return false;
        }
    }
    return true;
}

bool Scene::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    char header[32] = {0};
    in.getline(header, 32, '\0');
    if (std::string(header) != kSceneFileHeader) {
        std::fprintf(stderr, "Scene: bad file header in '%s'\n", path.c_str());
        return false;
    }

    uint32_t version = 0;
    if (!readUint32(in, version)) {
        std::fprintf(stderr, "Scene: failed to read format version\n");
        return false;
    }

    clear();

    if (!readString(in, m_name)) {
        std::fprintf(stderr, "Scene: failed to read scene name\n");
        return false;
    }

    if (version == 1) {
        return loadV1(in);
    }
    if (version == 2) {
        return loadV2(in);
    }
    if (version == kFormatVersion) {
        return loadV3(in);
    }
    std::fprintf(stderr, "Scene: unsupported format version %u\n", version);
    return false;
}

bool Scene::loadV2(std::ifstream& in) {
    uint32_t nodeCount = 0;
    if (!readUint32(in, nodeCount)) {
        return false;
    }
    if (nodeCount > kMaxNodes) {
        std::fprintf(stderr, "Scene: unreasonable node count %u\n", nodeCount);
        return false;
    }

    // First pass: create all nodes and read their properties without
    // parenting. This validates ids, types and property blocks before the
    // scene is mutated.
    std::vector<PendingParent> pending;
    pending.reserve(nodeCount);
    for (uint32_t i = 0; i < nodeCount; ++i) {
        uint64_t idValue = 0;
        if (!readUint64(in, idValue)) {
            return false;
        }
        std::string type;
        if (!readString(in, type)) {
            return false;
        }
        std::string name;
        if (!readString(in, name)) {
            return false;
        }
        uint64_t parentValue = 0;
        if (!readUint64(in, parentValue)) {
            return false;
        }

        if (idValue == 0) {
            std::fprintf(stderr, "Scene: null node id encountered\n");
            return false;
        }
        if (has(NodeId{idValue})) {
            std::fprintf(stderr, "Scene: duplicate node id %llu\n",
                         static_cast<unsigned long long>(idValue));
            return false;
        }

        Node* node = createNodeByType(type, NodeId{idValue}, std::move(name));
        if (node == nullptr) {
            std::fprintf(stderr, "Scene: unsupported node type '%s'\n", type.c_str());
            return false;
        }
        if (!node->deserializeProperties(in)) {
            std::fprintf(stderr, "Scene: failed to deserialize properties for node %llu\n",
                         static_cast<unsigned long long>(idValue));
            return false;
        }

        m_nodes.push_back(std::unique_ptr<Node>(node));
        m_nodeById[NodeId{idValue}] = node;
        pending.push_back({NodeId{idValue}, NodeId{parentValue}});
    }

    // Second pass: apply parenting with cycle detection.
    for (const PendingParent& pp : pending) {
        if (pp.parent.value != 0) {
            if (!setParent(pp.child, pp.parent)) {
                std::fprintf(stderr, "Scene: invalid parent reference for node %llu\n",
                             static_cast<unsigned long long>(pp.child.value));
                return false;
            }
        }
    }
    return true;
}

bool Scene::loadV3(std::ifstream& in) {
    // Read materials first (v3 writes them before nodes... actually after,
    // but we read them first so they're available when nodes reference them).
    // v3 writes: nodes, then materials. We need to read nodes first to know
    // which materials are referenced, but materials are written after nodes.
    // So we read all nodes first (without materials), then read materials.
    // Actually, v3 writes materials AFTER nodes, so we read nodes first,
    // then materials.

    // First pass: read all nodes.
    uint32_t nodeCount = 0;
    if (!readUint32(in, nodeCount)) {
        return false;
    }
    if (nodeCount > kMaxNodes) {
        std::fprintf(stderr, "Scene: unreasonable node count %u\n", nodeCount);
        return false;
    }

    std::vector<PendingParent> pending;
    pending.reserve(nodeCount);
    for (uint32_t i = 0; i < nodeCount; ++i) {
        uint64_t idValue = 0;
        if (!readUint64(in, idValue)) {
            return false;
        }
        std::string type;
        if (!readString(in, type)) {
            return false;
        }
        std::string name;
        if (!readString(in, name)) {
            return false;
        }
        uint64_t parentValue = 0;
        if (!readUint64(in, parentValue)) {
            return false;
        }

        if (idValue == 0) {
            std::fprintf(stderr, "Scene: null node id encountered\n");
            return false;
        }
        if (has(NodeId{idValue})) {
            std::fprintf(stderr, "Scene: duplicate node id %llu\n",
                         static_cast<unsigned long long>(idValue));
            return false;
        }

        Node* node = createNodeByType(type, NodeId{idValue}, std::move(name));
        if (node == nullptr) {
            std::fprintf(stderr, "Scene: unsupported node type '%s\n",
                         type.c_str());
            return false;
        }
        if (!node->deserializeProperties(in)) {
            std::fprintf(stderr, "Scene: failed to deserialize properties for node %llu\n",
                         static_cast<unsigned long long>(idValue));
            return false;
        }

        m_nodes.push_back(std::unique_ptr<Node>(node));
        m_nodeById[NodeId{idValue}] = node;
        pending.push_back({NodeId{idValue}, NodeId{parentValue}});
    }

    // Second pass: read materials.
    uint32_t materialCount = 0;
    if (!readUint32(in, materialCount)) {
        return false;
    }
    if (materialCount > kMaxNodes) {
        std::fprintf(stderr, "Scene: unreasonable material count %u\n",
                     materialCount);
        return false;
    }
    m_serializedMaterials.clear();
    m_serializedMaterials.reserve(materialCount);
    for (uint32_t i = 0; i < materialCount; ++i) {
        SerializedMaterial mat;
        if (!readString(in, mat.name)) {
            return false;
        }
        if (!readFloat(in, mat.baseColor[0]) ||
            !readFloat(in, mat.baseColor[1]) ||
            !readFloat(in, mat.baseColor[2])) {
            return false;
        }
        if (!readFloat(in, mat.metallic) ||
            !readFloat(in, mat.roughness)) {
            return false;
        }
        uint8_t hasTexture = 0;
        if (!in.read(reinterpret_cast<char*>(&hasTexture), sizeof(uint8_t))) {
            return false;
        }
        mat.hasTexture = (hasTexture != 0);
        if (!readString(in, mat.texturePath)) {
            return false;
        }
        m_serializedMaterials.push_back(std::move(mat));
    }

    // Third pass: apply parenting with cycle detection.
    for (const PendingParent& pp : pending) {
        if (pp.parent.value != 0) {
            if (!setParent(pp.child, pp.parent)) {
                std::fprintf(stderr,
                             "Scene: invalid parent reference for node %llu\n",
                             static_cast<unsigned long long>(pp.child.value));
                return false;
            }
        }
    }
    return true;
}

bool Scene::loadV1(std::ifstream& in) {
    // Legacy flat format: entities with a name and transform, no type or
    // hierarchy. Migrates each entity to a Part node with no parent.
    uint32_t entityCount = 0;
    if (!readUint32(in, entityCount)) {
        return false;
    }
    if (entityCount > kMaxNodes) {
        std::fprintf(stderr, "Scene: unreasonable entity count %u\n", entityCount);
        return false;
    }
    for (uint32_t i = 0; i < entityCount; ++i) {
        uint64_t idValue = 0;
        if (!readUint64(in, idValue)) {
            return false;
        }
        std::string name;
        if (!readString(in, name)) {
            return false;
        }
        if (idValue == 0 || has(NodeId{idValue})) {
            std::fprintf(stderr, "Scene: invalid or duplicate legacy id %llu\n",
                         static_cast<unsigned long long>(idValue));
            return false;
        }

        // Legacy v1 entities carry no type or hierarchy. Migrate each to a Part
        // node (they were rendered as cubes in the legacy renderer) with no
        // parent. Only the transform is present; there is no type-specific
        // property block, so read the transform directly rather than calling
        // deserializeProperties (which would also read a Part primitive field
        // that v1 does not contain).
        Node* node = createNodeByType("Part", NodeId{idValue}, std::move(name));
        if (node == nullptr) {
            return false;
        }
        Node3D* node3d = dynamic_cast<Node3D*>(node);
        if (node3d == nullptr) {
            return false;
        }
        if (!readFloat(in, node3d->position.x) ||
            !readFloat(in, node3d->position.y) ||
            !readFloat(in, node3d->position.z) ||
            !readFloat(in, node3d->rotation.x) ||
            !readFloat(in, node3d->rotation.y) ||
            !readFloat(in, node3d->rotation.z) ||
            !readFloat(in, node3d->scale.x) ||
            !readFloat(in, node3d->scale.y) ||
            !readFloat(in, node3d->scale.z)) {
            return false;
        }

        m_nodes.push_back(std::unique_ptr<Node>(node));
        m_nodeById[NodeId{idValue}] = node;
        if (idValue + 1 > m_nextId) {
            m_nextId = idValue + 1;
        }
    }
    return true;
}

Node* Scene::createNodeByType(const std::string& type, NodeId id, std::string name) {
    if (type == "Node3D") {
        return new Node3D(id, std::move(name));
    }
    if (type == "Part") {
        return new Part(id, std::move(name));
    }
    if (type == "Camera3D") {
        return new Camera3D(id, std::move(name));
    }
    if (type == "DirectionalLight") {
        return new DirectionalLight(id, std::move(name));
    }
    if (type == "PointLight") {
        return new PointLight(id, std::move(name));
    }
    if (type == "WorldEnvironment") {
        return new WorldEnvironment(id, std::move(name));
    }
    return nullptr;
}
