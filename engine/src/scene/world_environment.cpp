#include "scene/world_environment.hpp"

#include <cstdint>
#include <fstream>

WorldEnvironment::WorldEnvironment(NodeId id, std::string name)
    : Node(id, std::move(name), "WorldEnvironment") {}

void WorldEnvironment::serializeProperties(std::ofstream& out) const {
    // Sky mode.
    int32_t skyMode32 = static_cast<int32_t>(skyMode);
    out.write(reinterpret_cast<const char*>(&skyMode32), sizeof(int32_t));

    // Sky color.
    out.write(reinterpret_cast<const char*>(&skyColor.x), sizeof(float));
    out.write(reinterpret_cast<const char*>(&skyColor.y), sizeof(float));
    out.write(reinterpret_cast<const char*>(&skyColor.z), sizeof(float));

    // Horizon color.
    out.write(reinterpret_cast<const char*>(&horizonColor.x), sizeof(float));
    out.write(reinterpret_cast<const char*>(&horizonColor.y), sizeof(float));
    out.write(reinterpret_cast<const char*>(&horizonColor.z), sizeof(float));

    // Sky intensity.
    out.write(reinterpret_cast<const char*>(&skyIntensity), sizeof(float));

    // Environment rotation.
    out.write(reinterpret_cast<const char*>(&environmentRotation), sizeof(float));

    // Skybox path: write uint32 length + data.
    {
        const uint32_t pathSize =
            static_cast<uint32_t>(skyboxPath.size());
        out.write(reinterpret_cast<const char*>(&pathSize), sizeof(uint32_t));
        if (pathSize > 0) {
            out.write(skyboxPath.data(),
                      static_cast<std::streamsize>(pathSize));
        }
    }

    // Fog color.
    out.write(reinterpret_cast<const char*>(&fogColor.x), sizeof(float));
    out.write(reinterpret_cast<const char*>(&fogColor.y), sizeof(float));
    out.write(reinterpret_cast<const char*>(&fogColor.z), sizeof(float));
    out.write(reinterpret_cast<const char*>(&fogStart), sizeof(float));
    out.write(reinterpret_cast<const char*>(&fogEnd), sizeof(float));
}

bool WorldEnvironment::deserializeProperties(std::ifstream& in) {
    // Sky mode.
    int32_t skyMode32 = 0;
    if (!in.read(reinterpret_cast<char*>(&skyMode32), sizeof(int32_t))) {
        return false;
    }
    skyMode = static_cast<int>(skyMode32);

    // Sky color.
    if (!in.read(reinterpret_cast<char*>(&skyColor.x), sizeof(float)) ||
        !in.read(reinterpret_cast<char*>(&skyColor.y), sizeof(float)) ||
        !in.read(reinterpret_cast<char*>(&skyColor.z), sizeof(float))) {
        return false;
    }

    // Horizon color.
    if (!in.read(reinterpret_cast<char*>(&horizonColor.x), sizeof(float)) ||
        !in.read(reinterpret_cast<char*>(&horizonColor.y), sizeof(float)) ||
        !in.read(reinterpret_cast<char*>(&horizonColor.z), sizeof(float))) {
        return false;
    }

    // Sky intensity.
    if (!in.read(reinterpret_cast<char*>(&skyIntensity), sizeof(float))) {
        return false;
    }

    // Environment rotation.
    if (!in.read(reinterpret_cast<char*>(&environmentRotation), sizeof(float))) {
        return false;
    }

    // Skybox path: read uint32 length + data.
    {
        uint32_t pathSize = 0;
        if (!in.read(reinterpret_cast<char*>(&pathSize), sizeof(uint32_t))) {
            return false;
        }
        if (pathSize > 10000) {
            return false;
        }
        skyboxPath.assign(pathSize, '\0');
        if (pathSize > 0) {
            in.read(&skyboxPath[0], static_cast<std::streamsize>(pathSize));
        }
    }

    // Fog color.
    if (!in.read(reinterpret_cast<char*>(&fogColor.x), sizeof(float)) ||
        !in.read(reinterpret_cast<char*>(&fogColor.y), sizeof(float)) ||
        !in.read(reinterpret_cast<char*>(&fogColor.z), sizeof(float))) {
        return false;
    }
    if (!in.read(reinterpret_cast<char*>(&fogStart), sizeof(float)) ||
        !in.read(reinterpret_cast<char*>(&fogEnd), sizeof(float))) {
        return false;
    }
    return true;
}
