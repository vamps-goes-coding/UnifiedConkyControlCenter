#!/bin/bash
# =============================================================================
# Unified Conky Control Center - Graphical Installer
# Supports: Fresh Install, Local Build, Update, Uninstall
# GUI: Auto-detects KDE/GNOME/XFCE/Wayland/X11 and uses native toolkit
# =============================================================================

set -e
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-wayland}"

# =============================================================================
# CONFIG — change version here only, nowhere else
# =============================================================================
APP_NAME="UnifiedConkyControlCenter"
APP_DISPLAY_NAME="Unified Conky Control Center"
VERSION="v1.0.29"
VERSION_NUM="${VERSION#v}"
GITHUB_USER="vamps-goes-coding"
GITHUB_REPO="UnifiedConkyControlCenter"
RELEASE_URL="https://github.com/$GITHUB_USER/$GITHUB_REPO/releases/download"
GITHUB_API="https://api.github.com/repos/$GITHUB_USER/$GITHUB_REPO/releases/latest"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
CONFIG_DIR="$HOME/.config/$APP_NAME"
SKIP_SUDO_CHECK="${SKIP_SUDO_CHECK:-false}"

# =============================================================================
# COLORS (CLI only)
# =============================================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

# =============================================================================
# LOGGING
# =============================================================================
LOG_FILE="/tmp/${APP_NAME}-install-$(date +%Y%m%d-%H%M%S).log"
log() { echo "[$(date +%H:%M:%S)] $*" >> "$LOG_FILE"; }
info()  { echo -e "${GREEN}[INFO]${NC} $1" >&2;  log "INFO:  $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $1" >&2; log "WARN:  $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1" >&2;   log "ERROR: $1"; }
fatal() { error "$1"; gui_error "$1"; exit 1; }

# =============================================================================
# STEP 1 — DISPLAY SERVER DETECTION
# =============================================================================
detect_display_server() {
    if [ -n "$WAYLAND_DISPLAY" ] || [ "$XDG_SESSION_TYPE" = "wayland" ]; then
        DISPLAY_SERVER="wayland"
    elif [ -n "$DISPLAY" ] || [ "$XDG_SESSION_TYPE" = "x11" ]; then
        DISPLAY_SERVER="x11"
    else
        DISPLAY_SERVER="headless"
    fi
    log "Display server: $DISPLAY_SERVER"
}

# =============================================================================
# STEP 2 — DESKTOP ENVIRONMENT DETECTION
# =============================================================================
detect_desktop() {
    local de="${XDG_CURRENT_DESKTOP:-${DESKTOP_SESSION:-unknown}}"
    de=$(echo "$de" | tr '[:upper:]' '[:lower:]')

    case "$de" in
        *kde*|*plasma*)           DETECTED_DE="kde" ;;
        *gnome*)                  DETECTED_DE="gnome" ;;
        *xfce*)                   DETECTED_DE="xfce" ;;
        *cinnamon*)               DETECTED_DE="cinnamon" ;;
        *mate*)                   DETECTED_DE="mate" ;;
        *lxqt*)                   DETECTED_DE="lxqt" ;;
        *budgie*)                 DETECTED_DE="budgie" ;;
        *hyprland*|*sway*|*wlr*) DETECTED_DE="wlr" ;;
        *)                        DETECTED_DE="unknown" ;;
    esac
    log "Desktop environment: $DETECTED_DE (raw: $de)"
}

# =============================================================================
# STEP 3 — GUI TOOLKIT SELECTION
# =============================================================================
detect_gui_toolkit() {
    detect_display_server
    detect_desktop

    # Headless — force CLI
    if [ "$DISPLAY_SERVER" = "headless" ]; then
        GUI_TOOLKIT="cli"
        log "Headless environment — using CLI"
        return
    fi

    # Prefer native toolkit for known DEs, then fall back to what's installed
    case "$DETECTED_DE" in
        kde|lxqt)
            if command -v kdialog &>/dev/null; then
                GUI_TOOLKIT="kdialog"
            elif command -v zenity &>/dev/null; then
                GUI_TOOLKIT="zenity"
            elif command -v yad &>/dev/null; then
                GUI_TOOLKIT="yad"
            else
                GUI_TOOLKIT="cli"
            fi
            ;;
        gnome|xfce|cinnamon|mate|budgie|wlr|unknown)
            if command -v zenity &>/dev/null; then
                GUI_TOOLKIT="zenity"
            elif command -v yad &>/dev/null; then
                GUI_TOOLKIT="yad"
            elif command -v kdialog &>/dev/null; then
                GUI_TOOLKIT="kdialog"
            else
                GUI_TOOLKIT="cli"
            fi
            ;;
    esac

    log "GUI toolkit selected: $GUI_TOOLKIT"
    info "Using GUI toolkit: $GUI_TOOLKIT (DE: $DETECTED_DE, Display: $DISPLAY_SERVER)"
}

