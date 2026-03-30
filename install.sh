#!/bin/bash
# Unified Conky Control Center - Smart Installer Script
# Detects OS and installs the appropriate package format

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
RELEASE_URL="${RELEASE_URL:-https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases/download}"
VERSION="${VERSION:-latest}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
SKIP_SUDO_CHECK="${SKIP_SUDO_CHECK:-false}"

# Helper functions
info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# Check if running with appropriate privileges
check_privileges() {
    if [ "$SKIP_SUDO_CHECK" != "true" ] && [ "$EUID" -ne 0 ] && ! sudo -n true 2>/dev/null; then
        error "This script requires root/sudo privileges. Please run with: sudo $0"
    fi
}

# Detect OS and distribution
detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
        OS_VERSION=$VERSION_ID
    else
        error "Cannot detect operating system"
    fi

    case "$OS" in
        ubuntu|debian)
            info "Detected: $OS (version $OS_VERSION)"
            INSTALLER_TYPE="deb"
            ;;
        fedora|rhel|centos)
            info "Detected: $OS (version $OS_VERSION)"
            INSTALLER_TYPE="rpm"
            ;;
        opensuse*|sles)
            info "Detected: $OS (version $OS_VERSION)"
            INSTALLER_TYPE="rpm"
            ;;
        arch|manjaro|endeavouros|garuda)
            info "Detected: $OS (version $OS_VERSION)"
            INSTALLER_TYPE="arch"
            ;;
        *)
            warn "Unknown or unsupported distribution: $OS"
            info "Falling back to TGZ installer"
            INSTALLER_TYPE="tgz"
            ;;
    esac
}

# Download package
download_package() {
    local package_name=$1
    local temp_dir=$(mktemp -d)
    
    info "Downloading $package_name..."
    
    if ! command -v curl &> /dev/null && ! command -v wget &> /dev/null; then
        error "Neither curl nor wget found. Please install one of them."
    fi
    
    if command -v curl &> /dev/null; then
        curl -L "$RELEASE_URL/$VERSION/$package_name" -o "$temp_dir/$package_name" || error "Failed to download $package_name"
    else
        wget -O "$temp_dir/$package_name" "$RELEASE_URL/$VERSION/$package_name" || error "Failed to download $package_name"
    fi
    
    echo "$temp_dir/$package_name"
}

# Install DEB package
install_deb() {
    info "Installing DEB package..."
    
    if ! command -v apt-get &> /dev/null; then
        error "apt-get not found. This DEB requires a Debian-based system."
    fi
    
    sudo apt-get update || warn "Failed to update package list"
    sudo apt-get install -y "$1" || error "Failed to install DEB package"
    info "DEB package installed successfully"
}

# Install RPM package
install_rpm() {
    info "Installing RPM package..."
    
    if command -v dnf &> /dev/null; then
        sudo dnf install -y "$1" || error "Failed to install RPM package with dnf"
    elif command -v yum &> /dev/null; then
        sudo yum install -y "$1" || error "Failed to install RPM package with yum"
    elif command -v zypper &> /dev/null; then
        sudo zypper install -y "$1" || error "Failed to install RPM package with zypper"
    else
        error "No RPM package manager found (dnf, yum, or zypper)"
    fi
    
    info "RPM package installed successfully"
}

# Install Arch Linux package
install_arch() {
    info "Setting up Arch Linux PKGBUILD..."
    
    if ! command -v makepkg &> /dev/null; then
        error "makepkg not found. This requires an Arch Linux system with pacman."
    fi
    
    # Create temporary directory for PKGBUILD
    local temp_dir=$(mktemp -d)
    local pkgbuild_path="$temp_dir/PKGBUILD"
    
    # Create PKGBUILD file
    cat > "$pkgbuild_path" << 'EOF'
# Maintainer: Unified Conky Control Center Team
pkgname=unified-conky-control-center
pkgver=VERSION_PLACEHOLDER
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
EOF
    
    # Replace version placeholder
    sed -i "s/VERSION_PLACEHOLDER/${VERSION#v}/g" "$pkgbuild_path"
    
    info "Building Arch Linux package..."
    cd "$temp_dir"
    
    # Build package (this will download and extract the TGZ)
    makepkg -si --noconfirm || error "Failed to build/install Arch Linux package"
    
    # Cleanup
    cd /
    rm -rf "$temp_dir"
    
    info "Arch Linux package installed successfully"
}

