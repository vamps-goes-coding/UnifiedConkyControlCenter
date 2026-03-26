#pragma once

#include <string>
#include <filesystem>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

struct CrashRecoveryData {
    std::string last_config_path;
    std::string last_theme_path;
    std::vector<std::string> active_panels;
    std::chrono::system_clock::time_point last_save_time;
    bool was_clean_exit;
    
    json to_json() const;
    static CrashRecoveryData from_json(const json& j);
};

class CrashRecovery {
public:
    // Initialize crash recovery system
    static bool initialize();
    
    // Save current state
    static void save_state(const CrashRecoveryData& data);
    
    // Load recovery data
    static std::optional<CrashRecoveryData> load_recovery_data();
    
    // Check if recovery is needed
    static bool needs_recovery();
    
    // Perform recovery
    static bool perform_recovery();
    
    // Mark clean exit
    static void mark_clean_exit();
    
    // Get recovery file path
    static fs::path get_recovery_file_path();
    
    // Clean up recovery file
    static void cleanup_recovery_file();
    
private:
    static fs::path recovery_dir_;
    static fs::path recovery_file_;
};