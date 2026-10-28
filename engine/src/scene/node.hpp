#pragma once

#include <cstdint>
#include <functional>
#include <fstream>
#include <string>
#include <vector>

struct NodeId {
    uint64_t value = 0;

    bool operator==(const NodeId& other) const { return value == other.value; }
    bool operator!=(const NodeId& other) const { return value != other.value; }
    bool operator<(const NodeId& other) const { return value < other.value; }
};


// Base class for every object in the scene graph. A Node has a stable unique
// id, an editable name, a type identifier, and parent/child relationships.
// Ownership of nodes belongs to the Scene; a Node only holds non-owning
// references to its parent and children.
class Node {
public:
    Node(NodeId id, std::string name, std::string type);
    virtual ~Node();

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    NodeId id() const { return m_id; }
    const std::string& name() const { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }
    const std::string& type() const { return m_type; }

    bool hasParent() const { return m_parent != nullptr; }
    class Node* parent() const { return m_parent; }
    const std::vector<NodeId>& children() const { return m_children; }
    bool hasChildren() const { return !m_children.empty(); }

    // Returns true if this node is an ancestor (direct or indirect) of the
    // given descendant node id. Used to detect cycles when reparenting.
    bool isAncestorOf(NodeId descendant) const;

    // Type-specific properties. The base class writes/reads nothing here;
    // derived classes override to persist their own fields.
    virtual void serializeProperties(std::ofstream& out) const {
        (void)out;
    }
    virtual bool deserializeProperties(std::ifstream& in) {
        (void)in;
        return true;
    }

protected:
    friend class Scene;

    // Reparenting is managed exclusively by the Scene so that parent and
    // child bookkeeping stay consistent.
    void setParent(Node* parent);
    void addChild(NodeId childId);
    void removeChild(NodeId childId);

    NodeId m_id;
    std::string m_name;
    std::string m_type;
    Node* m_parent = nullptr;
    std::vector<NodeId> m_children;
};

namespace std {

template <>
struct hash<NodeId> {
    size_t operator()(const NodeId& id) const noexcept {
        return static_cast<size_t>(id.value);
    }
};

} // namespace std
