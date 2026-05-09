#include "theme_manager.h"
#include "utils.h"
#include "config_parser.h"
#include <dirent.h>
#include <fstream>
#include <algorithm>
#include <vector>
#include <map>
#include <stdexcept>
#include <string_view>

#include <signal.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <sstream>
#include <fcntl.h>
namespace fs = std::filesystem;

static std::atomic<bool> restart_in_progress{false};

// Spawn a process from a space-separated cmdline using fork()+execvp().
// The child setsid()s and redirects all fds to /dev/null so it doesn't
// block the parent or inherit stdin/stdout/stderr.
static void spawn_conky_fork_exec(const std::string& cmdline) {
    std::vector<std::string> tokens;
    std::istringstream stream(cmdline);
    std::string token;
    while (stream >> token) {
        tokens.push_back(std::move(token));
    }
    if (tokens.empty()) return;

    std::vector<char*> argv;
    for (auto& t : tokens) {
        argv.push_back(t.data());
    }
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO) close(fd);
        }
        execvp(argv[0], argv.data());
        _exit(127);
    }
}

static void restart_conky_instances() {
    if (restart_in_progress.exchange(true)) return;

    DIR* dir = opendir("/proc");
    if (!dir) {
        restart_in_progress = false;
        return;
    }

    std::vector<std::string> conky_cmdlines;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;
        const char* dname = entry->d_name;
        bool is_pid = true;
        for (const char* p = dname; *p; ++p) {
            if (!(*p >= '0' && *p <= '9')) { is_pid = false; break; }
        }
        if (!is_pid) continue;

        char cmdpath[256];
        snprintf(cmdpath, sizeof(cmdpath), "/proc/%s/cmdline", dname);
        FILE* f = fopen(cmdpath, "r");
        if (!f) continue;

        char buf[4096];
        size_t n = 0; size_t r;
        while ((r = fread(buf + n, 1, sizeof(buf) - n - 1, f)) > 0) {
            n += r;
            if (n >= sizeof(buf) - 1) break;
        }
        fclose(f);

        for (size_t i = 0; i < n; ++i) {
            if (buf[i] == '\0') buf[i] = ' ';
        }
        buf[n] = '\0';
        std::string cmdline(buf);
        if (cmdline.find("conky") != std::string::npos) {
            kill(atoi(dname), SIGTERM);
            conky_cmdlines.push_back(std::move(cmdline));
        }
    }
    closedir(dir);

    if (conky_cmdlines.empty()) {
        restart_in_progress = false;
        return;
    }

    std::thread([cmdlines = std::move(conky_cmdlines)]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        for (const auto& cl : cmdlines) {
            spawn_conky_fork_exec(cl);
        }
        restart_in_progress = false;
    }).detach();
}

// Static member implementations
std::map<std::string, std::vector<std::string>> ThemeManager::_cached_categories;
bool ThemeManager::_categories_loaded = false;

// Modern C++ improvements: use constexpr for patterns
constexpr std::string_view THEME_FILE_EXTENSION = ".lua";
constexpr std::string_view CURRENT_THEME_FILE = "current.lua";
constexpr std::string_view PREVIEW_HELPER_FILE = "preview_helper.lua";
constexpr std::string_view CATEGORIES_FILE = "categories.lua";

fs::path ThemeManager::get_category_directory(const std::string& category_key) {
    return Utils::themes_directory() / category_key;
}

fs::path ThemeManager::get_theme_file_path(const std::string& theme_name, const std::string& category_key) {
    if (category_key == "Root") {
        return Utils::themes_directory() / (theme_name + std::string(THEME_FILE_EXTENSION));
    }
    return get_category_directory(category_key) / (theme_name + std::string(THEME_FILE_EXTENSION));
}

std::string ThemeManager::normalize_category_key(const std::string& category_name) {
    std::string result = category_name;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    std::replace(result.begin(), result.end(), ' ', '_');
    return result;
}

std::string ThemeManager::normalize_theme_filename(const std::string& theme_name) {
    std::string result = theme_name;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    std::replace(result.begin(), result.end(), ' ', '-');
    result.erase(std::remove(result.begin(), result.end(), '('), result.end());
    result.erase(std::remove(result.begin(), result.end(), ')'), result.end());
    result.erase(std::remove(result.begin(), result.end(), '.'), result.end());
    return result;
}

