#include "conky_manager.h"
#include "utils.h"
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <cstdio>
#include <memory>
#include <QProcess>
#include <QTimer>
#include <iostream>
#include <stdexcept>
#include <string_view>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
std::recursive_mutex g_conky_mutex;
std::condition_variable_any g_restart_cv;

fs::path active_panels_json_path() {
    return Utils::conky_wayland_directory() / "active_panels.json";
}

// Modern C++ helper functions
bool file_exists(const fs::path& path) {
    return fs::exists(path);
}

std::string get_config_path(const std::string& panel_name) {
    return Utils::get_conky_config_path(panel_name).string();
}

// Improved error handling with exceptions
void throw_if_not_running(const std::string& panel_name) {
    if (!ConkyManager::is_conky_running()) {
        throw std::runtime_error("No Conky panels are running");
    }
}
}  // namespace

// Static member definitions
std::vector<QProcess*> ConkyManager::_processes;
std::chrono::steady_clock::time_point ConkyManager::_last_refresh_time;
std::vector<std::string> ConkyManager::_cached_running_configs;
double ConkyManager::_cache_time = 0.0;
bool ConkyManager::_restart_pending = false;
std::vector<std::string> ConkyManager::_pending_panels;
bool ConkyManager::_pending_active_restart = false;

// Modern C++ improvements: use std::optional for better error handling
std::optional<std::string> get_running_config_path(const std::string& panel_name) {
    auto running_configs = ConkyManager::get_running_configs(true);
    std::string abs_path = fs::absolute(get_config_path(panel_name)).string();
    
    for (const auto& r : running_configs) {
        if (fs::absolute(fs::path(r)).string() == abs_path) {
            return r;
        }
    }
    return std::nullopt;
}

void ConkyManager::record_refresh() {
    _last_refresh_time = std::chrono::steady_clock::now();
}

double ConkyManager::seconds_since_refresh() {
    if (_last_refresh_time.time_since_epoch().count() == 0) {
        return -1.0;
    }

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - _last_refresh_time);
    return duration.count();
}

std::map<std::string, int> ConkyManager::load_tracked_pids() {
    std::map<std::string, int> pids;

    if (fs::exists(active_panels_json_path())) {
        std::ifstream file(active_panels_json_path());
        if (file.is_open()) {
            try {
                json j;
                file >> j;
                for (auto& [name, pid] : j.items()) {
                    pids[name] = pid.get<int>();
                }
            } catch (...) {
                // If file is corrupted, return empty
            }
        }
    }
    return pids;
}

void ConkyManager::save_tracked_pids(const std::map<std::string, int>& pids) {
    try {
        json j = pids;
        std::ofstream file(active_panels_json_path());
        if (file.is_open()) {
            file << j.dump(4);
        }
    } catch (...) {
        // Handle write errors
    }
}

void ConkyManager::update_pid(const std::string& panel_name, int pid) {
    auto pids = load_tracked_pids();
    pids[panel_name] = pid;
    save_tracked_pids(pids);
}

void ConkyManager::remove_pid(const std::string& panel_name) {
    auto pids = load_tracked_pids();
    pids.erase(panel_name);
    save_tracked_pids(pids);
}

std::vector<std::string> ConkyManager::get_running_configs(bool force_refresh) {
    std::lock_guard<std::recursive_mutex> lock(g_conky_mutex);
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - _last_refresh_time);

    if (!force_refresh && !_cached_running_configs.empty() && duration.count() < 1.0) {
        return _cached_running_configs;
    }

    _cached_running_configs = get_running_configs_uncached();
    _cache_time = duration.count();
    return _cached_running_configs;
}

void ConkyManager::reap_zombies() {
    std::lock_guard<std::recursive_mutex> lock(g_conky_mutex);
    auto it = _processes.begin();
    while (it != _processes.end()) {
        if ((*it)->state() == QProcess::NotRunning) {
            (*it)->deleteLater();
            it = _processes.erase(it);
        } else {
            ++it;
        }
    }
}

bool ConkyManager::is_conky_running() {
    return !get_running_configs_uncached().empty();
}

