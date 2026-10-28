#pragma once

#include "scene/node3d.hpp"

// A camera node used to render a game scene. Its field of view, near plane
// and far plane are operational: they drive the renderer's projection matrix.
// The node's global transform drives the view matrix.
//
// A scene camera is distinct from the editor camera: the editor camera is
// used for viewport navigation and never mutates a scene camera node.
class Camera3D : public Node3D {
public:
    Camera3D(NodeId id, std::string name);

    float fieldOfView = 45.0f;   // degrees
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    // Returns true when the projection parameters are valid for rendering:
    // field of view in (0, 180), near > 0, far > near.
    bool isValid() const;

    // View matrix derived from this node's global transform (inverse of the
    // camera's world transform).
    Mat4 viewMatrix() const;

    // Projection matrix for the given aspect ratio. Returns an identity
    // matrix when the parameters are invalid.
    Mat4 projectionMatrix(float aspect) const;

    void serializeProperties(std::ofstream& out) const override;
    bool deserializeProperties(std::ifstream& in) override;
};
