# Maintainer: Unified Conky Control Center Team
pkgname=unified-conky-control-center
pkgver=1.0.2
pkgrel=1
pkgdesc="A unified control center for managing Conky configurations across X11 and Wayland"
arch=('x86_64')
url="https://github.com/vamps-goes-coding/UnifiedConkyControlCenter"
license=('GPL')
depends=('qt6-base' 'qt6-wayland' 'conky')
makedepends=('cmake' 'gcc' 'qt6-tools')
source=("https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases/download/v${pkgver}/UnifiedConkyControlCenter-${pkgver}-Linux-x86_64.tar.gz")
sha256sums=('SKIP')

prepare() {
    cd "${srcdir}"
    tar -xzf "UnifiedConkyControlCenter-${pkgver}-Linux-x86_64.tar.gz"
}

package() {
    local src="${srcdir}/UnifiedConkyControlCenter-${pkgver}-Linux-x86_64"

    install -Dm755 "$src/bin/UnifiedConkyControlCenter" "${pkgdir}/usr/bin/UnifiedConkyControlCenter"

    [ -f "$src/share/applications/conky-control-center.desktop" ] && \
        install -Dm644 "$src/share/applications/conky-control-center.desktop" \
            "${pkgdir}/usr/share/applications/conky-control-center.desktop"

    [ -d "$src/share/UnifiedConkyControlCenter" ] && \
        cp -r "$src/share/UnifiedConkyControlCenter" "${pkgdir}/usr/share/"

    [ -f "$src/share/icons/hicolor/256x256/apps/UnifiedConkyControlCenter.png" ] && \
        install -Dm644 "$src/share/icons/hicolor/256x256/apps/UnifiedConkyControlCenter.png" \
            "${pkgdir}/usr/share/icons/hicolor/256x256/apps/UnifiedConkyControlCenter.png"
}
