# Maintainer: Pebble project
pkgname=pebble
pkgver=0.3.3
pkgrel=1
pkgdesc="Pebble language, compiler, and native Qt Studio"
arch=('x86_64')
url="https://github.com/reevesisaac60-ctrl/Pebble"
license=('MIT')
depends=(
    'gcc'
    'glfw'
    'pkgconf'
    'qt6-base'
)
makedepends=(
    'cmake>=3.25'
    'ninja'
    'qt6-tools'
)
source=("https://github.com/reevesisaac60-ctrl/Pebble/archive/refs/tags/v${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
    local _builddir="$srcdir/Pebble-$pkgver"
    cmake -S "$_builddir" -B "$_builddir/build-root" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_BINDIR=bin \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_INSTALL_DATADIR=share \
        -DPEBBLE_WARNINGS_AS_ERRORS=ON

    cmake --build "$_builddir/build-root" \
        --target pebble pebble_native pebble_studio \
        --parallel
}

package() {
    local _builddir="$srcdir/Pebble-$pkgver"
    DESTDIR="$pkgdir" cmake --install "$_builddir/build-root"
}
