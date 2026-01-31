# Maintainer: Ido Perlmuter <ido@ido50.net>
pkgname=waymux-bin
pkgver=0.0.1
pkgrel=1
pkgdesc="Tabbed Wayland Compositor"
arch=('x86_64')
url="https://github.com/ido50/waymux"
license=('MIT')
depends=('wlroots0.19' 'wayland' 'libxkbcommon' 'cairo' 'tomlc17-git')
makedepends=('meson>=0.58.1' 'scdoc>=1.9.2' 'check')
checkdepends=('check')
source=()
sha256sums=()

build() {
  cd "$startdir"
  arch-meson -Duse_git_version=false "$startdir" build
  meson compile -C build
}

check() {
  cd "$startdir"
  meson test -C build --print-errorlogs
}

package() {
  cd "$startdir"
  meson install -C build --destdir "$pkgdir"

  # Install example profile
  install -Dm644 example-profile.toml "$pkgdir/usr/share/$pkgname/examples/example-profile.toml"

  # Install desktop file
  install -Dm644 waymux.desktop "$pkgdir/usr/share/applications/waymux.desktop"

  # Install license
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
