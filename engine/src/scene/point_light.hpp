#pragma once

#include "scene/node3d.hpp"

// A point light node. Stores position, colour, intensity and range. Stored
// settings only: lighting is not yet applied by the renderer.
class PointLight : public Node3D {
public:
    PointLight(NodeId id, std::string name);

    Vec3 color = Vec3(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    float range = 10.0f;

    void serializeProperties(std::ofstream& out) const override;
    bool deserializeProperties(std::ifstream& in) override;
};
