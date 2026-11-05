
#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

version="$(
    sed -nE 's/^[[:space:]]*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
        CMakeLists.txt | head -n 1
)"

if [[ -z "$version" ]]; then
    echo "Could not determine Pebble version from CMakeLists.txt" >&2
    exit 1
fi

architecture="$(dpkg-architecture -qDEB_HOST_ARCH)"
output_dir="$repo_root/package-output"
output_file="$output_dir/pebble_${version}_${architecture}.deb"

if [[ -e "$output_file" ]]; then
    echo "Refusing to overwrite existing package: $output_file" >&2
    exit 1
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

build_dir="$tmp_dir/build"
package_root="$tmp_dir/package"
dependency_workspace="$tmp_dir/dependency-workspace"

mkdir -p "$package_root" "$dependency_workspace/debian" "$output_dir"

echo "Configuring Pebble $version..."
cmake -S "$repo_root" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_INSTALL_BINDIR=bin \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_INSTALL_DATADIR=share

echo "Building Pebble..."
cmake --build "$build_dir" \
    --target pebble pebble_native pebble_studio \
    --parallel

echo "Installing into package staging directory..."
DESTDIR="$package_root" cmake --install "$build_dir"
find "$package_root/usr/share/pebble/examples" \
    -type d -name build -prune -exec rm -rf {} +
echo "Detecting shared-library dependencies..."
cp "$repo_root/packaging/debian/control" \
    "$dependency_workspace/debian/control"

(
    cd "$dependency_workspace"
    dpkg-shlibdeps -O \
        -e"$package_root/usr/bin/pebble" \
        -e"$package_root/usr/bin/pebble_native" \
        -e"$package_root/usr/bin/pebble_studio"
) > "$tmp_dir/shlibdeps.txt"

depends="$(sed -n 's/^shlibs:Depends=//p' "$tmp_dir/shlibdeps.txt")"

if [[ -z "$depends" ]]; then
    echo "Could not determine runtime dependencies" >&2
    cat "$tmp_dir/shlibdeps.txt" >&2
    exit 1
fi

mkdir -p "$package_root/DEBIAN"

cat > "$package_root/DEBIAN/control" <<EOF
Package: pebble
Version: $version
Section: devel
Priority: optional
Architecture: $architecture
Maintainer: Pebble Project
Depends: $depends
Homepage: https://github.com/reevesisaac60-ctrl/Pebble
Description: Pebble language, compiler, and native Qt Studio
 Pebble is a native programming language and development environment.
 It includes compiler frontends, a package system, and Pebble Studio.
EOF

echo "Building Debian package..."
dpkg-deb --root-owner-group --build "$package_root" "$output_file"

echo
echo "Package created:"
echo "$output_file"
dpkg-deb --info "$output_file"