# =============================================================================
# GUI HELPER FUNCTIONS — unified interface over kdialog/zenity/yad/cli
# =============================================================================

# Show info dialog
gui_info() {
    local title="$1" msg="$2"
    case "$GUI_TOOLKIT" in
        kdialog)
            # On Wayland, kdialog may hang — use timeout and fallback to CLI
            if [ "$DISPLAY_SERVER" = "wayland" ]; then
                if ! timeout 10 kdialog --title "$title" --msgbox "$msg" 2>/dev/null; then
                    warn "kdialog timed out or failed, falling back to CLI"
                    echo -e "\n${BOLD}$title${NC}\n$msg\n"
                    read -rp "Press Enter to continue..." _
                fi
            else
                kdialog --title "$title" --msgbox "$msg"
            fi
            ;;
        zenity)  zenity --info --title="$title" --text="$msg" --width=400 ;;
        yad)     yad --info --title="$title" --text="$msg" --width=400 --button="OK:0" ;;
        cli)     echo -e "\n${BOLD}$title${NC}\n$msg\n" ;;
    esac
}

# Show error dialog
gui_error() {
    local msg="$1"
    case "$GUI_TOOLKIT" in
        kdialog)
            # On Wayland, kdialog may hang — use timeout and fallback to CLI
            if [ "$DISPLAY_SERVER" = "wayland" ]; then
                if ! timeout 10 kdialog --title "Error — $APP_DISPLAY_NAME" --error "$msg\n\nSee log: $LOG_FILE" 2>/dev/null; then
                    error "$msg\nSee log: $LOG_FILE"
                fi
            else
                kdialog --title "Error — $APP_DISPLAY_NAME" --error "$msg\n\nSee log: $LOG_FILE"
            fi
            ;;
        zenity)  zenity --error --title="Error" --text="$msg\n\nSee log: $LOG_FILE" --width=400 ;;
        yad)     yad --error --title="Error" --text="$msg\n\nSee log: $LOG_FILE" --width=400 --button="OK:0" ;;
        cli)     error "$msg\nSee log: $LOG_FILE" ;;
    esac
}

# Show yes/no confirmation — returns 0 for yes, 1 for no
gui_confirm() {
    local title="$1" msg="$2"
    case "$GUI_TOOLKIT" in
        kdialog)
            # On Wayland, kdialog may hang — use timeout and fallback to CLI
            if [ "$DISPLAY_SERVER" = "wayland" ]; then
                local result
                if result=$(timeout 10 kdialog --title "$title" --yesno "$msg" 2>/dev/null); then
                    return 0
                else
                    local rc=$?
                    # If timeout (124) or error, fall back to CLI
                    if [ $rc -eq 124 ] || [ $rc -ne 0 ]; then
                        warn "kdialog timed out or failed, falling back to CLI"
                        echo -e "\n${BOLD}$title${NC}\n$msg"
                        read -rp "Confirm? [y/N]: " resp
                        [[ "$resp" =~ ^[Yy]$ ]]
                    else
                        return $rc
                    fi
                fi
            else
                kdialog --title "$title" --yesno "$msg"
            fi
            ;;
        zenity)  zenity --question --title="$title" --text="$msg" --width=400 ;;
        yad)     yad --question --title="$title" --text="$msg" --width=400 --button="Yes:0" --button="No:1" ;;
        cli)
            echo -e "\n${BOLD}$title${NC}\n$msg"
            read -rp "Confirm? [y/N]: " resp
            [[ "$resp" =~ ^[Yy]$ ]]
            ;;
    esac
}

# Show progress bar — pass messages on stdin, one per step
# Usage: some_func | gui_progress "Title" "Message" total_steps
gui_progress_start() {
    local title="$1" msg="$2"
    case "$GUI_TOOLKIT" in
        kdialog)
            PROGRESS_DBUS=$(kdialog --title "$title" --progressbar "$msg" 100)
            ;;
        zenity)
            exec 3> >(zenity --progress --title="$title" --text="$msg" \
                --percentage=0 --auto-close --width=450 2>/dev/null)
            PROGRESS_FD=3
            ;;
        yad)
            exec 3> >(yad --progress --title="$title" --text="$msg" \
                --percentage=0 --auto-close --width=450 --button="Cancel:1" 2>/dev/null)
            PROGRESS_FD=3
            ;;
        cli)
            info "$title: $msg"
            ;;
    esac
}

gui_progress_update() {
    local pct="$1" msg="$2"
    case "$GUI_TOOLKIT" in
        kdialog)
            qdbus $PROGRESS_DBUS Set "" value "$pct" 2>/dev/null || true
            qdbus $PROGRESS_DBUS setLabelText "$msg" 2>/dev/null || true
            ;;
        zenity|yad)
            echo "$pct" >&${PROGRESS_FD} 2>/dev/null || true
            echo "# $msg" >&${PROGRESS_FD} 2>/dev/null || true
            ;;
        cli)
            info "[$pct%] $msg"
            ;;
    esac
}

