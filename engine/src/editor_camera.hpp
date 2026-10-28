#pragma once

#include "math/mat4.hpp"
#include "math/vec3.hpp"

// Editor-only camera used to navigate the viewport. It is entirely separate
// from scene Camera3D nodes: moving the editor camera never mutates a scene
// node, and scene cameras never drive editor navigation. Editor state is not
// serialized as part of the scene graph.
class EditorCamera {
public:
    EditorCamera();

    Vec3 position() const;
    float yaw() const { return m_yaw; }      // degrees, around Y axis
    float pitch() const { return m_pitch; }  // degrees, around X axis
    float movementSpeed() const { return m_movementSpeed; }
    void setMovementSpeed(float speed) { m_movementSpeed = speed; }

    // View matrix (world-to-camera), computed from position, yaw and pitch.
    Mat4 viewMatrix() const;

    // Perspective projection matrix for the given aspect ratio.
    Mat4 projectionMatrix(float aspect) const;

    // World-space forward/right/up directions derived from yaw and pitch.
    Vec3 forward() const;
    Vec3 right() const;
    Vec3 up() const;

    // Horizontal (pitch-free) forward and right vectors derived from yaw
    // alone. Used for W/S/A/D navigation so movement stays parallel to the
    // ground plane regardless of camera pitch.
    Vec3 horizontalForward() const;
    Vec3 horizontalRight() const;

    // Move the camera by a world-space offset (in camera-local units when
    // combined with forward()/right()/up()).
    void move(const Vec3& offset);

    // Rotate by yaw/pitch deltas in degrees. Pitch is clamped to avoid
    // gimbal lock.
    void rotate(float yawDeltaDegrees, float pitchDeltaDegrees);

    // Reposition the camera to look at the given world-space target from a
    // fixed distance behind its current forward direction.
    void lookAt(const Vec3& target, float distance = 5.0f);

private:
    Vec3 m_position = Vec3(0.0f, 0.0f, 3.0f);
    float m_yaw = 0.0f;          // degrees
    float m_pitch = 0.0f;        // degrees, clamped to [-89, 89]
    float m_movementSpeed = 5.0f; // world units per second
    float m_fieldOfView = 65.0f; // degrees
    float m_nearPlane = 0.1f;
    float m_farPlane = 100.0f;

    static constexpr float kPitchMin = -89.0f;
    static constexpr float kPitchMax = 89.0f;
    static constexpr float kDegreesToRadians =
        3.14159265358979323846f / 180.0f;
};
