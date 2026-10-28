#include "renderer/material.hpp"

void MaterialManager::registerMaterial(Material material) {
    if (material.name.empty()) {
        return;
    }
    std::string name = material.name;
    auto it = m_materials.find(name);
    if (it != m_materials.end()) {
        it->second = std::move(material);
        return;
    }
    m_materials[name] = std::move(material);
    m_names.push_back(std::move(name));
}

Material* MaterialManager::find(const std::string& name) {
    auto it = m_materials.find(name);
    if (it == m_materials.end()) {
        return nullptr;
    }
    return &it->second;
}

const Material* MaterialManager::find(const std::string& name) const {
    auto it = m_materials.find(name);
    if (it == m_materials.end()) {
        return nullptr;
    }
    return &it->second;
}

void MaterialManager::clear() {
    m_materials.clear();
    m_names.clear();
}