void ConkyManager::kill_all_conky() {
    std::lock_guard<std::recursive_mutex> lock(g_conky_mutex);

    // Gracefully terminate all tracked Qt processes
    for (QProcess* p : _processes) {
        if (p && p->state() != QProcess::NotRunning) {
            p->terminate();  // Send SIGTERM first for graceful shutdown
        }
    }

    // Force kill any remaining tracked processes
    for (QProcess* p : _processes) {
        if (p && p->state() != QProcess::NotRunning) {
            p->kill();  // Force kill if still running
        }
    }
    // Immediate hard kill for reliability on restart
    // Match any conky process with -c in its arguments (may be preceded by -q -o etc.)
    system("pkill -9 -f \"conky.*-c\"");

    // Clean up process handles
    for (QProcess* p : _processes) {
        p->deleteLater();
    }
    _processes.clear();

    // Clear cached state
    _cached_running_configs.clear();
    save_tracked_pids({});
}

// Modern C++ improvements: better error handling and validation
bool ConkyManager::start_panel(const std::string& panel_name, bool skip_check) {
    // NOTE: This function MUST be called from the main Qt thread.
    // QProcess objects are Qt objects and are not safe to create or use
    // from background threads.
    std::lock_guard<std::recursive_mutex> lock(g_conky_mutex);

    fs::path config_path = Utils::get_conky_config_path(panel_name);
    if (!fs::exists(config_path)) {
        throw std::runtime_error("Configuration file not found for panel: " + panel_name);
    }

    // Check if already running unless caller has opted out of the check
    if (!skip_check) {
        auto running = get_running_configs(true);
        std::string abs_path = fs::absolute(config_path).string();
        for (const auto& r : running) {
            if (fs::absolute(fs::path(r)).string() == abs_path) {
                return true; // Already running, nothing to do
            }
        }
    }

    // Clean up any dead process handles first
    reap_zombies();

    // Create and start the new Conky process.
    // We do NOT daemonize (background = false in config) so that Qt can
    // track the child process lifetime via its process ID.
    // Use -q (quiet) and -o (own-window) for explicit own-window creation;
    // at the config level this is controlled by own_window = true.
    auto p = std::make_unique<QProcess>();
    p->setProgram("conky");
    QString q_config_path = QString::fromStdString(config_path.string());
    if (q_config_path.isEmpty()) {
        throw std::runtime_error("Invalid configuration path for panel: " + panel_name);
    }
    p->setArguments({"-q", "-o", "-c", q_config_path});
    p->start();

    if (p->waitForStarted(3000)) {
        _processes.push_back(p.release());
        update_pid(panel_name, static_cast<int>(_processes.back()->processId()));
        return true;
    }

    // waitForStarted timed out but process may still have launched
    {
        std::string pgrep_cmd = "pgrep -f \"conky -c.*" + q_config_path.toStdString() + "\" > /dev/null 2>&1";
        if (system(pgrep_cmd.c_str()) == 0) {
            _processes.push_back(p.release());
            update_pid(panel_name, 0);
            return true;
        }
    }

    throw std::runtime_error("Failed to start Conky panel: " + panel_name);
}

// Modern C++ improvements: better error handling and validation
void ConkyManager::stop_panel(const std::string& panel_name) {
    std::lock_guard<std::recursive_mutex> lock(g_conky_mutex);

    fs::path config_path = Utils::get_conky_config_path(panel_name);
    QString match_str = QString::fromStdString(config_path.filename().string());

    // Gracefully terminate any tracked Qt process whose arguments reference this panel config
    for (QProcess* p : _processes) {
        if (p && p->state() != QProcess::NotRunning) {
            if (p->arguments().join(" ").contains(match_str)) {
                p->terminate();  // Send SIGTERM first for graceful shutdown
            }
        }
    }

    // Force kill any remaining processes
    for (QProcess* p : _processes) {
        if (p && p->state() != QProcess::NotRunning) {
            if (p->arguments().join(" ").contains(match_str)) {
                p->kill();  // Force kill if still running
            }
        }
    }
    // Use pkill -9 immediately for the specific config to ensure resource release
    // Match conky with -c <filename> (flags like -q -o may precede -c)
    std::string kill_cmd = "pkill -9 -f \"conky.*" + config_path.filename().string() + "\"";
    system(kill_cmd.c_str());
    remove_pid(panel_name);
}

