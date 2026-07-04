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
    // Get the current display server type
    static DisplayServerType get_type();
    
    // Get display server name as string
    static std::string get_type_string();
    
    // Check if running on X11
    static bool is_x11();
    
    // Check if running on Wayland
    static bool is_wayland();
    
    // Get appropriate Conky config directory based on display server
    static fs::path get_conky_config_directory();
    
    // Get appropriate Conky config prefix based on display server
    static std::string get_config_prefix();
    
    // Get appropriate Conky config extension based on display server
    static std::string get_config_extension();
    
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
    
    // Check if process is running
    static bool is_process_running(const std::string& process_name);
};