#include "display_server.h"
#include "logger.h"
#include "config_manager.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>

#ifdef __linux__
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#endif

DisplayServerType DisplayServer::get_type() {
    static DisplayServerType cached_type = DisplayServerType::Unknown;
    static bool cached = false;
    
    if (!cached) {
        cached_type = detect_from_environment();
        if (cached_type == DisplayServerType::Unknown) {
            cached_type = detect_from_processes();
        }
        cached = true;
        
        LOG_INFO("Detected display server: " + get_type_string());
    }
    
    return cached_type;
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

fs::path DisplayServer::get_conky_config_directory() {
    auto& config = ConfigManager::instance();
    fs::path base_dir = config.get_conky_wayland_directory();
    
    // Check for display server specific subdirectories
    if (is_wayland()) {
        fs::path wayland_dir = base_dir / "conky-wayland";
        if (fs::exists(wayland_dir)) {
            return wayland_dir;
        }
    } else if (is_x11()) {
        fs::path x11_dir = base_dir / "conky-x11";
        if (fs::exists(x11_dir)) {
            return x11_dir;
        }
    }
    
    // Fall back to base directory
    return base_dir;
}

std::string DisplayServer::get_config_prefix() {
    if (is_wayland()) {
        return "conky-wayland-";
    } else if (is_x11()) {
        return "conky-x11-";
    }
    
    // Default prefix
    return "conky-";
}

std::string DisplayServer::get_config_extension() {
    return ".conf";
}

std::string DisplayServer::get_display_variable() {
    const char* display = std::getenv("DISPLAY");
    return display ? display : "";
}

std::string DisplayServer::get_wayland_display() {
    const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
    return wayland_display ? wayland_display : "";
}

bool DisplayServer::is_display_server_available(DisplayServerType type) {
    switch (type) {
        case DisplayServerType::X11:
            return !get_display_variable().empty();
        case DisplayServerType::Wayland:
            return !get_wayland_display().empty();
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
    // Check for Wayland compositor processes
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
        "qtile",
        "wayfire"
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