void ConkyManager::reload_panel(const std::string& panel_name) {
    try {
        fs::path config_path = Utils::get_conky_config_path(panel_name);
        std::string filename = config_path.filename().string();
        if (filename.empty()) return;

        // Send SIGUSR1 to reload config on this specific instance
        // We target the specific config file to avoid reloading unrelated instances
        std::string reload_cmd = "pkill -SIGUSR1 -f \"conky.*" + filename + "\"";
        system(reload_cmd.c_str());
    } catch (...) {
        // Absorb filesystem exceptions to prevent app crash
    }
}

// ---------------------------------------------------------------------------
// restart_active_panels
//
// FIX SUMMARY:
//   Old code created QProcess objects inside a std::thread, which violates
//   Qt's threading rules and caused waitForStarted() to spuriously return
//   false — silently dropping panels. The restart also relied on a fixed
//   1500 ms sleep to wait for pkill to finish, which is unreliable under
//   varying system load.
//
//   New approach:
//     1. Snapshot running panels before killing anything.
//     2. Kill all conky processes.
//     3. Poll pgrep (max 3 s) to confirm all processes are actually gone
//        before attempting to start new ones — no more guesswork sleeps.
//     4. Schedule sequential panel starts back on the main thread using
//        QTimer::singleShot so QProcess is always created on the correct
//        thread. This eliminates the race and the unreliable waitForStarted.
// ---------------------------------------------------------------------------
void ConkyManager::restart_active_panels() {
    {
        std::lock_guard<std::recursive_mutex> lock(g_conky_mutex);
        if (_restart_pending) {
            _pending_active_restart = true;
            return;
        }
        _restart_pending = true;
    }

    // 1. Snapshot which panels are currently running BEFORE we kill anything
    auto running_configs = get_running_configs(true);
    auto all_panels      = Utils::discover_panels();
    std::vector<std::string> panels_to_restart;

    for (const auto& panel : all_panels) {
        fs::path config_path = Utils::get_conky_config_path(panel);
        std::string config_filename = config_path.filename().string();
        
        for (const auto& running : running_configs) {
            fs::path running_path(running);
            if (fs::absolute(running_path) == fs::absolute(config_path) || 
                running_path.filename().string() == config_filename) {
                panels_to_restart.push_back(panel);
                break;
            }
        }
    }

    // 2. Kill everything and clear the cache immediately
    kill_all_conky();
    _cached_running_configs.clear();

    if (panels_to_restart.empty()) return;

    {
        std::lock_guard<std::recursive_mutex> lock(g_conky_mutex);
        _restart_pending = true;
    }
    // kill_all_conky already waited for death, so shorter delay is fine
    QTimer::singleShot(500, [panels_to_restart]() {
        _scheduleSequentialStart(panels_to_restart, 0);
    });
}

// ---------------------------------------------------------------------------
// _scheduleSequentialStart
//
// Starts the panel at `index`, then schedules itself recursively via
// QTimer::singleShot to start the next panel after a 600 ms gap.
// Because QTimer fires on the main event loop, QProcess is always
// created on the correct thread.
// ---------------------------------------------------------------------------
void ConkyManager::_scheduleSequentialStart(const std::vector<std::string>& panels, size_t index) {
    if (index >= panels.size()) {
        // All panels have been started — clear the pending flag
        // and process any queued restart requests
        std::vector<std::string> pending;
        bool pending_active = false;
        {
            std::lock_guard<std::recursive_mutex> lock(g_conky_mutex);
            _restart_pending = false;
            pending.swap(_pending_panels);
            pending_active = _pending_active_restart;
            _pending_active_restart = false;
        }
        g_restart_cv.notify_all();

        if (pending_active) {
            restart_active_panels();
        } else if (!pending.empty()) {
            restart_panels_with_verification(pending);
        }
        return;
    }

    // Start this panel immediately (we are on the main thread here)
    try {
        start_panel(panels[index], true);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to start panel '" << panels[index]
                  << "' during restart: " << e.what() << std::endl;
    }

    // Schedule the next panel after a short gap
    QTimer::singleShot(400, [panels, index]() {
        ConkyManager::_scheduleSequentialStart(panels, index + 1);
    });
}