std::map<std::string, std::vector<std::string>> ThemeManager::load_categories(bool scan_metadata) {
    if (!scan_metadata && _categories_loaded) {
        return _cached_categories;
    }
    
    std::map<std::string, std::vector<std::string>> categories;
    
    fs::path themes_dir = Utils::themes_directory();
    if (!fs::exists(themes_dir)) {
        return categories;
    }
    
    if (scan_metadata) {
        // Scan file metadata for category tags
        for (const auto& entry : fs::recursive_directory_iterator(themes_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == THEME_FILE_EXTENSION) {
                auto metadata = get_theme_metadata(entry.path());
                std::string category = metadata["category"];
                if (category.empty()) category = "Uncategorized";
                
                std::string theme_name = entry.path().stem().string();
                categories[category].push_back(theme_name);
            }
        }
    } else {
        // Use directory structure
        for (const auto& entry : fs::directory_iterator(themes_dir)) {
            if (entry.is_directory()) {
                std::string category_key = entry.path().filename().string();
                std::vector<std::string> themes;
                
                for (const auto& theme_entry : fs::directory_iterator(entry.path())) {
                    if (theme_entry.is_regular_file() && theme_entry.path().extension() == THEME_FILE_EXTENSION) {
                        std::string theme_name = theme_entry.path().stem().string();
                        if (theme_name != CURRENT_THEME_FILE && theme_name != PREVIEW_HELPER_FILE) {
                            themes.push_back(theme_name);
                        }
                    }
                }
                
                if (!themes.empty()) {
                    categories[category_key] = themes;
                }
            }
        }

        std::vector<std::string> root_themes;
        for (const auto& entry : fs::directory_iterator(themes_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == THEME_FILE_EXTENSION) {
                std::string theme_name = entry.path().stem().string();
                if (theme_name != CURRENT_THEME_FILE && theme_name != PREVIEW_HELPER_FILE && theme_name != CATEGORIES_FILE) {
                    root_themes.push_back(theme_name);
                }
            }
        }
        if (!root_themes.empty()) {
            std::sort(root_themes.begin(), root_themes.end());
            categories["Root"] = std::move(root_themes);
        }
    }
    
    if (!scan_metadata) {
        _cached_categories = categories;
        _categories_loaded = true;
    }
    
    return categories;
}

std::vector<std::string> ThemeManager::get_themes_for_category(const std::string& category_key) {
    if (_categories_loaded && _cached_categories.count(category_key)) {
        return _cached_categories[category_key];
    }
    
    // Fallback if not loaded
    auto cats = load_categories(false);
    return cats[category_key];
}

std::vector<std::string> ThemeManager::get_theme_colors(const std::string& theme_name, const std::string& category_key) {
    std::vector<std::string> colors = {"#FFFFFF", "#AAAAAA", "#888888", "#444444"};
    fs::path theme_file = get_theme_file_path(theme_name, category_key);
    
    if (fs::exists(theme_file)) {
        std::ifstream file(theme_file);
        if (file.is_open()) {
            std::string line;
            int color_index = 0;
            
            while (std::getline(file, line) && color_index < 4) {
                size_t pos = line.find("color" + std::to_string(color_index + 1) + " = ");
                if (pos != std::string::npos) {
                    size_t start = line.find("'", pos);
                    if (start != std::string::npos) {
                        size_t end = line.find("'", start + 1);
                        if (end != std::string::npos) {
                            colors[color_index] = line.substr(start + 1, end - start - 1);
                            color_index++;
                        }
                    }
                }
            }
            file.close();
        }
    }
    
    return colors;
}

