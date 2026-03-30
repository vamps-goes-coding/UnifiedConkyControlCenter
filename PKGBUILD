# Maintainer: Unified Conky Control Center Team
pkgname=unified-conky-control-center
pkgver=1.0.1
pkgrel=1
pkgdesc="A unified control center for managing Conky configurations across X11 and Wayland"
arch=('x86_64')
url="https://github.com/vamps-goes-coding/UnifiedConkyControlCenter"
license=('GPL')
depends=('qt6-base' 'qt6-wayland' 'conky')
makedepends=('cmake' 'gcc' 'qt6-tools')
source=("https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases/download/v${pkgver}/unified-conky-control-center-${pkgver}-Linux-x86_64.tar.gz")
sha256sums=('SKIP')

prepare() {
    cd "${srcdir}"
    tar -xzf "unified-conky-control-center-${pkgver}-Linux-x86_64.tar.gz"
}

package() {
    cd "${srcdir}/unified-conky-control-center-${pkgver}-Linux-x86_64"

    # Install binary
    install -Dm755 "usr/local/bin/UnifiedConkyControlCenter" "${pkgdir}/usr/bin/UnifiedConkyControlCenter"

    # Install desktop file
    if [ -f "usr/local/share/applications/unified-conky-control-center.desktop" ]; then
        install -Dm644 "usr/local/share/applications/unified-conky-control-center.desktop" "${pkgdir}/usr/share/applications/unified-conky-control-center.desktop"
    fi

    # Install config files
    if [ -d "usr/local/share/UnifiedConkyControlCenter" ]; then
        cp -r "usr/local/share/UnifiedConkyControlCenter" "${pkgdir}/usr/share/"
    fi
}