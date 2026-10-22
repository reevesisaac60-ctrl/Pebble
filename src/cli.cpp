#include "cli.hpp"

#include "pebble_manifest/project.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace pebble_cli {

namespace {

std::string describeDiagnostics(const PebbleManifestDiagnostics& diagnostics) {
    std::string message;

    for (size_t index = 0U; index < diagnostics.count; ++index) {
        const PebbleManifestDiagnostic& diagnostic = diagnostics.items[index];

        if (!message.empty()) {
            message += "\n";
        }

        if (diagnostic.file != nullptr && diagnostic.file[0] != '\0') {
            message += diagnostic.file;

            if (diagnostic.line > 0) {
                message += ":" + std::to_string(diagnostic.line);
            }

            message += ": ";
        }

        message += diagnostic.message;
    }

    return message;
}

bool writeFile(const fs::path& path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary);

    if (!file) {
        return false;
    }

    file << contents;

    return static_cast<bool>(file);
}

std::string packageSource() {
    return "export fn greet(who: String) -> String {\n"
           "    return \"Hello, \" + who + \"!\"\n"
           "}\n";
}

std::string exampleSource(const std::string& name) {
    return "import package " + name + "\n"
           "\n"
           "fn main() {\n"
           "    print(" + name + ".greet(\"Pebble\"))\n"
           "}\n";
}

}

bool writeManifest(const std::string& path, const PebbleManifest* manifest) {
    char* const rendered = pebble_manifest_write(manifest);

    if (rendered == nullptr) {
        return false;
    }

    const bool written = writeFile(fs::path(path), rendered);
    std::free(rendered);
    return written;
}

bool loadProject(
    const std::string& root,
    Project& project,
    std::string& message
) {
    PebbleManifestDiagnostics diagnostics = {};
    PebbleProject loaded = {};

    if (!pebble_project_load(root.c_str(), &loaded, &diagnostics)) {
        message = describeDiagnostics(diagnostics);
        pebble_manifest_diagnostics_destroy(&diagnostics);
        pebble_project_destroy(&loaded);
        return false;
    }

    if (!pebble_project_resolve(&loaded, &diagnostics)) {
        message = describeDiagnostics(diagnostics);
        pebble_manifest_diagnostics_destroy(&diagnostics);
        pebble_project_destroy(&loaded);
        return false;
    }

    pebble_manifest_diagnostics_destroy(&diagnostics);

    project.root = loaded.root == nullptr ? std::string{} : loaded.root;
    project.manifestPath =
        loaded.manifest_path == nullptr ? std::string{} : loaded.manifest_path;
    project.entryPoint =
        loaded.entry_point == nullptr ? std::string{} : loaded.entry_point;

    for (size_t index = 0U; index < loaded.packages.count; ++index) {
        const PebbleResolvedPackage& package = loaded.packages.items[index];

        project.searchPaths.push_back(package.directory);
        project.packageNames.push_back(package.name);
        project.packageVersions.push_back(package.version);
    }

    pebble_project_destroy(&loaded);
    return true;
}

int initCommand(const std::string& root, const std::string& name) {
    const fs::path directory(root);
    const fs::path manifestPath = directory / PEBBLE_MANIFEST_NAME;

    if (fs::exists(manifestPath)) {
        std::cerr << "error: '" << manifestPath.string()
                  << "' already exists.\n";
        return 1;
    }

    std::string packageName = name;

    if (packageName.empty()) {
        packageName = fs::absolute(directory).filename().string();
    }

    if (packageName.empty()) {
        std::cerr << "error: the package needs a name.\n";
        return 1;
    }

    PebbleManifest* const manifest =
        pebble_manifest_create(packageName.c_str(), "0.1.0");

    if (manifest == nullptr) {
        std::cerr << "error: out of memory.\n";
        return 1;
    }

    const bool written = writeManifest(manifestPath.string(), manifest);
    pebble_manifest_destroy(manifest);

    if (!written) {
        std::cerr << "error: cannot write '" << manifestPath.string() << "'\n";
        return 1;
    }

    const fs::path packagePath = directory / (packageName + ".peb");
    const fs::path examplePath = directory / "main.peb";

    if (!writeFile(packagePath, packageSource()) ||
        !writeFile(examplePath, exampleSource(packageName))) {
        std::cerr << "error: cannot write the package sources.\n";
        return 1;
    }

    std::cout << "Created package '" << packageName << "' in "
              << fs::absolute(directory).string() << '\n';
    std::cout << "  " << manifestPath.filename().string() << '\n';
    std::cout << "  " << packagePath.filename().string() << '\n';
    std::cout << "  " << examplePath.filename().string() << '\n';
    return 0;
}

