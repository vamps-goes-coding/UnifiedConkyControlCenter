#include "config_manager.h"
#include <fstream>
#include <iostream>
#include <cstdlib>

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::load_config(const fs::path& config_path) {
    fs::path path = config_path.empty() ? find_config_file() : config_path;
    
    if (path.empty() || !fs::exists(path)) {
        std::cerr << "Config file not found, using defaults" << std::endl;
        set_defaults();
        return false;
    }
    
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << path << std::endl;
            set_defaults();
            return false;
        }
        
        json config = json::parse(file);
        
        // Parse application config
        if (config.contains("application")) {
            auto& app = config["application"];
            if (app.contains("display_name")) app_config_.display_name = app["display_name"];
            if (app.contains("internal_name")) app_config_.internal_name = app["internal_name"];
            if (app.contains("version")) app_config_.version = app["version"];
            if (app.contains("organization")) app_config_.organization = app["organization"];
        }
        
        // Parse paths config
        if (config.contains("paths")) {
            auto& paths = config["paths"];
            if (paths.contains("conky_wayland_dir_env")) paths_config_.conky_wayland_dir_env = paths["conky_wayland_dir_env"];
            if (paths.contains("conky_themes_dir_env")) paths_config_.conky_themes_dir_env = paths["conky_themes_dir_env"];
            if (paths.contains("default_conky_subpath")) paths_config_.default_conky_subpath = paths["default_conky_subpath"];
            if (paths.contains("default_themes_subpath")) paths_config_.default_themes_subpath = paths["default_themes_subpath"];
        }
        
        // Parse panel discovery config
        if (config.contains("panel_discovery")) {
            auto& pd = config["panel_discovery"];
            if (pd.contains("config_prefix")) panel_discovery_config_.config_prefix = pd["config_prefix"];
            if (pd.contains("config_extension")) panel_discovery_config_.config_extension = pd["config_extension"];
            if (pd.contains("excluded_files")) {
                panel_discovery_config_.excluded_files.clear();
                for (const auto& file : pd["excluded_files"]) {
                    panel_discovery_config_.excluded_files.push_back(file);
                }
            }
        }
        
        // Parse UI config
        if (config.contains("ui")) {
            auto& ui = config["ui"];
            if (ui.contains("window")) {
                auto& win = ui["window"];
                if (win.contains("min_width")) ui_config_.window.min_width = win["min_width"];
                if (win.contains("min_height")) ui_config_.window.min_height = win["min_height"];
                if (win.contains("default_width")) ui_config_.window.default_width = win["default_width"];
                if (win.contains("default_height")) ui_config_.window.default_height = win["default_height"];
            }
            if (ui.contains("refresh_intervals")) {
                auto& ri = ui["refresh_intervals"];
                if (ri.contains("heartbeat_seconds")) ui_config_.refresh_intervals.heartbeat_seconds = ri["heartbeat_seconds"];
                if (ri.contains("panel_status_seconds")) ui_config_.refresh_intervals.panel_status_seconds = ri["panel_status_seconds"];
            }
            if (ui.contains("default_panels_to_start") && ui["default_panels_to_start"].is_array()) {
                ui_config_.default_panels_to_start.clear();
                for (const auto& panel : ui["default_panels_to_start"]) {
                    if (panel.is_string()) {
                        ui_config_.default_panels_to_start.push_back(panel.get<std::string>());
                    }
                }
            }
        }
        
        // Parse themes config
        if (config.contains("themes")) {
            auto& themes = config["themes"];
            if (themes.contains("file_extension")) themes_config_.file_extension = themes["file_extension"];
            if (themes.contains("current_theme_file")) themes_config_.current_theme_file = themes["current_theme_file"];
            if (themes.contains("preview_helper_file")) themes_config_.preview_helper_file = themes["preview_helper_file"];
            if (themes.contains("categories_file")) themes_config_.categories_file = themes["categories_file"];
            if (themes.contains("current_theme_txt")) themes_config_.current_theme_txt = themes["current_theme_txt"];
        }
        
        // Parse app themes
        if (config.contains("app_themes")) {
            app_themes_.clear();
            for (const auto& theme : config["app_themes"]) {
                app_themes_.push_back(theme);
            }
        }
        
        // Parse editors
        if (config.contains("editors")) {
            editors_.clear();
            for (const auto& editor : config["editors"]) {
                EditorInfo info;
                if (editor.contains("name")) info.name = editor["name"];
                if (editor.contains("command")) info.command = editor["command"];
                if (editor.contains("icon")) info.icon = editor["icon"];
                editors_.push_back(info);
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing config file: " << e.what() << std::endl;
        set_defaults();
        return false;
    }
}

fs::path ConfigManager::get_conky_wayland_directory() const {
    // Check environment variable first
    const char* env_dir = std::getenv(paths_config_.conky_wayland_dir_env.c_str());
    if (env_dir && strlen(env_dir) > 0) {
        return fs::path(env_dir);
    }
    
    // Fall back to default path
    const char* home = std::getenv("HOME");
    if (home) {
        return fs::path(home) / paths_config_.default_conky_subpath;
    }
    
    return fs::current_path();
}

fs::path ConfigManager::get_themes_directory() const {
    // Check environment variable first
    const char* env_dir = std::getenv(paths_config_.conky_themes_dir_env.c_str());
    if (env_dir && strlen(env_dir) > 0) {
        return fs::path(env_dir);
    }
    
    // Fall back to default path
    return get_conky_wayland_directory() / paths_config_.default_themes_subpath;
}

