# Maintainer: Your Name <your.email@example.com>
pkgname=waymux
pkgver=0.2.1
pkgrel=1
pkgdesc="Tabbed Wayland Compositor"
arch=('x86_64')
url="https://github.com/yourusername/waymux"
license=('MIT')
depends=('wlroots>=0.19' 'wayland' 'libxkbcommon' 'cairo' 'libtomlc17')
makedepends=('meson>=0.58.1' 'scdoc>=1.9.2' 'check')
checkdepends=('check')
source=()
sha256sums=()

build() {
  arch-meson build
  meson compile -C build
}

check() {
  meson test -C build --print-errorlogs
}

package() {
  DESTDIR="$pkgdir" meson install -C build

  # Install example profile
  install -Dm644 example-profile.toml "$pkgdir/usr/share/$pkgname/examples/example-profile.toml"

  # Install desktop file
  install -Dm644 waymux.desktop "$pkgdir/usr/share/applications/waymux.desktop"

  # Install license
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