int addCommand(const std::string& root, const std::string& path) {
    const fs::path directory(root);
    const fs::path manifestPath = directory / PEBBLE_MANIFEST_NAME;

    if (!fs::exists(manifestPath)) {
        std::cerr << "error: '" << manifestPath.string()
                  << "' does not exist. Run 'pebble init' first.\n";
        return 1;
    }

    const fs::path dependencyPath = fs::absolute(path);
    const fs::path dependencyManifest = dependencyPath / PEBBLE_MANIFEST_NAME;

    if (!fs::exists(dependencyManifest)) {
        std::cerr << "error: '" << dependencyManifest.string()
                  << "' does not exist.\n";
        return 1;
    }

    PebbleManifestDiagnostics diagnostics = {};
    PebbleManifest* const dependency =
        pebble_manifest_load(dependencyManifest.string().c_str(), &diagnostics);

    if (dependency == nullptr) {
        std::cerr << "error: " << describeDiagnostics(diagnostics) << '\n';
        pebble_manifest_diagnostics_destroy(&diagnostics);
        return 1;
    }

    pebble_manifest_diagnostics_destroy(&diagnostics);

    const std::string dependencyName = dependency->name;
    pebble_manifest_destroy(dependency);

    PebbleManifest* const manifest =
        pebble_manifest_load(manifestPath.string().c_str(), &diagnostics);

    if (manifest == nullptr) {
        std::cerr << "error: " << describeDiagnostics(diagnostics) << '\n';
        pebble_manifest_diagnostics_destroy(&diagnostics);
        return 1;
    }

    pebble_manifest_diagnostics_destroy(&diagnostics);

    if (pebble_manifest_find_dependency(manifest, dependencyName.c_str()) !=
        nullptr) {
        std::cerr << "error: the dependency '" << dependencyName
                  << "' is already declared.\n";
        pebble_manifest_destroy(manifest);
        return 1;
    }

    std::error_code error;
    fs::path relative = fs::relative(dependencyPath, fs::absolute(directory),
                                     error);

    if (error) {
        relative = dependencyPath;
    }

    if (!pebble_manifest_add_dependency(manifest, dependencyName.c_str(),
                                        relative.string().c_str(), "")) {
        std::cerr << "error: out of memory.\n";
        pebble_manifest_destroy(manifest);
        return 1;
    }

    const bool written = writeManifest(manifestPath.string(), manifest);
    pebble_manifest_destroy(manifest);

    if (!written) {
        std::cerr << "error: cannot write '" << manifestPath.string() << "'\n";
        return 1;
    }

    std::cout << "Added dependency '" << dependencyName << "' from "
              << relative.string() << '\n';
    return 0;
}

int removeCommand(const std::string& root, const std::string& name) {
    const fs::path directory(root);
    const fs::path manifestPath = directory / PEBBLE_MANIFEST_NAME;

    if (!fs::exists(manifestPath)) {
        std::cerr << "error: '" << manifestPath.string()
                  << "' does not exist. Run 'pebble init' first.\n";
        return 1;
    }

    PebbleManifestDiagnostics diagnostics = {};
    PebbleManifest* const manifest =
        pebble_manifest_load(manifestPath.string().c_str(), &diagnostics);

    if (manifest == nullptr) {
        std::cerr << "error: " << describeDiagnostics(diagnostics) << '\n';
        pebble_manifest_diagnostics_destroy(&diagnostics);
        return 1;
    }

    pebble_manifest_diagnostics_destroy(&diagnostics);

    if (!pebble_manifest_remove_dependency(manifest, name.c_str())) {
        std::cerr << "error: the dependency '" << name
                  << "' is not declared.\n";
        pebble_manifest_destroy(manifest);
        return 1;
    }

    const bool written = writeManifest(manifestPath.string(), manifest);
    pebble_manifest_destroy(manifest);

    if (!written) {
        std::cerr << "error: cannot write '" << manifestPath.string() << "'\n";
        return 1;
    }

    std::cout << "Removed dependency '" << name << "'\n";
    return 0;
}

