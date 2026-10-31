#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

source_archive="pebble-0.3.0.tar.gz"

if [[ -e "$source_archive" ]]; then
    printf 'Refusing to overwrite existing %s\n' "$source_archive" >&2
    exit 1
fi

tmp_archive="$(mktemp /tmp/pebble-source-XXXXXX.tar.gz)"

cleanup() {
    rm -f -- "$tmp_archive" "$source_archive"
}
trap cleanup EXIT

tar -czf "$tmp_archive" \
    --exclude='./.git' \
    --exclude='./.git/*' \
    --exclude='./build' \
    --exclude='./build/*' \
    --exclude='./build-*' \
    --exclude='./build-*/*' \
    --exclude='./cmake-build-*' \
    --exclude='./cmake-build-*/*' \
    --exclude='./examples/*/build' \
    --exclude='./examples/*/build/*' \
    --exclude='./dist' \
    --exclude='./dist/*' \
    --exclude='./pebble-linux-x86_64.tar.gz' \
    --exclude='./*.pkg.tar.zst' \
    .

mv -- "$tmp_archive" "$source_archive"

makepkg --syncdeps --cleanbuild --force
