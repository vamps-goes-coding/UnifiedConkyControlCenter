#include "crash_recovery.h"
#include "logger.h"
#include "app_info.h"
#include "config_manager.h"
#include "conky_manager.h"
#include "utils.h"

#include <fstream>
#include <iostream>

// Static member initialization
fs::path CrashRecovery::recovery_dir_;
fs::path CrashRecovery::recovery_file_;

json CrashRecoveryData::to_json() const {
    json j;
    j["last_config_path"] = last_config_path;
    j["last_theme_path"] = last_theme_path;
    j["active_panels"] = active_panels;
    j["last_save_time"] = std::chrono::system_clock::to_time_t(last_save_time);
    j["was_clean_exit"] = was_clean_exit;
    return j;
}

CrashRecoveryData CrashRecoveryData::from_json(const json& j) {
    CrashRecoveryData data;
    data.last_config_path = j.value("last_config_path", "");
    data.last_theme_path = j.value("last_theme_path", "");
    data.active_panels = j.value("active_panels", std::vector<std::string>());
    data.was_clean_exit = j.value("was_clean_exit", true);
    
    if (j.contains("last_save_time")) {
        auto time_t = j["last_save_time"].get<std::time_t>();
        data.last_save_time = std::chrono::system_clock::from_time_t(time_t);
    }
    
    return data;
}

bool CrashRecovery::initialize() {
    const char* home = std::getenv("HOME");
    if (!home) {
        LOG_ERROR("Cannot initialize crash recovery: HOME environment variable not set");
        return false;
    }
    
    recovery_dir_ = fs::path(home) / ".local" / "share" / AppInfo::get_internal_name() / "recovery";
    recovery_file_ = recovery_dir_ / "recovery_state.json";
    
    try {
        fs::create_directories(recovery_dir_);
        LOG_INFO("Crash recovery initialized: " + recovery_dir_.string());
        return true;
    } catch (const fs::filesystem_error& e) {
        LOG_ERROR("Failed to create recovery directory: " + std::string(e.what()));
        return false;
    }
}

void CrashRecovery::save_state(const CrashRecoveryData& data) {
    if (recovery_file_.empty()) {
        LOG_WARNING("Recovery file path not initialized");
        return;
    }
    
    try {
        json j = data.to_json();
        std::ofstream file(recovery_file_);
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
            LOG_DEBUG("Recovery state saved");
        } else {
            LOG_ERROR("Failed to open recovery file for writing");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save recovery state: " + std::string(e.what()));
    }
}

std::optional<CrashRecoveryData> CrashRecovery::load_recovery_data() {
    if (recovery_file_.empty() || !fs::exists(recovery_file_)) {
        return std::nullopt;
    }
    
    try {
        std::ifstream file(recovery_file_);
        if (!file.is_open()) {
            return std::nullopt;
        }
        
        json j;
        file >> j;
        file.close();
        
        return CrashRecoveryData::from_json(j);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load recovery data: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool CrashRecovery::needs_recovery() {
    auto data = load_recovery_data();
    if (!data) {
        return false;
    }
    
    // Check if last exit was clean
    if (data->was_clean_exit) {
        return false;
    }
    
    // Check if recovery data is recent (within last 24 hours)
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::hours>(now - data->last_save_time);
    
    if (diff.count() > 24) {
        LOG_INFO("Recovery data is too old, skipping recovery");
        cleanup_recovery_file();
        return false;
    }
    
    return true;
}

bool CrashRecovery::perform_recovery() {
    LOG_INFO("Performing crash recovery...");
    
    auto data = load_recovery_data();
    if (!data) {
        LOG_WARNING("No recovery data available");
        return false;
    }
    
    try {
        // Restore configuration path if available
        if (!data->last_config_path.empty()) {
            auto& config = ConfigManager::instance();
            config.set_conky_config_path(data->last_config_path);
            LOG_INFO("Recovered config path: " + data->last_config_path);
        }
        
        // Restore theme path if available
        if (!data->last_theme_path.empty()) {
            auto& config = ConfigManager::instance();
            config.set_themes_path(data->last_theme_path);
            LOG_INFO("Recovered theme path: " + data->last_theme_path);
        }
        
        // Restart active panels
        if (!data->active_panels.empty()) {
            LOG_INFO("Recovering " + std::to_string(data->active_panels.size()) + " active panels");
            
            for (const auto& panel : data->active_panels) {
                try {
                    ConkyManager::start_panel(panel);
                    LOG_INFO("Recovered panel: " + panel);
                } catch (const std::exception& e) {
                    LOG_WARNING("Failed to recover panel " + panel + ": " + e.what());
                }
            }
        }
        
        // Mark as clean exit after recovery
        mark_clean_exit();
        
        LOG_INFO("Crash recovery completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Crash recovery failed: " + std::string(e.what()));
        return false;
    }
}

void CrashRecovery::mark_clean_exit() {
    auto data = load_recovery_data();
    if (!data) {
        data = CrashRecoveryData();
    }
    
    data->was_clean_exit = true;
    save_state(*data);
    LOG_DEBUG("Marked clean exit");
}

fs::path CrashRecovery::get_recovery_file_path() {
    return recovery_file_;
}

void CrashRecovery::cleanup_recovery_file() {
    if (fs::exists(recovery_file_)) {
        try {
            fs::remove(recovery_file_);
            LOG_INFO("Recovery file cleaned up");
        } catch (const fs::filesystem_error& e) {
            LOG_ERROR("Failed to cleanup recovery file: " + std::string(e.what()));
        }
    }
}