fs::path ConfigManager::find_config_file() const {
    // Search order:
    // 1. Environment variable
    // 2. Current directory (./config/app_config.json)
    // 3. ~/.config/UnifiedConkyControlCenter/
    // 4. /etc/UnifiedConkyControlCenter/
    // 5. Installation directory (/usr/share/...)

    const char* env_config = std::getenv("CONKY_CONTROL_CENTER_CONFIG");
    if (env_config) {
        fs::path env_path(env_config);
        if (fs::exists(env_path)) {
            return env_path;
        }
    }

    fs::path current_dir_config = fs::current_path() / "config" / "app_config.json";
    if (fs::exists(current_dir_config)) {
        return current_dir_config;
    }

    const char* home = std::getenv("HOME");
    if (home) {
        fs::path home_config = fs::path(home) / ".config" / app_config_.internal_name / "app_config.json";
        if (fs::exists(home_config)) {
            return home_config;
        }
    }

    // 4. System-wide configuration override
    fs::path etc_config = fs::path("/etc") / app_config_.internal_name / "app_config.json";
    if (fs::exists(etc_config)) {
        return etc_config;
    }

    // 5. System-wide installation default (Set by CMake GNUInstallDirs)
    fs::path share_config = fs::path("/usr/share") / app_config_.internal_name / "config" / "app_config.json";
    if (fs::exists(share_config)) {
        return share_config;
    }

    return fs::path();
}

void ConfigManager::set_defaults() {
    // All defaults are already set in the struct definitions
    app_themes_ = {"Default Light", "Dark Charcoal", "Dracula", "Nord", "Solarized Light", "Oceanic"};
    
    editors_ = {
        {"VS Code", "code", "💠"},
        {"VSCodium", "codium", "🔷"},
        {"Sublime", "subl", "📑"},
        {"Kate", "kate", "📝"},
        {"Gedit", "gedit", "📄"},
        {"Mousepad", "mousepad", "🖱️"},
        {"Neovim", "nvim", "🟩"},
        {"Vim", "vim", "⚙️"},
        {"Nano", "nano", "⌨️"}
    };
}

void ConfigManager::set_conky_config_path(const std::string& path) {
    // Store the path in environment variable format
    // This will be used by get_conky_wayland_directory()
    setenv(paths_config_.conky_wayland_dir_env.c_str(), path.c_str(), 1);
}

void ConfigManager::set_themes_path(const std::string& path) {
    // Store the path in environment variable format
    // This will be used by get_themes_directory()
    setenv(paths_config_.conky_themes_dir_env.c_str(), path.c_str(), 1);
}

bool ConfigManager::save_config() {
    // For saving, we prioritize the user's home directory or the environment override.
    // We should NOT attempt to write to /usr/share or /etc as they are usually read-only.
    fs::path config_path;
    const char* env_config = std::getenv("CONKY_CONTROL_CENTER_CONFIG");

    if (env_config) {
        config_path = fs::path(env_config);
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            config_path = fs::path(home) / ".config" / app_config_.internal_name / "app_config.json";
            fs::create_directories(config_path.parent_path());
        } else {
            return false;
        }
    }

    try {
        // Create JSON object with current configuration
        json config;
        
        // Application config
        config["application"] = {
            {"display_name", app_config_.display_name},
            {"internal_name", app_config_.internal_name},
            {"version", app_config_.version},
            {"organization", app_config_.organization}
        };
        
        // Paths config
        config["paths"] = {
            {"conky_wayland_dir_env", paths_config_.conky_wayland_dir_env},
            {"conky_themes_dir_env", paths_config_.conky_themes_dir_env},
            {"default_conky_subpath", paths_config_.default_conky_subpath},
            {"default_themes_subpath", paths_config_.default_themes_subpath}
        };
        
        // Panel discovery config
        config["panel_discovery"] = {
            {"config_prefix", panel_discovery_config_.config_prefix},
            {"config_extension", panel_discovery_config_.config_extension},
            {"excluded_files", panel_discovery_config_.excluded_files}
        };
        
        // UI config
        config["ui"] = {
            {"window", {
                {"min_width", ui_config_.window.min_width},
                {"min_height", ui_config_.window.min_height},
                {"default_width", ui_config_.window.default_width},
                {"default_height", ui_config_.window.default_height}
            }},
            {"refresh_intervals", {
                {"heartbeat_seconds", ui_config_.refresh_intervals.heartbeat_seconds},
                {"panel_status_seconds", ui_config_.refresh_intervals.panel_status_seconds}
            }},
            {"default_panels_to_start", ui_config_.default_panels_to_start}
        };
        
        // Themes config
        config["themes"] = {
            {"file_extension", themes_config_.file_extension},
            {"current_theme_file", themes_config_.current_theme_file},
            {"preview_helper_file", themes_config_.preview_helper_file},
            {"categories_file", themes_config_.categories_file},
            {"current_theme_txt", themes_config_.current_theme_txt}
        };
        
        // App themes
        config["app_themes"] = app_themes_;
        
        // Editors
        json editors_json = json::array();
        for (const auto& editor : editors_) {
            editors_json.push_back({
                {"name", editor.name},
                {"command", editor.command},
                {"icon", editor.icon}
            });
        }
        config["editors"] = editors_json;
        
        // Write to file
        std::ofstream file(config_path);
        if (!file.is_open()) {
            return false;
        }
        
        file << config.dump(4); // Pretty print with 4 spaces
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving config: " << e.what() << std::endl;
        return false;
    }
}
