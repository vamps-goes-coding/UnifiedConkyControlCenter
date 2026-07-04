#include "utils.h"
#include "config_manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>

// No hardcoded home paths here

fs::path Utils::conky_wayland_directory() {
    return ConfigManager::instance().get_conky_wayland_directory();
}

fs::path Utils::themes_directory() {
    return ConfigManager::instance().get_themes_directory();
}

fs::path Utils::get_executable_directory() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
        return fs::path(std::string(result, count)).parent_path();
    }
    return fs::current_path();
}

fs::path Utils::normalize_path(const std::string& path) {
    return fs::absolute(fs::path(path));
}

std::string Utils::get_display_name(const std::string& panel_id) {
    // Convert panel ID to display name
    std::string display_name = panel_id;
    std::replace(display_name.begin(), display_name.end(), '_', ' ');
    std::replace(display_name.begin(), display_name.end(), '-', ' ');
    
    // Capitalize first letter of each word
    bool capitalize = true;
    for (auto& c : display_name) {
        if (std::isspace(c)) {
            capitalize = true;
        } else if (capitalize) {
            c = std::toupper(c);
            capitalize = false;
        } else {
            c = std::tolower(c);
        }
    }
    
    return display_name;
}

std::vector<std::string> Utils::discover_panels() {
    std::vector<std::string> panels;
    const auto& config = ConfigManager::instance();
    std::string kPrefix = config.get_config_prefix();
    std::vector<std::string> excluded_files = config.get_excluded_files();

    const fs::path root = conky_wayland_directory();
    if (!fs::exists(root)) {
        return panels;
    }

    for (const auto& entry : fs::directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().extension() != config.get_config_extension()) {
            continue;
        }
        std::string stem = entry.path().stem().string();
        
        // Check if file is in excluded list
        bool excluded = false;
        for (const auto& excluded_file : excluded_files) {
            if (stem == excluded_file) {
                excluded = true;
                break;
            }
        }
        if (excluded) {
            continue;
        }
        
        if (stem.size() >= kPrefix.size() && stem.compare(0, kPrefix.size(), kPrefix) == 0) {
            panels.push_back(stem.substr(kPrefix.size()));
        } else {
            panels.push_back(stem);
        }
    }

    std::sort(panels.begin(), panels.end());
    return panels;
}

fs::path Utils::get_conky_config_path(const std::string& panel_name) {
    const auto& config = ConfigManager::instance();
    const fs::path root = conky_wayland_directory();
    fs::path direct = root / (panel_name + config.get_config_extension());
    if (fs::exists(direct)) {
        return direct;
    }
    return root / (config.get_config_prefix() + panel_name + config.get_config_extension());
}

bool Utils::file_exists(const fs::path& path) {
    return fs::exists(path);
}

std::string Utils::read_file(const fs::path& path) {
    std::string content;
    std::ifstream file(path);
    
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();
        file.close();
    }
    
    return content;
}

bool Utils::write_file(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    file.close();
    return true;
}

bool Utils::copy_file(const fs::path& source, const fs::path& destination) {
    if (!fs::exists(source)) {
        return false;
    }
    
    fs::create_directories(destination.parent_path());
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
    return true;
}

bool Utils::delete_file(const fs::path& path) {
    if (fs::exists(path)) {
        return fs::remove(path);
    }
    return false;
}

bool Utils::create_directory(const fs::path& path) {
    return fs::create_directories(path);
}

bool Utils::directory_exists(const fs::path& path) {
    return fs::exists(path) && fs::is_directory(path);
}

std::vector<fs::path> Utils::list_files(const fs::path& directory, const std::string& pattern) {
    std::vector<fs::path> files;
    
    if (fs::exists(directory)) {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                if (pattern == "*" || entry.path().filename().string().find(pattern) != std::string::npos) {
                    files.push_back(entry.path());
                }
            }
        }
    }
    
    return files;
}

std::chrono::steady_clock::time_point Utils::get_current_time() {
    return std::chrono::steady_clock::now();
}

double Utils::get_time_difference(const std::chrono::steady_clock::time_point& start) {
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return duration.count() / 1000.0;
}

std::vector<std::string> Utils::split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    
    return tokens;
}

std::string Utils::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string Utils::to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string Utils::to_upper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string Utils::get_environment_variable(const std::string& name) {
    const char* value = getenv(name.c_str());
    return value ? std::string(value) : "";
}

bool Utils::set_environment_variable(const std::string& name, const std::string& value) {
    return setenv(name.c_str(), value.c_str(), 1) == 0;
}

// Qt-style interface implementations
std::string Utils::getConkyConfigPath() {
    return conky_wayland_directory().string();
}

std::string Utils::getThemePath() {
    return themes_directory().string();
}

bool Utils::fileExists(const std::string& filePath) {
    return file_exists(fs::path(filePath));
}

std::vector<std::string> Utils::listFiles(const std::string& directoryPath) {
    auto files = list_files(fs::path(directoryPath));
    std::vector<std::string> file_strings;
    
    for (const auto& file : files) {
        file_strings.push_back(file.string());
    }
    
    return file_strings;
}

bool Utils::createDirectory(const std::string& directoryPath) {
    return create_directory(fs::path(directoryPath));
}

const fs::path Utils::HOME = []() {
    const char* home = getenv("HOME");
    return home ? fs::path(home) : fs::path("");
}();

// New function to signal all Conky instances
void Utils::signal_all_conky_instances() {
    DIR* dir = opendir("/proc");
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Check if directory name is a number (a PID)
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            std::string pid_str = entry->d_name;
            std::string cmd_path = "/proc/" + pid_str + "/comm";
            std::ifstream comm_file(cmd_path);
            std::string comm;
            
            if (getline(comm_file, comm)) {
                if (comm.find("conky") != std::string::npos) {
                    try {
                        pid_t pid = std::stoi(pid_str);
                        kill(pid, SIGUSR1);
                    } catch (...) {
                        // Ignore malformed PID entries
                    }
                }
            }
        }
    }
    closedir(dir);
}