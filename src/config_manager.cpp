#include "config_manager.h"
#include "display_server.h"
#include "logger.h"
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
        LOG_INFO("Config file not found, using defaults");
        set_defaults();
        return false;
    }
    
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open config file: " + path.string());
            set_defaults();
            return false;
        }
        
        json config = json::parse(file);
        
        // Parse application config
        if (config.contains("application") && config["application"].is_object()) {
            auto& app = config["application"];
            app_config_.display_name = app.value("display_name", app_config_.display_name);
            app_config_.internal_name = app.value("internal_name", app_config_.internal_name);
            app_config_.organization = app.value("organization", app_config_.organization);
        }
        
        // Parse paths config
        if (config.contains("paths") && config["paths"].is_object()) {
            auto& paths = config["paths"];
            paths_config_.conky_wayland_dir_env = paths.value("conky_wayland_dir_env", paths_config_.conky_wayland_dir_env);
            paths_config_.conky_themes_dir_env = paths.value("conky_themes_dir_env", paths_config_.conky_themes_dir_env);
            paths_config_.default_conky_subpath = paths.value("default_conky_subpath", paths_config_.default_conky_subpath);
            paths_config_.default_themes_subpath = paths.value("default_themes_subpath", paths_config_.default_themes_subpath);
            paths_config_.conky_root_override = paths.value("conky_root_override", paths_config_.conky_root_override);
            paths_config_.themes_root_override = paths.value("themes_root_override", paths_config_.themes_root_override);
            paths_config_.display_server = paths.value("display_server", paths_config_.display_server);

            // Parse per-display-server overrides (optional).
            if (paths.contains("display_servers") && paths["display_servers"].is_object()) {
                paths_config_.display_servers.clear();
                for (const auto& [key, val] : paths["display_servers"].items()) {
                    if (!val.is_object()) continue;
                    DisplayServerConfig dsc;
                    dsc.config_subdir = val.value("config_subdir", dsc.config_subdir);
                    dsc.themes_subdir = val.value("themes_subdir", dsc.themes_subdir);
                    dsc.config_prefix = val.value("config_prefix", dsc.config_prefix);
                    dsc.config_extension = val.value("config_extension", dsc.config_extension);
                    dsc.conky_binary = val.value("conky_binary", dsc.conky_binary);
                    if (val.contains("conky_extra_args") && val["conky_extra_args"].is_array()) {
                        dsc.conky_extra_args =
                            val["conky_extra_args"].get<std::vector<std::string>>();
                    }
                    // Blank rows in the file must not stick: an empty
                    // prefix/extension would hide every panel from discovery.
                    dsc.normalize(key);
                    paths_config_.display_servers[key] = dsc;
                }
            }
        }
        
        // Parse panel discovery config
        if (config.contains("panel_discovery") && config["panel_discovery"].is_object()) {
            auto& pd = config["panel_discovery"];
            panel_discovery_config_.config_prefix = pd.value("config_prefix", panel_discovery_config_.config_prefix);
            panel_discovery_config_.config_extension = pd.value("config_extension", panel_discovery_config_.config_extension);
            if (pd.contains("excluded_files") && pd["excluded_files"].is_array()) {
                panel_discovery_config_.excluded_files = pd["excluded_files"].get<std::vector<std::string>>();
            }
        }
        
        // Parse UI config
        if (config.contains("ui") && config["ui"].is_object()) {
            auto& ui = config["ui"];
            if (ui.contains("window") && ui["window"].is_object()) {
                auto& win = ui["window"];
                ui_config_.window.min_width = win.value("min_width", ui_config_.window.min_width);
                ui_config_.window.min_height = win.value("min_height", ui_config_.window.min_height);
                ui_config_.window.default_width = win.value("default_width", ui_config_.window.default_width);
                ui_config_.window.default_height = win.value("default_height", ui_config_.window.default_height);
            }
            if (ui.contains("refresh_intervals") && ui["refresh_intervals"].is_object()) {
                auto& ri = ui["refresh_intervals"];
                ui_config_.refresh_intervals.heartbeat_seconds = ri.value("heartbeat_seconds", ui_config_.refresh_intervals.heartbeat_seconds);
                ui_config_.refresh_intervals.panel_status_seconds = ri.value("panel_status_seconds", ui_config_.refresh_intervals.panel_status_seconds);
            }
            if (ui.contains("default_panels_to_start") && ui["default_panels_to_start"].is_array()) {
                ui_config_.default_panels_to_start = ui["default_panels_to_start"].get<std::vector<std::string>>();
            }
        }
        
        // Parse themes config
        if (config.contains("themes") && config["themes"].is_object()) {
            auto& themes = config["themes"];
            themes_config_.file_extension = themes.value("file_extension", themes_config_.file_extension);
            themes_config_.current_theme_file = themes.value("current_theme_file", themes_config_.current_theme_file);
            themes_config_.preview_helper_file = themes.value("preview_helper_file", themes_config_.preview_helper_file);
            themes_config_.categories_file = themes.value("categories_file", themes_config_.categories_file);
            themes_config_.current_theme_txt = themes.value("current_theme_txt", themes_config_.current_theme_txt);
        }
        
        // Parse hardware config
        if (config.contains("hardware") && config["hardware"].is_object()) {
            hardware_prefs_.clear();
            for (const auto& [key, value] : config["hardware"].items()) {
                if (value.is_string()) {
                    hardware_prefs_[key] = value.get<std::string>();
                }
            }
        }

        // Parse panel placement (Placement tab)
        if (config.contains("panel_placement") && config["panel_placement"].is_object()) {
            panel_placement_.clear();
            const auto& placement = config["panel_placement"];
            if (placement.contains("panels") && placement["panels"].is_object()) {
                for (const auto& [panel, value] : placement["panels"].items()) {
                    if (!value.is_object()) continue;
                    PanelPlacement p;
                    p.output = value.value("output", p.output);
                    p.x = value.value("x", p.x);
                    p.y = value.value("y", p.y);
                    p.launch_env = value.value("launch_env", p.launch_env);
                    panel_placement_[panel] = p;
                }
            }
        }

        // Parse app themes
        if (config.contains("app_themes") && config["app_themes"].is_array()) {
            app_themes_ = config["app_themes"].get<std::vector<std::string>>();
        }
        
        // Parse editors
        if (config.contains("editors") && config["editors"].is_array()) {
            editors_.clear();
            for (const auto& editor : config["editors"]) {
                if (editor.is_object()) {
                    EditorInfo info;
                    info.name = editor.value("name", "");
                    info.command = editor.value("command", "");
                    info.icon = editor.value("icon", "");
                    editors_.push_back(info);
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing config file: " + std::string(e.what()));
        set_defaults();
        return false;
    }
}

fs::path ConfigManager::get_conky_wayland_directory() const {
    // Check environment variable first (explicit expert override; bypasses
    // server resolution by design — use conky_root_override instead if you
    // want switching to keep working).
    const char* env_dir = std::getenv(paths_config_.conky_wayland_dir_env.c_str());
    if (env_dir && strlen(env_dir) > 0) {
        return fs::path(env_dir);
    }

    // Persisted root from Preferences / first-run setup.
    if (!paths_config_.conky_root_override.empty()) {
        fs::path base(paths_config_.conky_root_override);
        std::string subdir = get_active_config_subdir();
        if (!subdir.empty()) {
            fs::path subdir_path(subdir);
            fs::path server_dir =
                subdir_path.is_absolute() ? subdir_path : base / subdir_path;
            if (fs::exists(server_dir)) {
                return server_dir;
            }
        }
        return base;
    }

    // Fall back to default path
    const char* home = std::getenv("HOME");
    if (home) {
        fs::path base = fs::path(home) / paths_config_.default_conky_subpath;

        // Resolve to the active display server's subdirectory so the panel
        // discovery prefix and the directory stay in sync. An absolute
        // subdir (e.g. user-browsed to /home/u/conky-confs/conky-x11) is
        // used as-is instead of being appended to the base.
        std::string subdir = get_active_config_subdir();
        if (!subdir.empty()) {
            fs::path subdir_path(subdir);
            fs::path server_dir =
                subdir_path.is_absolute() ? subdir_path : base / subdir_path;
            if (fs::exists(server_dir)) {
                return server_dir;
            }
            return base;
        }
        return base;
    }

    return fs::current_path();
}

fs::path ConfigManager::get_themes_directory() const {
    // Check environment variable first
    const char* env_dir = std::getenv(paths_config_.conky_themes_dir_env.c_str());
    if (env_dir && strlen(env_dir) > 0) {
        return fs::path(env_dir);
    }

    // Then the themes root persisted by First Run Setup / set_themes_path().
    if (!paths_config_.themes_root_override.empty()) {
        return fs::path(paths_config_.themes_root_override);
    }

    // Fall back to default path
    return get_conky_wayland_directory() / paths_config_.default_themes_subpath;
}

std::string ConfigManager::get_active_display_server_key() const {
    if (paths_config_.display_server == "x11" || paths_config_.display_server == "wayland") {
        return paths_config_.display_server;
    }

    // "auto" (or anything else) -> resolve live. Deliberately returns
    // "unknown" when detection fails instead of guessing "x11": callers
    // that need a working default use get_active_display_server_config()
    // (safe fallback), while the UI surfaces "unknown" to the user.
    DisplayServerType type = DisplayServer::get_type();
    if (type == DisplayServerType::Wayland) {
        return "wayland";
    } else if (type == DisplayServerType::X11) {
        return "x11";
    }
    return "unknown";
}

void ConfigManager::set_display_server(const std::string& display_server) {
    if (paths_config_.display_server == display_server) {
        return;
    }
    paths_config_.display_server = display_server;
    // The active server changed (or may have), so drop the cached
    // auto-detection — the next "auto" resolution re-detects live.
    DisplayServer::refresh();
}

const DisplayServerConfig& ConfigManager::get_active_display_server_config() const {
    std::string key = get_active_display_server_key();
    auto it = paths_config_.display_servers.find(key);
    if (it != paths_config_.display_servers.end()) {
        return it->second;
    }

    // Fallback to static per-server defaults. NOTE: the struct's inline
    // defaults are Wayland-flavored, so the X11 fallback must override the
    // subdir/prefix explicitly — otherwise an unconfigured "x11" key would
    // resolve to Wayland paths. An "unknown" key intentionally lands on the
    // X11 fallback as a safe operational default; the "unknown" state itself
    // stays visible via get_active_display_server_key() so the UI can prompt
    // the user instead of silently guessing.
    static const DisplayServerConfig x11_defaults = [] {
        DisplayServerConfig d;
        d.config_subdir = "conky-x11";
        d.config_prefix = "conky-x11-";
        return d;
    }();
    static const DisplayServerConfig wayland_defaults;
    return (key == "wayland") ? wayland_defaults : x11_defaults;
}

std::string ConfigManager::get_active_config_subdir() const {
    return get_active_display_server_config().config_subdir;
}

std::string ConfigManager::get_active_themes_subdir() const {
    return get_active_display_server_config().themes_subdir;
}

std::string ConfigManager::get_active_config_prefix() const {
    return get_active_display_server_config().config_prefix;
}

std::string ConfigManager::get_active_config_extension() const {
    return get_active_display_server_config().config_extension;
}

std::string ConfigManager::get_active_conky_binary() const {
    return get_active_display_server_config().conky_binary;
}

std::vector<std::string> ConfigManager::get_active_conky_extra_args() const {
    return get_active_display_server_config().conky_extra_args;
}

std::vector<std::string> ConfigManager::get_display_server_keys() const {
    std::vector<std::string> keys = {"x11", "wayland"};
    return keys;
}

DisplayServerConfig& ConfigManager::get_display_server_config(const std::string& key) {
    auto it = paths_config_.display_servers.find(key);
    if (it != paths_config_.display_servers.end()) {
        return it->second;
    }
    return paths_config_.display_servers[key];  // inserts default-constructed entry
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
    // Reset everything, not just the two list members. load_config() calls
    // this on a missing/unreadable file, and previously the path, discovery
    // and hardware members kept whatever the previous load had left behind -
    // so a "defaults" load was really "defaults plus stale state".
    app_config_ = ApplicationConfig{};
    paths_config_ = PathsConfig{};
    panel_discovery_config_ = PanelDiscoveryConfig{};
    ui_config_ = UIConfig{};
    themes_config_ = ThemesConfig{};
    hardware_prefs_.clear();
    panel_placement_.clear();

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
    // Persist the pick as the conky ROOT so display-server switching keeps
    // resolving per-server folders underneath it. If the user picked a
    // server subdirectory itself (e.g. ".../conky-confs/conky-x11"), store
    // its parent — storing the subdir directly would break switching.
    fs::path picked(path);
    std::string leaf = picked.filename().string();
    bool is_server_subdir = (leaf == "conky-x11" || leaf == "conky-wayland");
    if (!is_server_subdir) {
        for (const auto& [key, dsc] : paths_config_.display_servers) {
            if (leaf == dsc.config_subdir) {
                is_server_subdir = true;
                break;
            }
        }
    }
    if (is_server_subdir && picked.has_parent_path()) {
        paths_config_.conky_root_override = picked.parent_path().string();
    } else if (!path.empty()) {
        paths_config_.conky_root_override = path;
    }
    // Clear the legacy session-only env override so the persisted root wins.
    unsetenv(paths_config_.conky_wayland_dir_env.c_str());
}

void ConfigManager::set_themes_path(const std::string& path) {
    // Persist the choice. This used to be a session-only setenv(), so a themes
    // folder picked in First Run Setup was silently forgotten on the next
    // launch and get_themes_directory() fell back to <conky dir>/themes.
    //
    // A value equal to the derived default is stored as "empty" so themes keep
    // following the Conky folder: moving the Conky folder in Preferences still
    // moves the themes folder with it.
    auto strip = [](std::string s) {
        while (s.size() > 1 && (s.back() == '/' || s.back() == '\\')) s.pop_back();
        return s;
    };
    const fs::path derived = get_conky_wayland_directory() / paths_config_.default_themes_subpath;
    const bool is_default = strip(fs::path(path).string()) == strip(derived.string());

    paths_config_.themes_root_override = is_default ? std::string() : path;

    if (is_default) {
        unsetenv(paths_config_.conky_themes_dir_env.c_str());
    } else {
        setenv(paths_config_.conky_themes_dir_env.c_str(), path.c_str(), 1);
    }
}

std::string ConfigManager::get_hardware_pref(const std::string& key,
                                             const std::string& default_value) const {
    auto it = hardware_prefs_.find(key);
    return it != hardware_prefs_.end() ? it->second : default_value;
}

void ConfigManager::set_hardware_pref(const std::string& key, const std::string& value) {
    if (value.empty()) {
        hardware_prefs_.erase(key);
    } else {
        hardware_prefs_[key] = value;
    }
}

bool ConfigManager::has_placement(const std::string& panel) const {
    return panel_placement_.count(panel) > 0;
}

PanelPlacement ConfigManager::get_placement(const std::string& panel) const {
    auto it = panel_placement_.find(panel);
    return it != panel_placement_.end() ? it->second : PanelPlacement{};
}

void ConfigManager::set_placement(const std::string& panel,
                                  const PanelPlacement& placement) {
    panel_placement_[panel] = placement;
}

void ConfigManager::clear_placement(const std::string& panel) {
    panel_placement_.erase(panel);
}

bool ConfigManager::save_display_server_selection() {
    try {
        // Resolve the file we would load from; fall back to the standard
        // per-user location so a first switch still has somewhere to land.
        fs::path config_path = find_config_file();
        if (config_path.empty()) {
            const char* home = std::getenv("HOME");
            if (!home) {
                return false;
            }
            config_path = fs::path(home) / ".config" / app_config_.internal_name / "app_config.json";
            fs::create_directories(config_path.parent_path());
        }

        json config;
        if (fs::exists(config_path)) {
            std::ifstream in(config_path);
            if (!in.is_open()) {
                return false;
            }
            config = json::parse(in);
        }
        if (!config.is_object()) {
            config = json::object();
        }
        if (!config.contains("paths") || !config["paths"].is_object()) {
            config["paths"] = json::object();
        }
        config["paths"]["display_server"] = paths_config_.display_server;

        fs::path tmp_path = config_path;
        tmp_path += ".tmp";
        {
            std::ofstream out(tmp_path);
            if (!out.is_open()) {
                return false;
            }
            out << config.dump(4);
        }
        fs::rename(tmp_path, config_path);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving display server selection: " + std::string(e.what()));
        return false;
    }
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
            {"organization", app_config_.organization}
        };
        
        // Paths config
        config["paths"] = {
            {"conky_wayland_dir_env", paths_config_.conky_wayland_dir_env},
            {"conky_themes_dir_env", paths_config_.conky_themes_dir_env},
            {"default_conky_subpath", paths_config_.default_conky_subpath},
            {"default_themes_subpath", paths_config_.default_themes_subpath},
            {"conky_root_override", paths_config_.conky_root_override},
            {"themes_root_override", paths_config_.themes_root_override},
            {"display_server", paths_config_.display_server}
        };

        // Per-display-server overrides (optional)
        if (!paths_config_.display_servers.empty()) {
            json ds_map = json::object();
            for (const auto& [key, dsc] : paths_config_.display_servers) {
                ds_map[key] = {
                    {"config_subdir", dsc.config_subdir},
                    {"themes_subdir", dsc.themes_subdir},
                    {"config_prefix", dsc.config_prefix},
                    {"config_extension", dsc.config_extension},
                    {"conky_binary", dsc.conky_binary},
                    {"conky_extra_args", dsc.conky_extra_args}
                };
            }
            config["paths"]["display_servers"] = ds_map;
        }
        
        // Panel discovery config
        config["panel_discovery"] = {
            {"config_prefix", panel_discovery_config_.config_prefix},
            {"config_extension", panel_discovery_config_.config_extension},
            {"excluded_files", panel_discovery_config_.excluded_files}
        };

        // Hardware preferences (Preferences > Hardware)
        if (!hardware_prefs_.empty()) {
            json hw = json::object();
            for (const auto& [key, value] : hardware_prefs_) {
                hw[key] = value;
            }
            config["hardware"] = hw;
        }

        // Per-panel placement (Placement tab)
        if (!panel_placement_.empty()) {
            json panels = json::object();
            for (const auto& [panel, p] : panel_placement_) {
                panels[panel] = {
                    {"output", p.output},
                    {"x", p.x},
                    {"y", p.y},
                    {"launch_env", p.launch_env}
                };
            }
            config["panel_placement"] = {{"panels", panels}};
        }
        
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
        
        // Atomic save: write to .tmp file first to prevent corruption
        fs::path tmp_path = config_path;
        tmp_path += ".tmp";
        
        std::ofstream file(tmp_path);
        if (!file.is_open()) {
            return false;
        }
        
        file << config.dump(4); // Pretty print with 4 spaces
        file.close();
        
        fs::rename(tmp_path, config_path);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving config: " + std::string(e.what()));
        return false;
    }
}