gui_progress_end() {
    case "$GUI_TOOLKIT" in
        kdialog)
            qdbus $PROGRESS_DBUS close 2>/dev/null || true
            ;;
        zenity|yad)
            echo "100" >&${PROGRESS_FD} 2>/dev/null || true
            exec 3>&- 2>/dev/null || true
            ;;
        cli) ;;
    esac
}

# Show text input dialog — echoes result to stdout
gui_input() {
    local title="$1" msg="$2" default="$3"
    case "$GUI_TOOLKIT" in
        kdialog) kdialog --title "$title" --inputbox "$msg" "$default" ;;
        zenity)  zenity --entry --title="$title" --text="$msg" --entry-text="$default" ;;
        yad)     yad --entry --title="$title" --text="$msg" --entry-text="$default" --button="OK:0" --button="Cancel:1" ;;
        cli)
            echo -e "${BOLD}$title${NC}\n$msg"
            read -rp "[$default]: " val
            echo "${val:-$default}"
            ;;
    esac
}

# Show list/radio selection — echoes selected item to stdout
gui_menu() {
    local title="$1" msg="$2"
    shift 2
    case "$GUI_TOOLKIT" in
        kdialog)
            # Convert zenity format (TRUE/FALSE tag item ...) to kdialog format (tag item on/off ...)
            local -a kdialog_args=()
            local first=true
            while [ $# -gt 0 ]; do
                local state="$1" tag="$2" item="$3"
                shift 3
                if [ "$state" = "TRUE" ]; then
                    kdialog_args+=("$tag" "$item" "on")
                else
                    kdialog_args+=("$tag" "$item" "off")
                fi
            done
            kdialog --title "$title" --radiolist "$msg" "${kdialog_args[@]}"
            ;;
        zenity)  zenity --list --title="$title" --text="$msg" \
                     --radiolist --column="" --column="Option" "$@" ;;
        yad)     yad --list --title="$title" --text="$msg" \
                     --radiolist --column="" --column="Option" "$@" \
                     --button="OK:0" --button="Cancel:1" ;;
        cli)
            echo -e "${BOLD}$title${NC}\n$msg"
            local i=1
            local -a opts=()
            while [ $# -gt 0 ]; do
                # zenity radiolist format: TRUE/FALSE label value...
                # skip TRUE/FALSE tokens
                local tok="$1"; shift
                if [[ "$tok" != "TRUE" && "$tok" != "FALSE" ]]; then
                    echo "  $i) $tok"
                    opts+=("$tok")
                    ((i++))
                fi
            done
            read -rp "Select [1]: " sel
            echo "${opts[$((${sel:-1}-1))]}"
            ;;
    esac
}

# =============================================================================
# DISTRO DETECTION
# =============================================================================
detect_os() {
    if [ ! -f /etc/os-release ]; then
        fatal "Cannot detect operating system — /etc/os-release not found"
    fi

    . /etc/os-release
    OS="${ID:-unknown}"
    OS_LIKE="${ID_LIKE:-}"
    OS_VERSION="${VERSION_ID:-unknown}"

    case "$OS" in
        ubuntu|debian|linuxmint|pop|elementary|zorin|kali|parrot)
            INSTALLER_TYPE="deb" ;;
        fedora|rhel|centos|rocky|almalinux|ol)
            INSTALLER_TYPE="rpm_dnf" ;;
        opensuse*|sles|sled)
            INSTALLER_TYPE="rpm_zypper" ;;
        arch|manjaro|endeavouros|garuda|cachyos|artix|crystal)
            INSTALLER_TYPE="arch" ;;
        *)
            # Check ID_LIKE for derivative distros
            case "$OS_LIKE" in
                *debian*|*ubuntu*) INSTALLER_TYPE="deb" ;;
                *fedora*|*rhel*)   INSTALLER_TYPE="rpm_dnf" ;;
                *arch*)            INSTALLER_TYPE="arch" ;;
                *)                 INSTALLER_TYPE="tgz" ;;
            esac
            ;;
    esac

    log "OS: $OS ($OS_VERSION), installer type: $INSTALLER_TYPE"
    info "Detected: $OS $OS_VERSION → using $INSTALLER_TYPE installer"
}

