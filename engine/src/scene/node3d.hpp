#pragma once

#include "math/mat4.hpp"
#include "math/vec3.hpp"
#include "scene/node.hpp"

// A transform-bearing node: position, rotation (Euler degrees) and scale are
// stored relative to its parent. The global transform is composed by walking
// the parent chain, so moving a parent moves all of its descendants.
class Node3D : public Node {
public:
    Node3D(NodeId id, std::string name);

    Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 rotation = Vec3(0.0f, 0.0f, 0.0f); // Euler angles, degrees
    Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);

    // Local (parent-relative) model matrix.
    Mat4 localTransform() const;

    // Global (world) model matrix, composed with all Node3D ancestors. Plain
    // Node ancestors contribute no transform.
    Mat4 globalTransform() const;

    // World-space position of this node's origin.
    Vec3 worldPosition() const;

    void serializeProperties(std::ofstream& out) const override;
    bool deserializeProperties(std::ifstream& in) override;
};
