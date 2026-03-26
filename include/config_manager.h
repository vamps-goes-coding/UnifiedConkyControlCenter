#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

struct EditorInfo {
    std::string name;
    std::string command;
    std::string icon;
};

struct WindowConfig {
    int min_width = 900;
    int min_height = 700;
    int default_width = 1000;
    int default_height = 750;
};

struct RefreshIntervals {
    int heartbeat_seconds = 10;
    int panel_status_seconds = 5;
};

struct UIConfig {
    WindowConfig window;
    RefreshIntervals refresh_intervals;
    std::vector<std::string> default_panels_to_start;
};

struct PathsConfig {
    std::string conky_wayland_dir_env = "CONKY_WAYLAND_DIR";
    std::string conky_themes_dir_env = "CONKY_THEMES_DIR";
    std::string default_conky_subpath = "conky-confs/conky-wayland";
    std::string default_themes_subpath = "themes";
    std::string display_server = "auto";  // "x11", "wayland", or "auto"
};

struct PanelDiscoveryConfig {
    std::string config_prefix = "conky-wayland-";
    std::string config_extension = ".conf";
    std::vector<std::string> excluded_files;
};

struct ThemesConfig {
    std::string file_extension = ".lua";
    std::string current_theme_file = "current.lua";
    std::string preview_helper_file = "preview_helper.lua";
    std::string categories_file = "categories.lua";
    std::string current_theme_txt = "current_theme.txt";
};

struct ApplicationConfig {
    std::string display_name = "Unified Conky Control Center";
    std::string internal_name = "UnifiedConkyControlCenter";
    std::string version = "1.0.0";
    std::string organization = "Conky";
};

class ConfigManager {
public:
    // Singleton pattern
    static ConfigManager& instance();
    
    // Load configuration from file
    bool load_config(const fs::path& config_path = "");
    
    // Getters for configuration
    const ApplicationConfig& get_application_config() const { return app_config_; }
    const PathsConfig& get_paths_config() const { return paths_config_; }
    const PanelDiscoveryConfig& get_panel_discovery_config() const { return panel_discovery_config_; }
    const UIConfig& get_ui_config() const { return ui_config_; }
    const ThemesConfig& get_themes_config() const { return themes_config_; }
    const std::vector<std::string>& get_app_themes() const { return app_themes_; }
    const std::vector<EditorInfo>& get_editors() const { return editors_; }
    
    // Helper methods
    std::string get_display_name() const { return app_config_.display_name; }
    std::string get_internal_name() const { return app_config_.internal_name; }
    std::string get_version() const { return app_config_.version; }
    std::string get_organization() const { return app_config_.organization; }
    
    fs::path get_conky_wayland_directory() const;
    fs::path get_themes_directory() const;
    
    std::string get_config_prefix() const { return panel_discovery_config_.config_prefix; }
    std::string get_config_extension() const { return panel_discovery_config_.config_extension; }
    const std::vector<std::string>& get_excluded_files() const { return panel_discovery_config_.excluded_files; }
    
    int get_min_window_width() const { return ui_config_.window.min_width; }
    int get_min_window_height() const { return ui_config_.window.min_height; }
    int get_default_window_width() const { return ui_config_.window.default_width; }
    int get_default_window_height() const { return ui_config_.window.default_height; }
    
    int get_heartbeat_interval() const { return ui_config_.refresh_intervals.heartbeat_seconds; }
    int get_panel_status_interval() const { return ui_config_.refresh_intervals.panel_status_seconds; }
    
    const std::vector<std::string>& get_default_panels() const { return ui_config_.default_panels_to_start; }
    
    std::string get_theme_extension() const { return themes_config_.file_extension; }
    std::string get_current_theme_file() const { return themes_config_.current_theme_file; }
    std::string get_preview_helper_file() const { return themes_config_.preview_helper_file; }
    std::string get_categories_file() const { return themes_config_.categories_file; }
    std::string get_current_theme_txt() const { return themes_config_.current_theme_txt; }
    
    // Display server configuration
    std::string get_display_server() const { return paths_config_.display_server; }
    void set_display_server(const std::string& display_server) { paths_config_.display_server = display_server; }
    
    // Setters for first-run setup
    void set_conky_config_path(const std::string& path);
    void set_themes_path(const std::string& path);
    bool save_config();
    
private:
    ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // Configuration data
    ApplicationConfig app_config_;
    PathsConfig paths_config_;
    PanelDiscoveryConfig panel_discovery_config_;
    UIConfig ui_config_;
    ThemesConfig themes_config_;
    std::vector<std::string> app_themes_;
    std::vector<EditorInfo> editors_;
    
    // Helper methods
    fs::path find_config_file() const;
    void set_defaults();
};