std::map<std::string, std::string> ThemeManager::get_theme_metadata(const fs::path& theme_file) {
    std::map<std::string, std::string> metadata;
    metadata["name"] = theme_file.stem().string();
    metadata["category"] = "Uncategorized";
    
    if (fs::exists(theme_file)) {
        std::ifstream file(theme_file);
        if (file.is_open()) {
            std::string line;
            
            while (std::getline(file, line)) {
                if (line.find("name = ") != std::string::npos) {
                    size_t start = line.find("'", line.find("name = "));
                    if (start != std::string::npos) {
                        size_t end = line.find("'", start + 1);
                        if (end != std::string::npos) {
                            metadata["name"] = line.substr(start + 1, end - start - 1);
                        }
                    }
                }
                
                if (line.find("category = ") != std::string::npos) {
                    size_t start = line.find("'", line.find("category = "));
                    if (start != std::string::npos) {
                        size_t end = line.find("'", start + 1);
                        if (end != std::string::npos) {
                            metadata["category"] = line.substr(start + 1, end - start - 1);
                        }
                    }
                }
            }
            file.close();
        }
    }
    
    return metadata;
}

bool ThemeManager::apply_theme_to_panel(const std::string& theme_name, const std::string& category_key, const std::string& panel_name) {
    fs::path source_theme = get_theme_file_path(theme_name, category_key);
    fs::path destination_theme = Utils::conky_wayland_directory() / (panel_name + "-theme.lua");
    
    bool write_success = copy_theme_to_panel(source_theme, destination_theme);
    if (write_success) {
        Utils::signal_all_conky_instances();
        // Ensure all Conky panels across monitors refresh their layout
        restart_conky_instances();
    }
    return write_success;
}

bool ThemeManager::apply_global_theme(const std::string& theme_name, const std::string& category_key) {
    auto panels = Utils::discover_panels();
    fs::path source_theme = get_theme_file_path(theme_name, category_key);
    
    if (!fs::exists(source_theme)) {
        return false;
    }

    bool all_success = true;
    for (const auto& panel : panels) {
        fs::path destination_theme = Utils::conky_wayland_directory() / (panel + "-theme.lua");
        if (!copy_theme_to_panel(source_theme, destination_theme)) {
            all_success = false;
        }
    }
    
    // Also update the global fallback current.lua so any panel using
    // the global theme file will pick it up on next restart
    fs::path current_lua  = Utils::themes_directory() / "current.lua";
    if (!copy_theme_to_panel(source_theme, current_lua)) {
        all_success = false;
    }
    
    // Update the current_theme.txt file so the preview panel shows the correct theme name
    fs::path current_theme_txt = Utils::themes_directory() / "current_theme.txt";
    std::ofstream theme_name_file(current_theme_txt);
    if (theme_name_file.is_open()) {
        theme_name_file << theme_name;
        theme_name_file.close();
    }
    
    // Signal once after all files are written
    if (all_success) {
        Utils::signal_all_conky_instances();
        // Restart all Conky processes to guarantee cross-monitor theme application
        restart_conky_instances();
    }

    return all_success;
}

bool ThemeManager::create_lua_theme(const std::string& character_name, const std::vector<std::string>& colors, const fs::path& output_dir, const std::string& category_name) {
    if (colors.size() < 4) {
        throw std::invalid_argument("At least 4 colors are required to create a theme");
    }
    
    fs::path category_dir = output_dir / normalize_category_key(category_name);
    fs::create_directories(category_dir);
    
    std::string filename = normalize_theme_filename(character_name);
    fs::path theme_file = category_dir / (filename + ".lua");
    
    std::ofstream file(theme_file);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create theme file: " + theme_file.string());
    }
    
    file << "theme = {\n";
    file << "    name = \"" << character_name << "\",\n";
    file << "    category = \"" << category_name << "\",\n";
    file << "    color1 = '" << colors[0] << "',  -- Primary\n";
    file << "    color2 = '" << colors[1] << "',  -- Secondary\n";
    file << "    color3 = '" << colors[2] << "',  -- Accent 1\n";
    file << "    color4 = '" << colors[3] << "',  -- Accent 2\n";
    file << "}\n";
    
    file.close();
    return true;
}

bool ThemeManager::copy_theme_to_panel(const fs::path& source_theme, const fs::path& destination_theme) {
    if (!fs::exists(source_theme)) {
        throw std::runtime_error("Source theme file not found: " + source_theme.string());
    }
    
    fs::create_directories(destination_theme.parent_path());
    fs::copy_file(source_theme, destination_theme, fs::copy_options::overwrite_existing);
    return true;
}

