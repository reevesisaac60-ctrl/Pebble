#include "scene/point_light.hpp"

PointLight::PointLight(NodeId id, std::string name)
    : Node3D(id, std::move(name)) {
    m_type = "PointLight";
}

void PointLight::serializeProperties(std::ofstream& out) const {
    Node3D::serializeProperties(out);
    out.write(reinterpret_cast<const char*>(&color.x), sizeof(float));
    out.write(reinterpret_cast<const char*>(&color.y), sizeof(float));
    out.write(reinterpret_cast<const char*>(&color.z), sizeof(float));
    out.write(reinterpret_cast<const char*>(&intensity), sizeof(float));
    out.write(reinterpret_cast<const char*>(&range), sizeof(float));
}

bool PointLight::deserializeProperties(std::ifstream& in) {
    if (!Node3D::deserializeProperties(in)) {
        return false;
    }
    if (!in.read(reinterpret_cast<char*>(&color.x), sizeof(float)) ||
        !in.read(reinterpret_cast<char*>(&color.y), sizeof(float)) ||
        !in.read(reinterpret_cast<char*>(&color.z), sizeof(float))) {
        return false;
    }
    if (!in.read(reinterpret_cast<char*>(&intensity), sizeof(float)) ||
        !in.read(reinterpret_cast<char*>(&range), sizeof(float))) {
        return false;
    }
    return true;
}
