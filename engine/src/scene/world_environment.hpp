#pragma once

#include <string>

#include "math/vec3.hpp"
#include "scene/node.hpp"

// World environment node. Stores sky/fog settings for the scene. Unlike
// Node3D this node does not carry a transform.
class WorldEnvironment : public Node {
public:
    WorldEnvironment(NodeId id, std::string name);

    // Sky rendering mode: 0 = procedural gradient sky, 1 = textured skybox.
    int skyMode = 0;

    // Procedural sky colors.
    Vec3 skyColor = Vec3(0.4f, 0.6f, 0.85f);   // clear sky blue
    Vec3 horizonColor = Vec3(0.8f, 0.85f, 0.9f); // lighter at horizon
    float skyIntensity = 1.0f;                  // brightness multiplier

    // Environment rotation around world Y, in degrees.
    float environmentRotation = 0.0f;

    // Path to a skybox image (cubemap or equirectangular), used when
    // skyMode == 1. Relative to the assets directory.
    std::string skyboxPath;

    // Fog settings.
    Vec3 fogColor = Vec3(0.04f, 0.045f, 0.06f);
    float fogStart = 2.0f;
    float fogEnd = 15.0f;

    void serializeProperties(std::ofstream& out) const override;
    bool deserializeProperties(std::ifstream& in) override;
};