# =============================================================================
# ON-DEMAND SUDO EXECUTION
# =============================================================================
# Execute command with sudo, prompting ONLY when actually needed
# No pre-authentication, no password caching, no temporary files
sudo_run() {
    if [ "$EUID" -eq 0 ]; then
        # Already running as root
        "$@"
    elif [ "$GUI_TOOLKIT" = "zenity" ] || [ "$GUI_TOOLKIT" = "yad" ]; then
        zenity --password --title="Sudo Password Required" 2>/dev/null | sudo -S "$@"
    elif [ "$GUI_TOOLKIT" = "kdialog" ]; then
        kdialog --password "Enter sudo password:" --title "Sudo Password Required" 2>/dev/null | sudo -S "$@"
    elif command -v ksshaskpass >/dev/null 2>&1; then
        SUDO_ASKPASS=$(command -v ksshaskpass) sudo -A "$@"
    elif command -v ssh-askpass >/dev/null 2>&1; then
        SUDO_ASKPASS=$(command -v ssh-askpass) sudo -A "$@"
    else
        # Fallback to standard terminal sudo
        sudo "$@"
    fi
}

# =============================================================================
# DEPENDENCY CHECK
# =============================================================================
check_dependencies() {
    local missing=()

    # Check for Qt (either Qt5 or Qt6)
    if ! (ldconfig -p 2>/dev/null | grep -q "libQt[56]Core") && \
       ! (find /usr/lib* /usr/local/lib* -name "libQt*Core*" 2>/dev/null | grep -q .); then
        missing+=("Qt5 or Qt6 runtime libraries")
    fi

    # Check for conky
    if ! command -v conky &>/dev/null; then
        missing+=("conky")
    fi

    if [ ${#missing[@]} -eq 0 ]; then
        log "All dependencies satisfied"
        return 0
    fi

    local missing_str=$(printf '• %s\n' "${missing[@]}")
    gui_confirm "Missing Dependencies" \
        "The following dependencies are missing:\n\n$missing_str\n\nWould you like to install them automatically?" && \
        install_dependencies "${missing[@]}" || \
        warn "Skipping dependency installation — app may not run correctly"
}

install_dependencies() {
    gui_progress_start "Installing Dependencies" "Installing missing packages..."
    gui_progress_update 10 "Detecting package manager..."

    case "$INSTALLER_TYPE" in
        deb)
            gui_progress_update 30 "Running apt-get..."
            sudo apt-get update -y >> "$LOG_FILE" 2>&1
            command -v conky &>/dev/null || sudo apt-get install -y conky >> "$LOG_FILE" 2>&1
            # Try Qt6 first, fall back to Qt5
            sudo apt-get install -y qt6-base-dev 2>/dev/null || \
                sudo apt-get install -y qtbase5-dev >> "$LOG_FILE" 2>&1 || true
            ;;
        rpm_dnf)
            gui_progress_update 30 "Running dnf..."
            command -v conky &>/dev/null || sudo dnf install -y conky >> "$LOG_FILE" 2>&1
            sudo dnf install -y qt6-qtbase 2>/dev/null || \
                sudo dnf install -y qt5-qtbase >> "$LOG_FILE" 2>&1 || true
            ;;
        rpm_zypper)
            gui_progress_update 30 "Running zypper..."
            command -v conky &>/dev/null || sudo zypper install -y conky >> "$LOG_FILE" 2>&1
            sudo zypper install -y libqt6-qtbase 2>/dev/null || \
                sudo zypper install -y libqt5-qtbase >> "$LOG_FILE" 2>&1 || true
            ;;
        arch)
            gui_progress_update 30 "Running pacman..."
            command -v conky &>/dev/null || sudo pacman -S --noconfirm conky >> "$LOG_FILE" 2>&1
            sudo pacman -S --noconfirm --needed qt6-base 2>/dev/null || \
                sudo pacman -S --noconfirm --needed qt5-base >> "$LOG_FILE" 2>&1 || true
            ;;
    esac

    gui_progress_update 100 "Dependencies installed"
    gui_progress_end
}

# =============================================================================
# PACKAGE DOWNLOAD
# =============================================================================
get_latest_version() {
    if command -v curl &>/dev/null; then
        curl -s "$GITHUB_API" 2>/dev/null | grep '"tag_name"' | \
            sed 's/.*"tag_name": *"\([^"]*\)".*/\1/' | head -1
    elif command -v wget &>/dev/null; then
        wget -qO- "$GITHUB_API" 2>/dev/null | grep '"tag_name"' | \
            sed 's/.*"tag_name": *"\([^"]*\)".*/\1/' | head -1
    fi
}

download_file() {
    local url="$1" dest="$2"
    log "Downloading: $url → $dest"
    if command -v curl &>/dev/null; then
        curl -L --progress-bar "$url" -o "$dest" 2>>"$LOG_FILE" || return 1
    elif command -v wget &>/dev/null; then
        wget -q --show-progress "$url" -O "$dest" 2>>"$LOG_FILE" || return 1
    else
        fatal "Neither curl nor wget found — cannot download files"
    fi
}

