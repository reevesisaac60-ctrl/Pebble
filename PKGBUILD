# Maintainer: Pebble project
pkgname=pebble
pkgver=0.3.1
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
sha256sums=('b88eaed87fbb2ab96da91f4c749f92e676ff8dfe20c73a8cfc2a9dfc573074a8')

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
