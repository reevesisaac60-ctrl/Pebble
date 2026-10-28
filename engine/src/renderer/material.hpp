#pragma once

#include <epoxy/gl.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "math/vec3.hpp"

// A reusable material: base color, metallic, roughness, and an optional
// texture path. Materials are identified by name and shared through
// MaterialManager so multiple Part nodes can reference the same material
// without duplicating CPU or GPU state.
//
// Texture loading is deferred to the renderer/application; the material only
// stores the project-relative path. A missing or invalid texture falls back
// to an untextured (base-color-only) draw.
struct Material {
    std::string name;
    Vec3 baseColor = Vec3(0.8f, 0.8f, 0.8f);
    float metallic = 0.0f;
    float roughness = 0.5f;
    std::string texturePath; // relative to assets directory, empty = none
    bool hasTexture = false;

    Material() = default;
    explicit Material(std::string matName) : name(std::move(matName)) {}
};

// Manages shared material resources by name.
class MaterialManager {
public:
    MaterialManager() = default;
    ~MaterialManager() = default;

    MaterialManager(const MaterialManager&) = delete;
    MaterialManager& operator=(const MaterialManager&) = delete;

    void registerMaterial(Material material);
    Material* find(const std::string& name);
    const Material* find(const std::string& name) const;
    void clear();

    const std::vector<std::string>& names() const { return m_names; }

private:
    std::unordered_map<std::string, Material> m_materials;
    std::vector<std::string> m_names;
};