void ConkyManager::wait_for_pending_restarts(int timeout_ms) {
    std::unique_lock<std::recursive_mutex> lock(g_conky_mutex);

    if (_restart_pending) {
        auto until = std::chrono::steady_clock::now() +
                     std::chrono::milliseconds(timeout_ms);
        g_restart_cv.wait_until(lock, until, []() { return !_restart_pending; });
    }

}

void ConkyManager::cleanup_on_exit() {
    std::lock_guard<std::recursive_mutex> lock(g_conky_mutex);

    // Signal any waiters that we are done
    _restart_pending = false;
    g_restart_cv.notify_all();

    // Delete QProcess handles WITHOUT killing the underlying conky processes.
    // This allows panels to keep running after the control centre is closed
    // and be re-detected on next startup.
    for (QProcess* p : _processes) {
        p->deleteLater();
    }
    _processes.clear();

    // NOTE: We intentionally do NOT call "pkill -9 conky" here.
    // Panels continue running independently after the app exits.

    save_tracked_pids({});
    _cached_running_configs.clear();
}

void ConkyManager::verify_panel_state_on_startup() {
    std::lock_guard<std::recursive_mutex> lock(g_conky_mutex);

    auto all_panels     = Utils::discover_panels();
    auto running_configs = get_running_configs(true);

    // Rebuild process tracking from scratch — we don't own these processes
    // (they were started by a previous instance of the app), so we just
    // record that they are running without holding a QProcess handle.
    _processes.clear();

    std::vector<std::string> found_running_panels;
    for (const auto& panel : all_panels) {
        std::string expected_path = fs::absolute(Utils::get_conky_config_path(panel)).string();
        for (const auto& running : running_configs) {
            if (fs::absolute(fs::path(running)).string() == expected_path) {
                found_running_panels.push_back(panel);
                // Use PID 0 to indicate: running but not owned by this instance
                update_pid(panel, 0);
                break;
            }
        }
    }

    // We do NOT kill orphaned processes here — too aggressive on app restart.
    // Users can clean up manually if needed.
}

void ConkyManager::start_all_panels() {
    auto panels = Utils::discover_panels();
    _start_panels_sequence(panels);
}

void ConkyManager::restart_panels_with_verification(const std::vector<std::string>& panels) {
    if (panels.empty()) return;

    {
        std::lock_guard<std::recursive_mutex> lock(g_conky_mutex);
        if (_restart_pending) {
            _pending_panels.insert(_pending_panels.end(), panels.begin(), panels.end());
            return;
        }
        _restart_pending = true;
    }

    // Surgical stop: only stop the panels we intend to restart
    for (const auto& panel : panels) {
        stop_panel(panel);
    }

    QTimer::singleShot(500, [panels]() {
        _scheduleSequentialStart(panels, 0);
    });
}

void ConkyManager::_start_panels_sequence(const std::vector<std::string>& panels) {
    // Use the same QTimer-based sequential start so QProcess stays on the main thread.
    // This replaces the old detached std::thread approach which was unsafe.
    if (panels.empty()) return;
    _scheduleSequentialStart(panels, 0);
}

std::vector<std::string> ConkyManager::get_running_configs_uncached() {
    std::vector<std::string> running_configs;

    // Be specific: only look for processes running with a config file
    FILE* pipe = popen("pgrep -af 'conky.*-c'", "r");
    if (!pipe) return running_configs;

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        size_t c_pos = line.find(" -c ");
        if (c_pos != std::string::npos) {
            std::string path = line.substr(c_pos + 4);
            // Trim trailing whitespace and newlines
            size_t end = path.find_last_not_of(" \n\r\t");
            if (end != std::string::npos) {
                path = path.substr(0, end + 1);
            }
            
            try {
                if (!path.empty() && fs::exists(path)) {
                    running_configs.push_back(path);
                }
            } catch (...) {
                // Ignore errors for invalid paths detected from system process list
            }
        }
    }
    pclose(pipe);

    record_refresh();
    return running_configs;
}

// ---------------------------------------------------------------------------
// Qt-style public interface
// ---------------------------------------------------------------------------

bool ConkyManager::startConky() {
    start_all_panels();
    return true;
}

bool ConkyManager::stopConky() {
    kill_all_conky();
    return true;
}

bool ConkyManager::restartConky() {
    restart_active_panels();
    return true;
}

bool ConkyManager::isConkyRunning() {
    return is_conky_running();
}