namespace {

int installProject(const std::string& root, bool announce) {
    Project project;
    std::string message;

    if (!loadProject(root, project, message)) {
        std::cerr << "error: " << message << '\n';
        return 1;
    }

    PebbleManifestDiagnostics diagnostics = {};
    PebbleProject loaded = {};

    if (!pebble_project_load(root.c_str(), &loaded, &diagnostics)) {
        std::cerr << "error: " << describeDiagnostics(diagnostics) << '\n';
        pebble_manifest_diagnostics_destroy(&diagnostics);
        pebble_project_destroy(&loaded);
        return 1;
    }

    if (!pebble_project_resolve(&loaded, &diagnostics)) {
        std::cerr << "error: " << describeDiagnostics(diagnostics) << '\n';
        pebble_manifest_diagnostics_destroy(&diagnostics);
        pebble_project_destroy(&loaded);
        return 1;
    }

    pebble_manifest_diagnostics_destroy(&diagnostics);

    const bool written = pebble_project_write_lockfile(&loaded);
    const size_t count = loaded.packages.count;

    for (size_t index = 0U; index < count; ++index) {
        std::cout << "  " << loaded.packages.items[index].name << ' '
                  << loaded.packages.items[index].version << " ("
                  << loaded.packages.items[index].path << ")\n";
    }

    pebble_project_destroy(&loaded);

    if (!written) {
        std::cerr << "error: cannot write the lockfile.\n";
        return 1;
    }

    if (announce) {
        std::cout << "Resolved " << count << " dependenc"
                  << (count == 1U ? "y" : "ies") << ".\n";
    }

    return 0;
}

}

int installCommand(const std::string& root) {
    return installProject(root, true);
}

int updateCommand(const std::string& root) {
    return installProject(root, true);
}

int treeCommand(const std::string& root) {
    Project project;
    std::string message;

    if (!loadProject(root, project, message)) {
        std::cerr << "error: " << message << '\n';
        return 1;
    }

    PebbleManifestDiagnostics diagnostics = {};
    PebbleManifest* const manifest =
        pebble_manifest_load(project.manifestPath.c_str(), &diagnostics);

    if (manifest == nullptr) {
        std::cerr << "error: " << describeDiagnostics(diagnostics) << '\n';
        pebble_manifest_diagnostics_destroy(&diagnostics);
        return 1;
    }

    pebble_manifest_diagnostics_destroy(&diagnostics);

    std::cout << manifest->name << ' ' << manifest->version << '\n';

    for (size_t index = 0U; index < project.packageNames.size(); ++index) {
        std::cout << "  " << project.packageNames[index] << ' '
                  << project.packageVersions[index] << '\n';
    }

    pebble_manifest_destroy(manifest);
    return 0;
}

int packageCommand(const std::string& root) {
    Project project;
    std::string message;

    if (!loadProject(root, project, message)) {
        std::cerr << "error: " << message << '\n';
        return 1;
    }

    PebbleManifestDiagnostics diagnostics = {};
    PebbleManifest* const manifest =
        pebble_manifest_load(project.manifestPath.c_str(), &diagnostics);

    if (manifest == nullptr) {
        std::cerr << "error: " << describeDiagnostics(diagnostics) << '\n';
        pebble_manifest_diagnostics_destroy(&diagnostics);
        return 1;
    }

    pebble_manifest_diagnostics_destroy(&diagnostics);

    const fs::path directory(root);
    const fs::path distribution =
        directory / "dist" / (std::string(manifest->name) + "-" +
                              std::string(manifest->version));

    std::error_code error;
    fs::create_directories(distribution, error);

    if (error) {
        std::cerr << "error: cannot create '" << distribution.string() << "'\n";
        pebble_manifest_destroy(manifest);
        return 1;
    }

    if (!writeManifest((distribution / PEBBLE_MANIFEST_NAME).string(),
                       manifest)) {
        std::cerr << "error: cannot write the package manifest.\n";
        pebble_manifest_destroy(manifest);
        return 1;
    }

    size_t copied = 0U;

    for (const fs::directory_entry& entry :
         fs::directory_iterator(directory, error)) {
        if (error) {
            break;
        }

        if (!entry.is_regular_file(error) || error) {
            continue;
        }

        if (entry.path().extension() != ".peb") {
            continue;
        }

        if (entry.path().filename() == "pebble.toml") {
            continue;
        }

        const fs::path target = distribution / entry.path().filename();

        if (fs::copy_file(entry.path(), target,
                          fs::copy_options::overwrite_existing, error)) {
            copied++;
        } else {
            error.clear();
        }
    }

    std::cout << "Packaged " << manifest->name << ' ' << manifest->version
              << " with " << copied << " source file(s) into "
              << distribution.string() << '\n';

    pebble_manifest_destroy(manifest);
    return 0;
}

}