get_package_name() {
    local ver="${VERSION_NUM}"
    case "$INSTALLER_TYPE" in
        deb)         echo "unified-conky-control-center_${ver}_amd64.deb" ;;
        rpm_dnf|rpm_zypper) echo "unified-conky-control-center-${ver}-1.x86_64.rpm" ;;
        arch|tgz)    echo "UnifiedConkyControlCenter-${ver}-Linux-x86_64.tar.gz" ;;
    esac
}

# =============================================================================
# INSTALL BACKENDS
# =============================================================================
install_deb() {
    local pkg="$1"
    log "Installing deb: $pkg"
    sudo dpkg -i "$pkg" >> "$LOG_FILE" 2>&1 || \
        sudo apt-get install -f -y >> "$LOG_FILE" 2>&1 || \
        fatal "Failed to install DEB package"
}

install_rpm() {
    local pkg="$1"
    log "Installing rpm: $pkg"
    case "$INSTALLER_TYPE" in
        rpm_dnf)    sudo dnf install -y "$pkg" >> "$LOG_FILE" 2>&1 || fatal "Failed to install RPM (dnf)" ;;
        rpm_zypper) sudo zypper install -y "$pkg" >> "$LOG_FILE" 2>&1 || fatal "Failed to install RPM (zypper)" ;;
    esac
}

install_arch_pkg() {
    local tgz="$1"
    local tmp=$(mktemp -d)

    log "Building Arch package from: $tgz"

    # Write PKGBUILD
    cat > "$tmp/PKGBUILD" << PKGEOF
# Maintainer: $APP_DISPLAY_NAME Team
pkgname=unified-conky-control-center
pkgver=${VERSION_NUM}
pkgrel=1
pkgdesc="A unified control center for managing Conky configurations across X11 and Wayland"
arch=(x86_64)
url="https://github.com/${GITHUB_USER}/${GITHUB_REPO}"
license=(GPL)
depends=(conky)
optdepends=('qt6-base: Qt6 runtime' 'qt5-base: Qt5 runtime')
source=("${APP_NAME}-${VERSION_NUM}-Linux-x86_64.tar.gz")
sha256sums=(SKIP)

prepare() {
    cd "\${srcdir}"
    tar -xzf "${APP_NAME}-${VERSION_NUM}-Linux-x86_64.tar.gz" 2>/dev/null || true
}

package() {
    local src="\${srcdir}/UnifiedConkyControlCenter-${VERSION_NUM}-Linux-x86_64"

    install -Dm755 "\$src/bin/UnifiedConkyControlCenter" "\${pkgdir}/usr/bin/UnifiedConkyControlCenter"

    [ -f "\$src/share/applications/conky-control-center.desktop" ] && \
        install -Dm644 "\$src/share/applications/conky-control-center.desktop" \
            "\${pkgdir}/usr/share/applications/conky-control-center.desktop"

    [ -d "\$src/share/UnifiedConkyControlCenter" ] && \
        cp -r "\$src/share/UnifiedConkyControlCenter" "\${pkgdir}/usr/share/"

    [ -f "\$src/share/icons/hicolor/256x256/apps/UnifiedConkyControlCenter.png" ] && \
        install -Dm644 "\$src/share/icons/hicolor/256x256/apps/UnifiedConkyControlCenter.png" \
            "\${pkgdir}/usr/share/icons/hicolor/256x256/apps/UnifiedConkyControlCenter.png"
}
PKGEOF

    # Copy TGZ into build dir
    cp "$tgz" "$tmp/"

    cd "$tmp"
    makepkg -si --noconfirm --force >> "$LOG_FILE" 2>&1 || fatal "Failed to build/install Arch package"
    cd /
    rm -rf "$tmp"
}

install_tgz() {
    local pkg="$1"
    local tmp=$(mktemp -d)
    log "Installing tgz to $INSTALL_PREFIX"

    tar -xzf "$pkg" -C "$tmp" >> "$LOG_FILE" 2>&1 || fatal "Failed to extract archive"

    # Try common extract layouts
    local src="$tmp"
    [ -d "$tmp/usr/local" ] && src="$tmp/usr/local"
    [ -d "$tmp/unified-conky-control-center-${VERSION_NUM}-Linux_x86_64/usr/local" ] && \
        src="$tmp/unified-conky-control-center-${VERSION_NUM}-Linux_x86_64/usr/local"

    sudo cp -r "$src/"* "$INSTALL_PREFIX/" >> "$LOG_FILE" 2>&1 || \
        fatal "Failed to copy files to $INSTALL_PREFIX"

    command -v update-desktop-database &>/dev/null && \
        sudo update-desktop-database "$INSTALL_PREFIX/share/applications" 2>/dev/null || true

    rm -rf "$tmp"
}

