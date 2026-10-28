# Maintainer: Pebble project
pkgname=pebble
pkgver=0.3.0
pkgrel=1
pkgdesc="Native Pebble language and game creation platform (compiler, engine, studio)"
arch=('x86_64')
url="https://github.com/Kilo-Org/pebble"
license=('MIT')
depends=(
    'sdl3'
    'libepoxy'
    'qt6-base'
)
makedepends=(
    'cmake>=3.25'
    'ninja'
    'gcc'
    'python'
    'qt6-tools'
    'git'
)
source=("$pkgname-$pkgver.tar.gz")
sha256sums=('SKIP')

_build_root="build-root"
_build_engine="build-engine"

build() {
    # Root project: compiler, CLI, Qt Studio. Unix Makefiles generator.
    # No CMAKE_BUILD_TYPE: matches the configuration under which the test
    # suite passes (Release-mode _FORTIFY_SOURCE triggers a pre-existing
    # -Werror=unused-result in test_resources_c.c unrelated to packaging).
    cmake -S . -B "$_build_root" \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_BINDIR=bin \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_INSTALL_DATADIR=share
    # Build only the runtime targets shipped in the package; test executables
    # are not part of the install payload and are skipped here.
    cmake --build "$_build_root" --target pebble pebble_native pebble_studio

    # Engine: standalone SDL3 + OpenGL project. Ninja generator.
    # Fetches Dear ImGui docking branch from GitHub at configure time.
    cmake -S engine -B "$_build_engine" -G Ninja \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_BINDIR=bin \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_INSTALL_DATADIR=share
    cmake --build "$_build_engine"
}

package() {
    # Stage both projects into a single coherent layout under $pkgdir.
    # The staged installation is the source of truth for the package payload.
    DESTDIR="$pkgdir" cmake --install "$_build_root"
    DESTDIR="$pkgdir" cmake --install "$_build_engine"
}
