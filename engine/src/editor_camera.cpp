#include "editor_camera.hpp"

#include <cmath>
#include <algorithm>

namespace {

constexpr float kDegreesToRadians =
    3.14159265358979323846f / 180.0f;

} // namespace

EditorCamera::EditorCamera() = default;

Vec3 EditorCamera::position() const {
    return m_position;
}

Vec3 EditorCamera::forward() const {
    const float pitch = m_pitch * kDegreesToRadians;
    const float yaw = m_yaw * kDegreesToRadians;
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    // Right-handed, looking down -Z when yaw == 0 and pitch == 0.
    return Vec3(cp * sy, sp, -cp * cy).normalized();
}

Vec3 EditorCamera::right() const {
    return cross(forward(), Vec3(0.0f, 1.0f, 0.0f)).normalized();
}

Vec3 EditorCamera::up() const {
    return cross(right(), forward()).normalized();
}

Vec3 EditorCamera::horizontalForward() const {
    const float yaw = m_yaw * kDegreesToRadians;
    const float sy = std::sin(yaw);
    const float cy = std::cos(yaw);
    // Right-handed, looking down -Z when yaw == 0.
    return Vec3(sy, 0.0f, -cy).normalized();
}

Vec3 EditorCamera::horizontalRight() const {
    const Vec3 f = horizontalForward();
    return cross(f, Vec3(0.0f, 1.0f, 0.0f)).normalized();
}

Mat4 EditorCamera::viewMatrix() const {
    return ::lookAt(m_position, m_position + forward(), up());
}

Mat4 EditorCamera::projectionMatrix(float aspect) const {
    // A collapsed/invalid viewport can briefly report a non-positive or
    // non-finite aspect ratio while panels are being resized. Keep the
    // projection finite until a valid viewport size is available.
    if (!std::isfinite(aspect) || aspect <= 0.0f) {
        aspect = 1.0f;
    }
    return perspective(m_fieldOfView * kDegreesToRadians, aspect,
                       m_nearPlane, m_farPlane);
}

void EditorCamera::move(const Vec3& offset) {
    m_position += offset;
}

void EditorCamera::rotate(float yawDeltaDegrees, float pitchDeltaDegrees) {
    m_yaw += yawDeltaDegrees;
    m_pitch += pitchDeltaDegrees;
    if (m_pitch < kPitchMin) {
        m_pitch = kPitchMin;
    }
    if (m_pitch > kPitchMax) {
        m_pitch = kPitchMax;
    }
    // Keep yaw in a sane range to avoid floating point drift.
    while (m_yaw > 180.0f) {
        m_yaw -= 360.0f;
    }
    while (m_yaw < -180.0f) {
        m_yaw += 360.0f;
    }
}

void EditorCamera::lookAt(const Vec3& target, float distance) {
    const Vec3 f = forward();
    m_position = target - f * distance;
    // Re-derive yaw and pitch from the new forward direction so subsequent
    // rotations stay consistent.
    const Vec3 toTarget = (target - m_position).normalized();
    m_pitch = std::asin(std::clamp(toTarget.y, -1.0f, 1.0f)) / kDegreesToRadians;
    m_yaw = std::atan2(toTarget.x, -toTarget.z) / kDegreesToRadians;
    if (m_pitch < kPitchMin) {
        m_pitch = kPitchMin;
    }
    if (m_pitch > kPitchMax) {
        m_pitch = kPitchMax;
    }
}
