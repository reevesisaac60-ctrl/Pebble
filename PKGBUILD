# Maintainer: Pebble project
pkgname=pebble
pkgver=0.3.2
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
source=("$pkgname-$pkgver.tar.gz")
sha256sums=('0f17082f1fd4a4843301b402c91fe475d8aa8872bbe28b2b86a2de4e48795bb4')

build() {
    cmake -S "$srcdir" -B "$srcdir/build-root" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_BINDIR=bin \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_INSTALL_DATADIR=share \
        -DPEBBLE_WARNINGS_AS_ERRORS=ON

    cmake --build "$srcdir/build-root" \
        --target pebble pebble_native pebble_studio \
        --parallel
}

package() {
    DESTDIR="$pkgdir" cmake --install "$srcdir/build-root"
}
