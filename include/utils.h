#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

class Utils {
public:
    // Path utilities
    static fs::path normalize_path(const std::string& path);
    static std::string get_display_name(const std::string& panel_id);
    
    // Directory discovery
    static std::vector<std::string> discover_panels();
    static fs::path get_conky_config_path(const std::string& panel_name);
    
    // File operations
    static bool file_exists(const fs::path& path);
    static std::string read_file(const fs::path& path);
    static bool write_file(const fs::path& path, const std::string& content);
    static bool copy_file(const fs::path& source, const fs::path& destination);
    static bool delete_file(const fs::path& path);
    
    // Directory operations
    static bool create_directory(const fs::path& path);
    static bool directory_exists(const fs::path& path);
    static std::vector<fs::path> list_files(const fs::path& directory, const std::string& pattern = "*");
    
    // Time utilities
    static std::chrono::steady_clock::time_point get_current_time();
    static double get_time_difference(const std::chrono::steady_clock::time_point& start);
    
    // String utilities
    static std::vector<std::string> split_string(const std::string& str, char delimiter);
    static std::string trim(const std::string& str);
    static std::string to_lower(const std::string& str);
    static std::string to_upper(const std::string& str);
    
    // Environment utilities
    static std::string get_environment_variable(const std::string& name);
    static bool set_environment_variable(const std::string& name, const std::string& value);
    
    /// Conky Wayland install root (panel .conf files, per-panel *-theme.lua, scripts).
    /// Uses ConfigManager for paths.
    static fs::path conky_wayland_directory();
    /// Theme library under conky-wayland (subfolders + loose .lua files).
    static fs::path themes_directory();

    static const fs::path HOME;
    
    // Qt-style interface for main.cpp
    std::string getConkyConfigPath();
    std::string getThemePath();
    bool fileExists(const std::string& filePath);
    std::vector<std::string> listFiles(const std::string& directoryPath);
    bool createDirectory(const std::string& directoryPath);
    
private:
    static fs::path get_executable_directory();
};