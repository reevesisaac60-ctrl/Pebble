#include "scene/node3d.hpp"

#include <cstdio>

namespace {

constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.0f;

} // namespace

Node3D::Node3D(NodeId id, std::string name)
    : Node(id, std::move(name), "Node3D") {}

Mat4 Node3D::localTransform() const {
    Mat4 scaleMatrix;
    scaleMatrix.setIdentity();
    scaleMatrix(0, 0) = scale.x;
    scaleMatrix(1, 1) = scale.y;
    scaleMatrix(2, 2) = scale.z;

    const Mat4 translation = translate(position);
    const Mat4 rotateX = rotate(rotation.x * kDegreesToRadians, Vec3(1.0f, 0.0f, 0.0f));
    const Mat4 rotateY = rotate(rotation.y * kDegreesToRadians, Vec3(0.0f, 1.0f, 0.0f));
    const Mat4 rotateZ = rotate(rotation.z * kDegreesToRadians, Vec3(0.0f, 0.0f, 1.0f));

    return translation * rotateZ * rotateY * rotateX * scaleMatrix;
}

Mat4 Node3D::globalTransform() const {
    Mat4 result = localTransform();
    const Node3D* parent3d = dynamic_cast<const Node3D*>(m_parent);
    if (parent3d != nullptr) {
        result = parent3d->globalTransform() * result;
    }
    return result;
}

Vec3 Node3D::worldPosition() const {
    return transformPoint(globalTransform(), Vec3(0.0f, 0.0f, 0.0f));
}

void Node3D::serializeProperties(std::ofstream& out) const {
    auto writeFloat = [&out](float value) {
        out.write(reinterpret_cast<const char*>(&value), sizeof(float));
    };
    writeFloat(position.x);
    writeFloat(position.y);
    writeFloat(position.z);
    writeFloat(rotation.x);
    writeFloat(rotation.y);
    writeFloat(rotation.z);
    writeFloat(scale.x);
    writeFloat(scale.y);
    writeFloat(scale.z);
}

bool Node3D::deserializeProperties(std::ifstream& in) {
    auto readFloat = [&in](float& value) {
        in.read(reinterpret_cast<char*>(&value), sizeof(float));
        return in.good();
    };
    if (!readFloat(position.x) || !readFloat(position.y) || !readFloat(position.z)) {
        return false;
    }
    if (!readFloat(rotation.x) || !readFloat(rotation.y) || !readFloat(rotation.z)) {
        return false;
    }
    if (!readFloat(scale.x) || !readFloat(scale.y) || !readFloat(scale.z)) {
        return false;
    }
    return true;
}
