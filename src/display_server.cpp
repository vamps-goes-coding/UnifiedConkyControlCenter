#include "display_server.h"
#include "logger.h"
#include "config_manager.h"

#include <cstdlib>
#include <cctype>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>

#ifdef __linux__
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#endif

namespace {
// File-scope detection cache so refresh() can invalidate it.
DisplayServerType g_cached_type = DisplayServerType::Unknown;
bool g_cache_valid = false;

void invalidate_cache() {
    g_cache_valid = false;
}
}

DisplayServerType DisplayServer::get_type() {
    if (!g_cache_valid) {
        g_cached_type = detect_from_environment();
        if (g_cached_type == DisplayServerType::Unknown) {
            g_cached_type = detect_from_processes();
        }
        g_cache_valid = true;

        LOG_INFO("Detected display server: " + get_type_string());
    }

    return g_cached_type;
}

void DisplayServer::refresh() {
    invalidate_cache();
    LOG_INFO("Display server detection cache cleared; will re-detect on next query");
}

std::string DisplayServer::get_type_string() {
    switch (get_type()) {
        case DisplayServerType::X11:
            return "X11";
        case DisplayServerType::Wayland:
            return "Wayland";
        default:
            return "Unknown";
    }
}

bool DisplayServer::is_x11() {
    return get_type() == DisplayServerType::X11;
}

bool DisplayServer::is_wayland() {
    return get_type() == DisplayServerType::Wayland;
}

std::string DisplayServer::get_display_variable() {
    const char* display = std::getenv("DISPLAY");
    return display ? display : "";
}

std::string DisplayServer::get_wayland_display() {
    const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
    return wayland_display ? wayland_display : "";
}

bool DisplayServer::wayland_socket_exists(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    fs::path sock(name);
    if (sock.is_absolute()) {
        return fs::exists(sock);
    }
    // Relative names live under $XDG_RUNTIME_DIR (e.g. "wayland-0").
    const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR");
    if (runtime_dir && *runtime_dir) {
        if (fs::exists(fs::path(runtime_dir) / sock)) {
            return true;
        }
    }
    // Fall back to a relative lookup in case the caller already chdir'd
    // into the runtime dir (unlikely, but harmless).
    return fs::exists(sock);
}

bool DisplayServer::x11_socket_exists(const std::string& display) {
    if (display.empty()) {
        return false;
    }
    // DISPLAY forms: ":0", ":0.0", "hostname:0", "/tmp/...". Extract the
    // display number after the last ':' and check /tmp/.X11-unix/X<n>.
    std::string::size_type colon = display.rfind(':');
    if (colon == std::string::npos) {
        return false;
    }
    std::string rest = display.substr(colon + 1);
    std::string::size_type dot = rest.find('.');
    std::string number = (dot == std::string::npos) ? rest : rest.substr(0, dot);
    if (number.empty() ||
        !std::all_of(number.begin(), number.end(), ::isdigit)) {
        return false;
    }
    return fs::exists(fs::path("/tmp/.X11-unix") / ("X" + number));
}

bool DisplayServer::is_display_server_available(DisplayServerType type) {
    switch (type) {
        case DisplayServerType::X11:
            // $DISPLAY is often set even without a real X server (notably
            // under XWayland), so require the socket to exist too.
            return x11_socket_exists(get_display_variable());
        case DisplayServerType::Wayland:
            return wayland_socket_exists(get_wayland_display());
        default:
            return false;
    }
}

DisplayServerType DisplayServer::detect_from_environment() {
    // Check WAYLAND_DISPLAY first (most reliable for Wayland)
    if (!get_wayland_display().empty()) {
        return DisplayServerType::Wayland;
    }
    
    // Check DISPLAY for X11
    if (!get_display_variable().empty()) {
        return DisplayServerType::X11;
    }
    
    // Check XDG_SESSION_TYPE
    const char* xdg_session = std::getenv("XDG_SESSION_TYPE");
    if (xdg_session) {
        std::string session_type = xdg_session;
        std::transform(session_type.begin(), session_type.end(), session_type.begin(), ::tolower);
        
        if (session_type == "wayland") {
            return DisplayServerType::Wayland;
        } else if (session_type == "x11") {
            return DisplayServerType::X11;
        }
    }
    
    return DisplayServerType::Unknown;
}

DisplayServerType DisplayServer::detect_from_processes() {
    // Check for Wayland compositor processes.
    // NOTE: Xwayland is deliberately in this list — it only ever runs as
    // a nested X server *under* a Wayland compositor, so its presence
    // means the session is Wayland-based (critical for hybrid desktops
    // where WAYLAND_DISPLAY may be unset in some contexts).
    // NOTE: qtile is NOT here — it defaults to X11 (its Wayland backend
    // is experimental), and X11 qtile sessions must not misdetect.
    std::vector<std::string> wayland_compositors = {
        "gnome-shell",
        "kwin_wayland",
        "sway",
        "weston",
        "hyprland",
        "river",
        "labwc",
        "cage",
        "dwl",
        "gamescope",
        "newm",
        "niri",
        "wayfire",
        "Xwayland"
    };
    
    for (const auto& compositor : wayland_compositors) {
        if (is_process_running(compositor)) {
            return DisplayServerType::Wayland;
        }
    }
    
    // Check for X11 server
    if (is_process_running("Xorg") || is_process_running("X")) {
        return DisplayServerType::X11;
    }
    
    return DisplayServerType::Unknown;
}

bool DisplayServer::is_process_running(const std::string& process_name) {
#ifdef __linux__
    DIR* dir = opendir("/proc");
    if (!dir) {
        return false;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Check if directory name is a number (PID)
        if (!std::all_of(entry->d_name, entry->d_name + strlen(entry->d_name), ::isdigit)) {
            continue;
        }
        
        std::string comm_path = "/proc/" + std::string(entry->d_name) + "/comm";
        std::ifstream comm_file(comm_path);
        
        if (comm_file.is_open()) {
            std::string comm;
            std::getline(comm_file, comm);
            comm_file.close();
            
            if (comm == process_name) {
                closedir(dir);
                return true;
            }
        }
    }
    
    closedir(dir);
#endif
    
    return false;
}