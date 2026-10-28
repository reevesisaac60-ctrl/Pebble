#pragma once

#include <string>

#include "renderer/material.hpp"
#include "renderer/mesh.hpp"
#include "scene/node3d.hpp"

// A renderable primitive node. Part nodes are the objects the renderer draws
// using their global transform. Each Part references a named mesh (geometry)
// and a named material (surface properties), both looked up through the
// renderer's resource managers at render time. This allows multiple parts to
// share the same GPU mesh and material without duplication.
class Part : public Node3D {
public:
    Part(NodeId id, std::string name);

    // Name of the mesh resource (looked up in MeshManager). Defaults to
    // "cube" so existing behavior is preserved.
    std::string meshName = "cube";

    // Name of the material resource (looked up in MaterialManager). Defaults
    // to "default" so existing behavior is preserved.
    std::string materialName = "default";

    void serializeProperties(std::ofstream& out) const override;
    bool deserializeProperties(std::ifstream& in) override;
};
