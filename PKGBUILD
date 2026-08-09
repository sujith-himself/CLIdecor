# Maintainer: sujith-himself <https://github.com/sujith-himself>
pkgname=clidecor
pkgver=1.0.0
pkgrel=1
pkgdesc="A blazing-fast, config-driven Neofetch replacement written in C++17"
arch=('x86_64' 'aarch64')
url="https://github.com/sujith-himself/CLIdecor"
license=('MIT')
depends=()
makedepends=('gcc')
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/heads/main.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$srcdir/CLIdecor-main"
    g++ -O3 -std=c++17 src/main.cpp -o clidecor -pthread
}

package() {
    cd "$srcdir/CLIdecor-main"

    # Binary
    install -Dm755 clidecor "$pkgdir/usr/bin/clidecor"

    # Man page
    if [ -f "clidecor.1" ]; then
        install -Dm644 clidecor.1 "$pkgdir/usr/share/man/man1/clidecor.1"
    fi

    # Default config
    install -Dm644 config.conf "$pkgdir/usr/share/clidecor/config.conf"

    # Tux default image
    install -Dm644 tux.png "$pkgdir/usr/share/clidecor/tux.png"

    # License
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
