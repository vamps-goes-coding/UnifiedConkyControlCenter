#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <nlohmann/json.hpp>

#include "placement.h"

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

struct DisplayServerConfig {
    // Subdirectory name under the conky root for this server's panel .conf files
    std::string config_subdir = "conky-wayland";
    // Subdirectory name under the conky root for this server's theme .lua files
    std::string themes_subdir = "themes";
    // File prefix used to name per-panel config files (e.g. "conky-wayland-")
    std::string config_prefix = "conky-wayland-";
    // File extension for panel config files (e.g. ".conf")
    std::string config_extension = ".conf";
    // Conky binary used to launch this server's panels. Distros may ship
    // separate X11 and Wayland builds (e.g. "conky" vs "conky-wayland").
    std::string conky_binary = "conky";
    // Extra argv placed before "-c <config>" when launching a panel.
    // Defaults preserve the historical flags: quiet + explicit own-window.
    std::vector<std::string> conky_extra_args = {"-q", "-o"};

    // Replace empty fields with the compiled defaults. Guards against
    // preferences files saved with blank rows — an empty prefix/extension
    // would otherwise make panel discovery match nothing and hide every
    // panel in the Start/Stop list. Defaults are per-server because the
    // struct's inline values are Wayland-flavored.
    void normalize(const std::string& server_key = "wayland") {
        DisplayServerConfig defaults;
        if (server_key == "x11") {
            defaults.config_subdir = "conky-x11";
            defaults.config_prefix = "conky-x11-";
        }
        if (config_subdir.empty()) config_subdir = defaults.config_subdir;
        if (themes_subdir.empty()) themes_subdir = defaults.themes_subdir;
        if (config_prefix.empty()) config_prefix = defaults.config_prefix;
        if (config_extension.empty()) config_extension = defaults.config_extension;
        if (conky_binary.empty()) conky_binary = defaults.conky_binary;
        if (conky_extra_args.empty()) conky_extra_args = defaults.conky_extra_args;
    }
};

struct PathsConfig {
    std::string conky_wayland_dir_env = "CONKY_WAYLAND_DIR";
    std::string conky_themes_dir_env = "CONKY_THEMES_DIR";
    // Default fallback path under $HOME when no env/override is set.
    // This is the conky ROOT (server-specific subdirs are appended from
    // the active DisplayServerConfig), e.g. "~/conky-confs" + "conky-x11".
    std::string default_conky_subpath = "conky-confs";
    std::string default_themes_subpath = "themes";
    // Explicit conky root persisted from Preferences / first-run setup.
    // Empty = unused (fall back to $HOME + default_conky_subpath).
    // Stored as the ROOT (never a server subdir) so display-server
    // switching keeps resolving per-server folders underneath it.
    std::string conky_root_override;
    // Explicit themes root persisted from first-run setup. Empty = unused
    // (fall back to <conky dir>/default_themes_subpath).
    std::string themes_root_override;
    // "x11", "wayland", or "auto"
    std::string display_server = "auto";
    // Per-display-server overrides. Keys are "x11" and "wayland".
    // When a server is missing from this map, DisplayServerConfig defaults are used.
    std::map<std::string, DisplayServerConfig> display_servers;
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
    ApplicationConfig& get_application_config() { return app_config_; }
    const PathsConfig& get_paths_config() const { return paths_config_; }
    const PanelDiscoveryConfig& get_panel_discovery_config() const { return panel_discovery_config_; }
    PanelDiscoveryConfig& get_panel_discovery_config() { return panel_discovery_config_; }
    const UIConfig& get_ui_config() const { return ui_config_; }
    UIConfig& get_ui_config() { return ui_config_; }
    const ThemesConfig& get_themes_config() const { return themes_config_; }
    ThemesConfig& get_themes_config() { return themes_config_; }
    const std::vector<std::string>& get_app_themes() const { return app_themes_; }
    const std::vector<EditorInfo>& get_editors() const { return editors_; }
    std::vector<EditorInfo>& get_editors() { return editors_; }
    
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
    void set_display_server(const std::string& display_server);

    // Resolve the display-server key ("x11" or "wayland") honoring the
    // "auto" setting by querying the live display server.
    std::string get_active_display_server_key() const;

    // Get the DisplayServerConfig for the active display server. Falls back to
    // defaults when the server has no explicit entry in the config map.
    const DisplayServerConfig& get_active_display_server_config() const;

    // Convenience accessors that resolve against the active server.
    std::string get_active_config_subdir() const;
    std::string get_active_themes_subdir() const;
    std::string get_active_config_prefix() const;
    std::string get_active_config_extension() const;
    std::string get_active_conky_binary() const;
    std::vector<std::string> get_active_conky_extra_args() const;

    // Get all known display server keys (for UI population).
    std::vector<std::string> get_display_server_keys() const;

    // Get or create the DisplayServerConfig entry for a given server key.
    DisplayServerConfig& get_display_server_config(const std::string& key);

    // Setters for first-run setup
    void set_conky_config_path(const std::string& path);
    void set_themes_path(const std::string& path);
    bool save_config();

    // Persist ONLY the display-server selection to disk (surgical update of
    // paths.display_server), so a toolbar/menu switch survives restarts
    // without rewriting unrelated in-memory state. Returns false when no
    // config file location is available.
    bool save_display_server_selection();

    // Hardware preferences chosen in Preferences > Hardware (default GPU,
    // primary NIC, default sound card) so panel configs can reference them.
    const std::map<std::string, std::string>& get_hardware_prefs() const { return hardware_prefs_; }
    std::string get_hardware_pref(const std::string& key, const std::string& default_value = "") const;
    void set_hardware_pref(const std::string& key, const std::string& value);

    // Per-panel placement (Placement tab). Coordinates are desktop-space
    // whatever the display server does with them; see placement.h.
    const std::map<std::string, PanelPlacement>& get_panel_placements() const { return panel_placement_; }
    bool has_placement(const std::string& panel) const;
    // Default-constructed entry when the panel has none, so callers can use
    // the result without checking first.
    PanelPlacement get_placement(const std::string& panel) const;
    void set_placement(const std::string& panel, const PanelPlacement& placement);
    void clear_placement(const std::string& panel);
    
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
    std::map<std::string, std::string> hardware_prefs_;
    std::map<std::string, PanelPlacement> panel_placement_;
    
    // Helper methods
    fs::path find_config_file() const;
    void set_defaults();
};