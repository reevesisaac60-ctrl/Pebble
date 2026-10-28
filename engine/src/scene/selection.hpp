#ifndef PEBBLE_SCENE_SELECTION_HPP
#define PEBBLE_SCENE_SELECTION_HPP

#include <optional>
#include "scene/node.hpp"

class Selection {
public:
    Selection() = default;

    std::optional<NodeId> selected() const { return m_selected; }
    bool isSelected(NodeId id) const { return m_selected.has_value() && m_selected->value == id.value; }

    void select(NodeId id) { m_selected = id; }
    void clear() { m_selected.reset(); }

private:
    std::optional<NodeId> m_selected;
};

#endif // PEBBLE_SCENE_SELECTION_HPP
