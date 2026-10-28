#include "scene/part.hpp"

#include <cstdint>
#include <fstream>
#include <string>

Part::Part(NodeId id, std::string name)
    : Node3D(id, std::move(name)) {
    m_type = "Part";
}

void Part::serializeProperties(std::ofstream& out) const {
    Node3D::serializeProperties(out);

    // Mesh name: uint32 length + bytes.
    const uint32_t meshNameSize =
        static_cast<uint32_t>(meshName.size());
    out.write(reinterpret_cast<const char*>(&meshNameSize), sizeof(uint32_t));
    if (meshNameSize > 0) {
        out.write(meshName.data(),
                  static_cast<std::streamsize>(meshNameSize));
    }

    // Material name: uint32 length + bytes.
    const uint32_t materialNameSize =
        static_cast<uint32_t>(materialName.size());
    out.write(reinterpret_cast<const char*>(&materialNameSize), sizeof(uint32_t));
    if (materialNameSize > 0) {
        out.write(materialName.data(),
                  static_cast<std::streamsize>(materialNameSize));
    }
}

bool Part::deserializeProperties(std::ifstream& in) {
    if (!Node3D::deserializeProperties(in)) {
        return false;
    }

    // Mesh name.
    {
        uint32_t meshNameSize = 0;
        if (!in.read(reinterpret_cast<char*>(&meshNameSize), sizeof(uint32_t))) {
            return false;
        }
        if (meshNameSize > 10000) {
            return false;
        }
        meshName.assign(meshNameSize, '\0');
        if (meshNameSize > 0) {
            in.read(&meshName[0], static_cast<std::streamsize>(meshNameSize));
        }
    }

    // Material name.
    {
        uint32_t materialNameSize = 0;
        if (!in.read(reinterpret_cast<char*>(&materialNameSize), sizeof(uint32_t))) {
            return false;
        }
        if (materialNameSize > 10000) {
            return false;
        }
        materialName.assign(materialNameSize, '\0');
        if (materialNameSize > 0) {
            in.read(&materialName[0], static_cast<std::streamsize>(materialNameSize));
        }
    }

    return in.good();
}
