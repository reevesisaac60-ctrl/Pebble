#pragma once

#include "scene/node3d.hpp"

// A directional light node. Stores direction, colour and intensity. Stored
// settings only: lighting is not yet applied by the renderer.
class DirectionalLight : public Node3D {
public:
    DirectionalLight(NodeId id, std::string name);

    Vec3 direction = Vec3(0.0f, 0.0f, -1.0f);
    Vec3 color = Vec3(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;

    void serializeProperties(std::ofstream& out) const override;
    bool deserializeProperties(std::ifstream& in) override;
};