install_local_build() {
    # Auto-build if build directory doesn't exist
    if [ ! -d "./build" ]; then
        log "Build directory not found — running cmake build automatically"
        info "Build directory not found — running cmake build automatically..."
        
        # Check for cmake
        if ! command -v cmake &>/dev/null; then
            fatal "cmake not found. Please install cmake first."
        fi
        
        # Check for build-essential/base-devel
        case "$INSTALLER_TYPE" in
            deb)
                if ! dpkg -l | grep -q build-essential 2>/dev/null; then
                    info "Installing build-essential..."
                    sudo apt-get install -y build-essential >> "$LOG_FILE" 2>&1 || true
                fi
                ;;
            arch)
                if ! pacman -Q base-devel &>/dev/null; then
                    info "Installing base-devel..."
                    sudo pacman -S --noconfirm --needed base-devel >> "$LOG_FILE" 2>&1 || true
                fi
                ;;
            rpm_dnf)
                if ! rpm -q gcc-c++ &>/dev/null; then
                    info "Installing development tools..."
                    sudo dnf groupinstall -y "Development Tools" >> "$LOG_FILE" 2>&1 || true
                fi
                ;;
        esac
        
        # Configure and build
        info "Running cmake -B build..."
        cmake -B build -DCMAKE_BUILD_TYPE=Release >> "$LOG_FILE" 2>&1 || \
            fatal "cmake configure failed — check $LOG_FILE"
        
        info "Running cmake --build build..."
        cmake --build build -j"$(nproc)" >> "$LOG_FILE" 2>&1 || \
            fatal "cmake build failed — check $LOG_FILE"
    fi
    
    log "Installing from local build"
    cd build
    sudo cmake --install . --prefix "$INSTALL_PREFIX" >> "$LOG_FILE" 2>&1 || \
        fatal "cmake --install failed — check $LOG_FILE"
    cd ..
}

# =============================================================================
# DESKTOP SHORTCUT
# =============================================================================
create_desktop_shortcut() {
    local desktop_file="$HOME/.local/share/applications/unified-conky-control-center.desktop"
    mkdir -p "$(dirname "$desktop_file")"
    cat > "$desktop_file" << DEOF
[Desktop Entry]
Name=$APP_DISPLAY_NAME
Comment=Manage Conky panels, themes, and configurations
Exec=$INSTALL_PREFIX/bin/$APP_NAME
Icon=$APP_NAME
Terminal=false
Type=Application
Categories=Utility;System;
Keywords=conky;desktop;widget;monitor;
DEOF
    log "Desktop shortcut created: $desktop_file"
}

# =============================================================================
# UNINSTALL
# =============================================================================
do_uninstall() {
    gui_confirm "Uninstall $APP_DISPLAY_NAME" \
        "Are you sure you want to uninstall $APP_DISPLAY_NAME $VERSION?" || {
        info "Uninstall cancelled"
        exit 0
    }

    local keep_config=false
    if [ -d "$CONFIG_DIR" ]; then
        gui_confirm "Keep Settings?" \
            "Keep your personal settings and config files?\n($CONFIG_DIR)" && keep_config=true
    fi

    # Sudo will prompt on demand when actually needed

    gui_progress_start "Uninstalling" "Removing $APP_DISPLAY_NAME..."
    gui_progress_update 10 "Detecting install method..."

    # Try package manager removal first
    case "$INSTALLER_TYPE" in
        deb)
            if dpkg -l | grep -q unified-conky-control-center 2>/dev/null; then
                gui_progress_update 40 "Removing via apt..."
                sudo apt-get remove -y unified-conky-control-center >> "$LOG_FILE" 2>&1 || true
            fi ;;
        rpm_dnf)
            if rpm -q unified-conky-control-center &>/dev/null; then
                gui_progress_update 40 "Removing via dnf..."
                sudo dnf remove -y unified-conky-control-center >> "$LOG_FILE" 2>&1 || true
            fi ;;
        rpm_zypper)
            if rpm -q unified-conky-control-center &>/dev/null; then
                gui_progress_update 40 "Removing via zypper..."
                sudo zypper remove -y unified-conky-control-center >> "$LOG_FILE" 2>&1 || true
            fi ;;
        arch)
            if pacman -Q unified-conky-control-center &>/dev/null; then
                gui_progress_update 40 "Removing via pacman..."
                sudo pacman -R --noconfirm unified-conky-control-center >> "$LOG_FILE" 2>&1 || true
            fi ;;
    esac

    # Manual cleanup (catches tgz installs and leftovers)
    gui_progress_update 60 "Removing files..."
    sudo rm -f "$INSTALL_PREFIX/bin/$APP_NAME"
    sudo rm -f "$INSTALL_PREFIX/share/applications/unified-conky-control-center.desktop"
    sudo rm -f "$INSTALL_PREFIX/share/icons/hicolor/256x256/apps/$APP_NAME.png"
    sudo rm -rf "$INSTALL_PREFIX/share/$APP_NAME"
    rm -f "$HOME/.local/share/applications/unified-conky-control-center.desktop"

    if [ "$keep_config" = false ] && [ -d "$CONFIG_DIR" ]; then
        gui_progress_update 80 "Removing config..."
        rm -rf "$CONFIG_DIR"
    fi

    gui_progress_update 100 "Done"
    gui_progress_end

    gui_info "Uninstall Complete" "$APP_DISPLAY_NAME has been removed successfully."
    log "Uninstall complete"
}

