#include "editor/asset_browser.hpp"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#ifdef PEBBLE_ENGINE_HAS_IMGUI
#include <imgui.h>
#endif

namespace fs = std::filesystem;

void AssetBrowser::open(const std::string& title,
                        const std::vector<std::string>& extensions) {
    m_title = title;
    m_extensions = extensions;
    m_selectedPath.clear();
    m_selectedDirIndex = -1;
    m_selectedFileIndex = -1;
    m_selectDirectory = false;
    m_dirsValid = false;
    m_searchText.clear();
    m_searchBuffer[0] = '\0';
    refreshListing();
}

void AssetBrowser::setSelectDirectory(bool selectDirectory) {
    m_selectDirectory = selectDirectory;
}

void AssetBrowser::setRootDirectory(const std::string& rootDir) {
    m_rootDir = rootDir;
    m_currentDir = rootDir;
    m_dirsValid = false;
    refreshListing();
}

bool AssetBrowser::matchesFilter(const std::string& filename) const {
    if (m_extensions.empty()) {
        return true;
    }
    std::string lowerFilename = filename;
    std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const auto& ext : m_extensions) {
        std::string lowerExt = ext;
        std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lowerFilename.size() >= lowerExt.size() &&
            lowerFilename.compare(lowerFilename.size() - lowerExt.size(), lowerExt.size(), lowerExt) == 0) {
            return true;
        }
    }
    return false;
}

static bool matchesSearch(const std::string& filename,
                          const std::string& searchText) {
    if (searchText.empty()) {
        return true;
    }
    std::string lowerName = filename;
    std::string lowerSearch = searchText;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lowerName.find(lowerSearch) != std::string::npos;
}

std::string AssetBrowser::relativePath(const std::string& fullPath) const {
    fs::path full(fullPath);
    fs::path root(m_rootDir);
    try {
        return fs::relative(full, root).generic_string();
    } catch (...) {
        return fullPath;
    }
}

void AssetBrowser::refreshListing() {
    m_dirs.clear();
    m_files.clear();
    m_dirsValid = false;

    std::error_code ec;
    if (!fs::exists(m_currentDir, ec) || !fs::is_directory(m_currentDir, ec)) {
        return;
    }

    // Collect directories first (sorted), then files (sorted).
    std::vector<std::string> dirs;
    std::vector<std::string> files;

    for (fs::directory_iterator it(m_currentDir, ec), end;
         it != end && !ec;
         it.increment(ec)) {
        if (ec) break;
        if (it->is_directory(ec)) {
            if (ec) continue;
            dirs.push_back(it->path().filename().generic_string());
        } else if (it->is_regular_file(ec)) {
            if (ec) continue;
            if (matchesFilter(it->path().filename().generic_string())) {
                files.push_back(it->path().filename().generic_string());
            }
        }
    }

    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());

    // Apply search filter to files.
    if (!m_searchText.empty()) {
        std::vector<std::string> filtered;
        for (const auto& f : files) {
            if (matchesSearch(f, m_searchText)) {
                filtered.push_back(f);
            }
        }
        files = std::move(filtered);
    }

    m_dirs = std::move(dirs);
    m_files = std::move(files);
    m_dirsValid = true;
}

bool AssetBrowser::draw(const std::string& popupId, bool* open,
                        std::string* outPath) {
    (void)popupId;
#ifdef PEBBLE_ENGINE_HAS_IMGUI
    // The caller owns the modal popup. Do not test *open here: the caller
    // clears its one-shot open request as soon as OpenPopup is issued.
    if (open == nullptr) return false;

    bool confirmed = false;
    ImGui::Text("%s", m_title.c_str());
    ImGui::Separator();

    // --- Breadcrumb path ---
    std::string relCurrent = relativePath(m_currentDir);
    ImGui::Text("Path: %s", relCurrent.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Root")) {
        m_currentDir = m_rootDir;
        m_selectedDirIndex = -1;
        m_selectedFileIndex = -1;
        refreshListing();
    }

    // --- Navigation buttons ---
    ImGui::SameLine();
    if (ImGui::Button("..")) {
        fs::path p(m_currentDir);
        if (p.has_parent_path() &&
            p.parent_path().generic_string().size() >= m_rootDir.size()) {
            m_currentDir = p.parent_path().generic_string();
            m_selectedDirIndex = -1;
            m_selectedFileIndex = -1;
            refreshListing();
        }
    }

    // --- Search ---
    ImGui::Separator();
    ImGui::Text("Search:");
    ImGui::SameLine();
    if (ImGui::InputText("##search", m_searchBuffer,
                         sizeof(m_searchBuffer))) {
        m_searchText = m_searchBuffer;
        m_selectedDirIndex = -1;
        m_selectedFileIndex = -1;
        refreshListing();
    }

    ImGui::Separator();

    // --- Directories list ---
    const float listHeight = std::clamp(ImGui::GetContentRegionAvail().y * 0.48f,
                                        100.0f, 420.0f);
    const float halfWidth = std::max(100.0f, ImGui::GetContentRegionAvail().x * 0.48f);
    if (ImGui::BeginListBox("##dirs",
                            ImVec2(halfWidth, listHeight))) {
        for (int i = 0; i < static_cast<int>(m_dirs.size()); ++i) {
            bool selected = (m_selectedDirIndex == i);
            if (ImGui::Selectable(m_dirs[i].c_str(), selected)) {
                m_selectedDirIndex = i;
                m_selectedFileIndex = -1;
                m_currentDir =
                    (fs::path(m_currentDir) / m_dirs[i]).generic_string();
                refreshListing();
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndListBox();
    }

    ImGui::SameLine();

    // --- Files list ---
    if (ImGui::BeginListBox("##files",
                            ImVec2(-FLT_MIN, listHeight))) {
        for (int i = 0; i < static_cast<int>(m_files.size()); ++i) {
            bool selected = (m_selectedFileIndex == i);
            if (ImGui::Selectable(m_files[i].c_str(), selected)) {
                m_selectedFileIndex = i;
                m_selectedDirIndex = -1;
                m_selectedPath =
                    (fs::path(m_currentDir) / m_files[i]).generic_string();
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndListBox();
    }

    // Show empty state.
    if (m_dirs.empty() && m_files.empty()) {
        ImGui::TextDisabled("(no matching assets)");
    }

    // Show selected path.
    if (m_selectedFileIndex >= 0) {
        ImGui::Separator();
        ImGui::Text("Selected: %s",
                    relativePath(m_selectedPath).c_str());
    }

    ImGui::Separator();

    if (ImGui::Button("OK", ImVec2(120, 0))) {
        if (outPath != nullptr) {
            if (m_selectDirectory) {
                // Confirm the current directory (e.g. a cubemap face
                // folder) as the asset.
                *outPath = relativePath(m_currentDir);
                confirmed = true;
            } else if (m_selectedFileIndex >= 0) {
                *outPath = relativePath(m_selectedPath);
                confirmed = true;
            }
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
    }

    return confirmed;
#else
    (void)open;
    (void)outPath;
    return false;
#endif
}
