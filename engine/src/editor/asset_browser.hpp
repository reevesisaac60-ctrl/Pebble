#pragma once

#include <string>
#include <vector>

#ifdef PEBBLE_ENGINE_HAS_IMGUI
#include <imgui.h>
#endif

// A reusable ImGui asset browser modal. It lets the user navigate the
// project's asset directory and pick a file, returning a project-relative
// path. Designed for skybox cubemap directories and material texture paths.
//
// Usage:
//   AssetBrowser browser;
//   browser.open("Select texture", {".ppm", ".png"});
//   if (browser.draw("Asset Browser", &showModal, selectedPath)) {
//       // selectedPath now holds the project-relative path
//   }
class AssetBrowser {
public:
    AssetBrowser() = default;

    // Open the browser with the given title and optional file extension
    // filters (e.g. {".ppm", ".png"}). When empty, all files are shown.
    void open(const std::string& title,
              const std::vector<std::string>& extensions = {});

    // When true, confirming the browser returns the current directory
    // (project-relative) instead of a selected file. Used for assets that are
    // directories, such as cubemap face folders.
    void setSelectDirectory(bool selectDirectory);

    // Draw the modal. Returns true when the user confirms a selection, in
    // which case `outPath` is set to the project-relative path. Returns false
    // otherwise (cancelled or still open).
    bool draw(const std::string& popupId, bool* open,
              std::string* outPath);

    // Set the root assets directory (project-relative or absolute).
    void setRootDirectory(const std::string& rootDir);

    const std::string& currentDirectory() const { return m_currentDir; }
    const std::vector<std::string>& dirs() const { return m_dirs; }
    const std::vector<std::string>& files() const { return m_files; }

    // Resolve a full path to a project-relative path (used to store portable
    // asset references). Pure filesystem helper, safe to call without an
    // ImGui context.
    std::string relativePath(const std::string& fullPath) const;

    // Test whether a filename matches the current extension filters.
    bool matchesFilter(const std::string& filename) const;

private:
    void refreshListing();

    std::string m_title;
    std::string m_rootDir;
    std::string m_currentDir;
    std::vector<std::string> m_extensions;
    std::vector<std::string> m_dirs;
    std::vector<std::string> m_files;
    std::string m_selectedPath;
    std::string m_searchText;
    char m_searchBuffer[256] = "";
    bool m_dirsValid = false;
    bool m_selectDirectory = false;
    int m_selectedDirIndex = -1;
    int m_selectedFileIndex = -1;
};