# =============================================================================
# UPDATE
# =============================================================================
do_update() {
    gui_progress_start "Checking for Updates" "Fetching latest version info..."
    gui_progress_update 20 "Querying GitHub API..."

    local latest
    latest=$(get_latest_version) || true

    gui_progress_end

    if [ -z "$latest" ]; then
        gui_info "Update Check Failed" \
            "Could not reach GitHub to check for updates.\nCheck your internet connection and try again."
        exit 1
    fi

    if [ "$latest" = "$VERSION" ]; then
        gui_info "Up to Date" \
            "$APP_DISPLAY_NAME $VERSION is already the latest version."
        exit 0
    fi

    gui_confirm "Update Available" \
        "A new version is available!\n\nInstalled: $VERSION\nAvailable: $latest\n\nUpdate now?" || {
        info "Update cancelled"
        exit 0
    }

    # Update VERSION and VERSION_NUM then run fresh install
    VERSION="$latest"
    VERSION_NUM="${VERSION#v}"
    info "Updating to $VERSION..."
    do_fresh_install
}

# =============================================================================
# FRESH INSTALL
# =============================================================================
do_fresh_install() {
    # --- Welcome ---
    gui_info "Welcome" \
        "Welcome to the $APP_DISPLAY_NAME installer!\n\nVersion: $VERSION\nThis will install $APP_DISPLAY_NAME on your system."

    # --- Privileges ---
    # Sudo will prompt on demand when actually needed

    # --- Distro detection ---
    detect_os

    # --- Dependency check ---
    check_dependencies

    # --- Install location ---
    local chosen_prefix
    chosen_prefix=$(gui_input "Install Location" \
        "Where should $APP_DISPLAY_NAME be installed?" \
        "$INSTALL_PREFIX") || chosen_prefix="$INSTALL_PREFIX"
    [ -n "$chosen_prefix" ] && INSTALL_PREFIX="$chosen_prefix"

    # --- Desktop shortcut ---
    local want_shortcut=true
    gui_confirm "Desktop Shortcut" \
        "Create a desktop shortcut for $APP_DISPLAY_NAME?" || want_shortcut=false

    # --- Download & Install ---
    local tmp_dir
    tmp_dir=$(mktemp -d)
    local pkg_name
    pkg_name=$(get_package_name)
    local pkg_url="$RELEASE_URL/$VERSION/$pkg_name"
    local pkg_path="$tmp_dir/$pkg_name"

    # Check for bundled package first
    if [ -f "./packages/$pkg_name" ]; then
        info "Using bundled package"
        pkg_path="./packages/$pkg_name"
    else
        gui_progress_start "Downloading" "Downloading $APP_DISPLAY_NAME $VERSION..."
        gui_progress_update 10 "Connecting to GitHub..."
        download_file "$pkg_url" "$pkg_path" || fatal "Download failed. Check your internet connection.\nURL: $pkg_url"
        gui_progress_update 100 "Download complete"
        gui_progress_end
    fi

    gui_progress_start "Installing" "Installing $APP_DISPLAY_NAME..."
    gui_progress_update 10 "Preparing installation..."

    case "$INSTALLER_TYPE" in
        deb)
            gui_progress_update 40 "Installing DEB package..."
            install_deb "$pkg_path"
            ;;
        rpm_dnf|rpm_zypper)
            gui_progress_update 40 "Installing RPM package..."
            install_rpm "$pkg_path"
            ;;
        arch)
            gui_progress_update 40 "Building Arch package..."
            install_arch_pkg "$pkg_path"
            ;;
        tgz)
            gui_progress_update 40 "Extracting archive..."
            install_tgz "$pkg_path"
            ;;
    esac

    gui_progress_update 80 "Creating desktop shortcut..."
    [ "$want_shortcut" = true ] && create_desktop_shortcut

    gui_progress_update 90 "Updating system databases..."
    command -v gtk-update-icon-cache &>/dev/null && \
        sudo gtk-update-icon-cache -f /usr/share/icons/hicolor 2>/dev/null || true
    command -v update-desktop-database &>/dev/null && \
        sudo update-desktop-database 2>/dev/null || true

    gui_progress_update 100 "Installation complete"
    gui_progress_end

    rm -rf "$tmp_dir"

    # --- Success ---
    gui_confirm "Installation Complete" \
        "$APP_DISPLAY_NAME $VERSION has been installed successfully!\n\nLaunch it now?" && \
        nohup "$INSTALL_PREFIX/bin/$APP_NAME" &>/dev/null & disown || true

    log "Fresh install complete: $VERSION → $INSTALL_PREFIX"
}

