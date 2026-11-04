#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
pkgver="$(sed -n "s/^pkgver=//p" "${repo_root}/PKGBUILD")"
source_archive="pebble-${pkgver}.tar.gz"
output_dir="${repo_root}/package-output"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/pebble-package-XXXXXX")"
cleanup() { rm -rf -- "$work_dir"; }
trap cleanup EXIT

cd "$repo_root"

if [[ -e "$source_archive" ]]; then
    printf 'Refusing to overwrite existing %s\n' "$source_archive" >&2
    exit 1
fi


tar --create --file=- \
    --directory="$repo_root" \
    --mtime='@0' \
    --sort=name \
    --exclude='./.git' \
    --exclude='./.git/*' \
    --exclude='./build' \
    --exclude='./build/*' \
    --exclude='./build-*' \
    --exclude='./build-*/*' \
    --exclude='./build-release' \
    --exclude='./build-release/*' \
    --exclude='./cmake-build-*' \
    --exclude='./cmake-build-*/*' \
    --exclude='./src/build' \
    --exclude='./src/build/*' \
    --exclude='./src/build-root' \
    --exclude='./src/build-root/*' \
    --exclude='./src/src' \
    --exclude='./src/src/*' \
    --exclude='./src/Pebble-*' \
    --exclude='./src/pebble-*.tar.gz' \
    --exclude='./examples/*/build' \
    --exclude='./examples/*/build/*' \
    --exclude='./dist' \
    --exclude='./dist/*' \
    --exclude='./package-output' \
    --exclude='./package-output/*' \
    --exclude='./pkg' \
    --exclude='./pkg/*' \
    --exclude='./release-assets' \
    --exclude='./release-assets/*' \
    --exclude='./CMakeFiles' \
    --exclude='./CMakeFiles/*' \
    --exclude='./imgui.ini' \
    --exclude='./:GITHUB_ENV' \
    --exclude='./:GITHUB_PATH' \
    --exclude='./pebble-linux-x86_64.tar.gz' \
    --exclude='./pebble-*.tar.gz' \
    --exclude='./*.pkg.tar.zst' \
    . | gzip -n > "${work_dir}/${source_archive}"

archive_path="${work_dir}/${source_archive}"
checksum=$(sha256sum "$archive_path" | awk '{print $1}')
printf 'Source archive SHA-256: %s\n' "$checksum"

pkgbuild="${work_dir}/PKGBUILD"
cp -- "${repo_root}/PKGBUILD" "$pkgbuild"

replacement="sha256sums=('${checksum}')"
awk -v replacement="$replacement" '/^sha256sums=/ { print replacement; next } { print }' "$pkgbuild" > "${pkgbuild}.patched"
mv -f -- "${pkgbuild}.patched" "$pkgbuild"

cd "$work_dir"
makepkg --syncdeps --cleanbuild --force

shopt -s nullglob
packages=("${work_dir}"/*.pkg.tar.zst)
shopt -u nullglob

if ((${#packages[@]} == 0)); then
    printf 'No package artifact was produced.\n' >&2
    exit 1
fi

mkdir -p -- "$output_dir"

for package in "${packages[@]}"; do
    base="$(basename -- "$package")"
    destination="${output_dir}/${base}"
    publish_tmp="$(mktemp "${output_dir}/.${base}.XXXXXX")"
    cp -- "$package" "$publish_tmp"
    chmod 644 "$publish_tmp"
    mv -f -- "$publish_tmp" "$destination"
done

printf 'Package artifact(s) written to %s\n' "$output_dir"