# Install TGZ archive
install_tgz() {
    info "Installing from TGZ archive..."
    
    local temp_dir=$(mktemp -d)
    tar -xzf "$1" -C "$temp_dir" || error "Failed to extract TGZ archive"
    
    info "Installing to $INSTALL_PREFIX..."
    sudo cp -r "$temp_dir/usr/local/"* "$INSTALL_PREFIX/" 2>/dev/null || \
    sudo cp -r "$temp_dir/"* "$INSTALL_PREFIX/" 2>/dev/null || \
    error "Failed to install TGZ archive"
    
    # Update desktop database if available
    if command -v update-desktop-database &> /dev/null; then
        sudo update-desktop-database "$INSTALL_PREFIX/share/applications" || warn "Failed to update desktop database"
    fi
    
    rm -rf "$temp_dir"
    info "TGZ archive installed successfully to $INSTALL_PREFIX"
}

# Local install from build directory
install_local() {
    info "Installing from local build directory..."
    
    if [ ! -d "./build" ]; then
        error "Build directory not found. Please run 'cmake .. && make -j\$(nproc)' first."
    fi
    
    cd build
    info "Running 'make install' to $INSTALL_PREFIX..."
    sudo cmake --install . --prefix "$INSTALL_PREFIX" || error "Failed to install from build"
    cd ..
    
    info "Local installation completed successfully"
}

# Main installation flow
main() {
    echo "=========================================="
    echo "Unified Conky Control Center - Installer"
    echo "=========================================="
    echo

    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --version)
                VERSION="$2"
                shift 2
                ;;
            --prefix)
                INSTALL_PREFIX="$2"
                shift 2
                ;;
            --local)
                info "Using local build from ./build directory"
                check_privileges
                install_local
                exit 0
                ;;
            --skip-sudo-check)
                SKIP_SUDO_CHECK="true"
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [OPTIONS]"
                echo
                echo "Options:"
                echo "  --version <VERSION>      Release version to install (default: latest)"
                echo "  --prefix <PATH>          Installation prefix (default: /usr/local)"
                echo "  --local                  Install from local build directory"
                echo "  --skip-sudo-check        Skip sudo privilege check"
                echo "  --help, -h              Show this help message"
                echo
                echo "Examples:"
                echo "  sudo $0                  # Install latest release for your OS"
                echo "  sudo $0 --version v1.0.0 # Install specific version"
                echo "  $0 --local               # Install from ./build directory"
                exit 0
                ;;
            *)
                error "Unknown option: $1"
                ;;
        esac
    done

    check_privileges
    detect_os
    
    # Determine package filename based on installer type
    case "$INSTALLER_TYPE" in
        deb)
            PACKAGE_NAME="unified-conky-control-center-${VERSION}-Linux-x86_64.deb"
            PACKAGE_PATH=$(download_package "$PACKAGE_NAME")
            install_deb "$PACKAGE_PATH"
            ;;
        rpm)
            PACKAGE_NAME="unified-conky-control-center-${VERSION}-Linux-x86_64.rpm"
            PACKAGE_PATH=$(download_package "$PACKAGE_NAME")
            install_rpm "$PACKAGE_PATH"
            ;;
        arch)
            install_arch
            ;;
        tgz)
            PACKAGE_NAME="unified-conky-control-center-${VERSION}-Linux-x86_64.tar.gz"
            PACKAGE_PATH=$(download_package "$PACKAGE_NAME")
            install_tgz "$PACKAGE_PATH"
            ;;
    esac
    
    echo
    info "Installation completed successfully!"
    info "Run 'UnifiedConkyControlCenter' to start the application"
    echo
}

# Run main function
main "$@"