bool ThemeManager::delete_theme(const std::string& theme_name, const std::string& category_key) {
    fs::path theme_file = get_theme_file_path(theme_name, category_key);
    
    if (fs::exists(theme_file)) {
        fs::remove(theme_file);
        return true;
    }
    
    return false;
}

bool ThemeManager::move_theme(const std::string& theme_name, const std::string& source_category, const std::string& target_category) {
    fs::path source_file = get_theme_file_path(theme_name, source_category);
    fs::path target_file = get_theme_file_path(theme_name, target_category);
    
    if (!fs::exists(source_file)) {
        throw std::runtime_error("Source theme file not found: " + source_file.string());
    }
    
    fs::create_directories(target_file.parent_path());
    fs::copy_file(source_file, target_file, fs::copy_options::overwrite_existing);
    fs::remove(source_file);
    
    return true;
}

bool ThemeManager::create_category(const std::string& category_name) {
    fs::path category_dir = get_category_directory(normalize_category_key(category_name));
    return fs::create_directories(category_dir);
}

bool ThemeManager::delete_category(const std::string& category_name) {
    fs::path category_dir = get_category_directory(normalize_category_key(category_name));
    
    if (fs::exists(category_dir)) {
        fs::remove_all(category_dir);
        return true;
    }
    
    return false;
}

bool ThemeManager::rename_category(const std::string& old_name, const std::string& new_name) {
    fs::path old_dir = get_category_directory(normalize_category_key(old_name));
    fs::path new_dir = get_category_directory(normalize_category_key(new_name));
    
    if (fs::exists(old_dir) && !fs::exists(new_dir)) {
        fs::rename(old_dir, new_dir);
        return true;
    }
    
    return false;
}

bool ThemeManager::sync_category_with_csv(const std::string& category_name, const fs::path& csv_path) {
    auto csv_data = ConfigParser::parse_csv_file(csv_path);
    if (csv_data.empty()) return false;
    
    bool all_success = true;
    for (const auto& row : csv_data) {
        if (row.count("Character") && row.count("Primary") && row.count("Secondary") && row.count("Accent1") && row.count("Accent2")) {
            std::vector<std::string> colors = {
                row.at("Primary"), row.at("Secondary"), row.at("Accent1"), row.at("Accent2")
            };
            if (!create_lua_theme(row.at("Character"), colors, Utils::themes_directory(), category_name)) {
                all_success = false;
            }
        }
    }
    return all_success;
}

bool ThemeManager::export_category_to_csv(const std::string& category_name, const fs::path& csv_path) {
    auto themes = get_themes_for_category(category_name);
    std::vector<std::map<std::string, std::string>> data;
    std::vector<std::string> headers = {"Character", "Primary", "Secondary", "Accent1", "Accent2"};
    
    for (const auto& theme : themes) {
        auto colors = get_theme_colors(theme, category_name);
        if (colors.size() >= 4) {
            std::map<std::string, std::string> row;
            row["Character"] = theme;
            row["Primary"] = colors[0];
            row["Secondary"] = colors[1];
            row["Accent1"] = colors[2];
            row["Accent2"] = colors[3];
            data.push_back(row);
        }
    }
    
    return ConfigParser::write_csv_file(csv_path, data, headers);
}

bool ThemeManager::validate_theme_file(const fs::path& theme_path) {
    if (!fs::exists(theme_path)) {
        return false;
    }
    
    std::ifstream file(theme_path);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    bool has_colors = false;
    
    while (std::getline(file, line)) {
        if (line.find("color") != std::string::npos && line.find("'#") != std::string::npos) {
            has_colors = true;
            break;
        }
    }
    
    file.close();
    return has_colors;
}

bool ThemeManager::validate_category_name(const std::string& category_name) {
    return !category_name.empty() && category_name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ") == std::string::npos;
}

void ThemeManager::invalidate_cache() {
    _cached_categories.clear();
    _categories_loaded = false;
}

// Qt-style interface implementations
void ThemeManager::applyCurrentTheme() {
    // Apply current theme - implementation would depend on your theme system
}

void ThemeManager::refreshThemes() {
    // Refresh themes - implementation would depend on your theme system
}
