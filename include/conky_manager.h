#pragma once

#include <string>
#include <vector>
#include <map>
#include <thread>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

class QProcess;

class ConkyManager {
public:
    // Process management
    static bool is_conky_running();
    static void kill_all_conky();
    static bool start_panel(const std::string& panel_name, bool skip_check = false);
    static void stop_panel(const std::string& panel_name);
    
    // Process tracking
    static void record_refresh();
    static double seconds_since_refresh();
    static std::map<std::string, int> load_tracked_pids();
    static void save_tracked_pids(const std::map<std::string, int>& pids);
    static void update_pid(const std::string& panel_name, int pid);
    static void remove_pid(const std::string& panel_name);
    
    // Running configuration detection
    static std::vector<std::string> get_running_configs(bool force_refresh = false);
    
    // Process lifecycle
    static void reap_zombies();
    
    // Panel operations with timing
    static void reload_panel(const std::string& panel_name);
    static void restart_active_panels();
    static void start_all_panels();
    static void restart_panels_with_verification(const std::vector<std::string>& panels);
    
    // Cleanup and synchronization
    static void cleanup_on_exit();
    static void wait_for_pending_restarts(int timeout_ms = 3000);
    static void verify_panel_state_on_startup();
    
    // Qt-style interface for main.cpp
    bool startConky();
    bool stopConky();
    bool restartConky();
    bool isConkyRunning();
    
private:
    static std::vector<QProcess*> _processes;
    static std::chrono::steady_clock::time_point _last_refresh_time;
    static std::vector<std::string> _cached_running_configs;
    static double _cache_time;
    static std::thread _restart_thread;
    static bool _restart_pending;
    
    static std::vector<std::string> get_running_configs_uncached();
    static void _start_panels_sequence(const std::vector<std::string>& panels);
    static void _scheduleSequentialStart(const std::vector<std::string>& panels, size_t index);
};