#pragma once

#include "pebble_manifest/manifest.h"

#include <string>
#include <vector>

namespace pebble_cli {

struct Project {
    std::string root;
    std::string manifestPath;
    std::string entryPoint;
    std::vector<std::string> searchPaths;
    std::vector<std::string> packageNames;
    std::vector<std::string> packageVersions;
};

bool loadProject(
    const std::string& root,
    Project& project,
    std::string& message
);

bool writeManifest(const std::string& path, const PebbleManifest* manifest);

int initCommand(const std::string& root, const std::string& name);
int addCommand(const std::string& root, const std::string& path);
int removeCommand(const std::string& root, const std::string& name);
int installCommand(const std::string& root);
int updateCommand(const std::string& root);
int treeCommand(const std::string& root);
int packageCommand(const std::string& root);

}
