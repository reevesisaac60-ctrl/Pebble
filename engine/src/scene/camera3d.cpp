#include "scene/camera3d.hpp"

#include <cmath>
#include <cstdio>

Camera3D::Camera3D(NodeId id, std::string name)
    : Node3D(id, std::move(name)) {
    m_type = "Camera3D";
}

bool Camera3D::isValid() const {
    if (fieldOfView <= 0.0f || fieldOfView >= 180.0f) {
        return false;
    }
    if (nearPlane <= 0.0f) {
        return false;
    }
    if (farPlane <= nearPlane) {
        return false;
    }
    return true;
}

Mat4 Camera3D::viewMatrix() const {
    const Mat4 world = globalTransform();
    // Camera position is the translation column of the world transform.
    const Vec3 eye(world(0, 3), world(1, 3), world(2, 3));
    // The camera looks down its local -Z axis; column 2 of the column-major
    // world matrix holds the local +Z axis, so forward is its negation.
    const Vec3 forward = Vec3(-world(0, 2), -world(1, 2), -world(2, 2)).normalized();
    // Column 1 holds the local +Y axis, used as the camera up vector.
    const Vec3 cameraUp = Vec3(world(0, 1), world(1, 1), world(2, 1)).normalized();

    if (forward.lengthSquared() == 0.0f) {
        return lookAt(eye, eye + Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, 1.0f, 0.0f));
    }
    return lookAt(eye, eye + forward, cameraUp);
}

Mat4 Camera3D::projectionMatrix(float aspect) const {
    if (!isValid()) {
        return Mat4(); // identity: caller detects invalid via isValid()
    }
    return perspective(fieldOfView * 3.14159265358979323846f / 180.0f, aspect,
                       nearPlane, farPlane);
}

void Camera3D::serializeProperties(std::ofstream& out) const {
    Node3D::serializeProperties(out);
    out.write(reinterpret_cast<const char*>(&fieldOfView), sizeof(float));
    out.write(reinterpret_cast<const char*>(&nearPlane), sizeof(float));
    out.write(reinterpret_cast<const char*>(&farPlane), sizeof(float));
}

bool Camera3D::deserializeProperties(std::ifstream& in) {
    if (!Node3D::deserializeProperties(in)) {
        return false;
    }
    if (!in.read(reinterpret_cast<char*>(&fieldOfView), sizeof(float))) {
        return false;
    }
    if (!in.read(reinterpret_cast<char*>(&nearPlane), sizeof(float))) {
        return false;
    }
    if (!in.read(reinterpret_cast<char*>(&farPlane), sizeof(float))) {
        return false;
    }
    return true;
}
