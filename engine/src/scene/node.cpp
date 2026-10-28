#include "scene/node.hpp"

#include <algorithm>

Node::Node(NodeId id, std::string name, std::string type)
    : m_id(id), m_name(std::move(name)), m_type(std::move(type)) {}

Node::~Node() = default;

bool Node::isAncestorOf(NodeId descendant) const {
    const Node* current = m_parent;
    while (current != nullptr) {
        if (current->id() == descendant) {
            return true;
        }
        current = current->m_parent;
    }
    return false;
}

void Node::setParent(Node* parent) {
    m_parent = parent;
}

void Node::addChild(NodeId childId) {
    m_children.push_back(childId);
}

void Node::removeChild(NodeId childId) {
    m_children.erase(
        std::remove(m_children.begin(), m_children.end(), childId),
        m_children.end());
}