# =============================================================================
# LOCAL BUILD INSTALL
# =============================================================================
do_local_install() {
    gui_info "Local Build Install" \
        "Installing $APP_DISPLAY_NAME from local build directory.\n\nMake sure you have run:\ncmake -B build && cmake --build build"

    # Sudo will prompt on demand when actually needed
    detect_os
    check_dependencies

    local chosen_prefix
    chosen_prefix=$(gui_input "Install Location" \
        "Where should $APP_DISPLAY_NAME be installed?" \
        "$INSTALL_PREFIX") || chosen_prefix="$INSTALL_PREFIX"
    [ -n "$chosen_prefix" ] && INSTALL_PREFIX="$chosen_prefix"

    local want_shortcut=true
    gui_confirm "Desktop Shortcut" \
        "Create a desktop shortcut for $APP_DISPLAY_NAME?" || want_shortcut=false

    gui_progress_start "Installing" "Installing from local build..."
    gui_progress_update 30 "Running cmake --install..."
    install_local_build
    gui_progress_update 80 "Creating desktop entry..."
    [ "$want_shortcut" = true ] && create_desktop_shortcut
    gui_progress_update 100 "Done"
    gui_progress_end

    gui_confirm "Installation Complete" \
        "$APP_DISPLAY_NAME installed from local build!\n\nLaunch it now?" && \
        nohup "$INSTALL_PREFIX/bin/$APP_NAME" &>/dev/null & disown || true

    log "Local install complete → $INSTALL_PREFIX"
}

# =============================================================================
# INTERACTIVE MODE SELECTION (no args given)
# =============================================================================
interactive_mode_select() {
    local choice
    choice=$(gui_menu "Installer" "What would you like to do?" \
        TRUE  "fresh"     "Fresh Install — download and install latest release" \
        FALSE "local"     "Local Build  — install from ./build directory" \
        FALSE "update"    "Update       — check for and install updates" \
        FALSE "uninstall" "Uninstall    — remove $APP_DISPLAY_NAME from this system" \
    ) || exit 0

    case "$choice" in
        *"Fresh"*|*"fresh"*)   do_fresh_install ;;
        *"Local"*|*"local"*)   do_local_install ;;
        *"Update"*|*"update"*) do_update ;;
        *"Uninstall"*|*"uninstall"*) do_uninstall ;;
        *) fatal "Unknown selection: $choice" ;;
    esac
}

# =============================================================================
# MAIN — argument parsing
# =============================================================================
main() {
    log "=== $APP_DISPLAY_NAME Installer started ==="
    log "Args: $*"

    detect_gui_toolkit

    # Parse args
    local mode=""
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --install|-i)        mode="fresh"; shift ;;
            --local|-l)          mode="local"; shift ;;
            --update|-u)         mode="update"; shift ;;
            --uninstall|--remove) mode="uninstall"; shift ;;
            --version)           VERSION="$2"; VERSION_NUM="${VERSION#v}"; shift 2 ;;
            --prefix)            INSTALL_PREFIX="$2"; shift 2 ;;
            --skip-sudo-check)   SKIP_SUDO_CHECK="true"; shift ;;
            --cli)               GUI_TOOLKIT="cli"; shift ;;
            --help|-h)
                cat << HELP
Usage: $0 [MODE] [OPTIONS]

Modes:
  --install, -i        Fresh install from GitHub release (default)
  --local,   -l        Install from local ./build directory
  --update,  -u        Check for and apply updates
  --uninstall          Remove $APP_DISPLAY_NAME from this system
  (no mode)            Show interactive mode selection dialog

Options:
  --version <VER>      Override release version (e.g. v1.0.3)
  --prefix  <PATH>     Installation prefix (default: /usr/local)
  --skip-sudo-check    Don't check for sudo upfront
  --cli                Force CLI mode (no GUI dialogs)
  --help, -h           Show this help

Examples:
  sudo $0              # Interactive GUI installer
  sudo $0 --install    # Fresh install, latest release
  sudo $0 --local      # Install from ./build
  sudo $0 --update     # Update to latest
  sudo $0 --uninstall  # Remove the app
HELP
                exit 0
                ;;
            *) fatal "Unknown option: $1" ;;
        esac
    done

    case "$mode" in
        fresh)     detect_os; do_fresh_install ;;
        local)     detect_os; do_local_install ;;
        update)    detect_os; do_update ;;
        uninstall) detect_os; do_uninstall ;;
        "")        interactive_mode_select ;;
    esac

    log "=== Installer finished ==="
}

main "$@"