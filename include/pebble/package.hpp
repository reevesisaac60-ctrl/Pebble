#pragma once

#include <string>
#include <vector>

namespace pebble {

struct PackageFunction {
    std::string name;
    std::vector<std::string> parameters;
    std::string returnType;
    std::string runtimeSymbol;
};

struct Package {
    std::string name;
    std::vector<PackageFunction> functions;
};

[[nodiscard]] const std::vector<Package>& builtinPackages();

[[nodiscard]] const Package* findBuiltinPackage(const std::string& name);

[[nodiscard]] const PackageFunction* findPackageFunction(
    const Package& package,
    const std::string& name
);

}
