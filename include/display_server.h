#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

enum class DisplayServerType {
    X11,
    Wayland,
    Unknown
};

class DisplayServer {
public:
    // Get the current display server type (cached; call refresh() to re-detect)
    static DisplayServerType get_type();

    // Clear the cached detection so the next get_type() call re-detects
    // from the live environment. Call this when the session may have
    // changed (e.g. after the user switches the display-server override).
    static void refresh();
    // Get display server name as string
    static std::string get_type_string();
    
    // Check if running on X11
    static bool is_x11();
    
    // Check if running on Wayland
    static bool is_wayland();

    // Get display server specific environment variables
    static std::string get_display_variable();
    
    // Get Wayland specific variables
    static std::string get_wayland_display();
    
    // Check if a specific display server is available
    static bool is_display_server_available(DisplayServerType type);
    
private:
    // Detect display server from environment variables
    static DisplayServerType detect_from_environment();

    // Detect display server from running processes
    static DisplayServerType detect_from_processes();

    // Validate that the socket behind an env var actually exists
    // (guards against stale XWayland-provided variables)
    static bool wayland_socket_exists(const std::string& name);
    static bool x11_socket_exists(const std::string& display);

    // Check if process is running
    static bool is_process_running(const std::string& process_name);
};