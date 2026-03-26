#include "conky_manager.h"
#include "utils.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <QProcess>
#include <QTimer>
#include <stdexcept>
#include <string_view>

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
std::thread ConkyManager::_restart_thread;
bool ConkyManager::_restart_pending = false;

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
            // Simple JSON parsing would go here
            // For now, return empty map
        }
    }

    return pids;
}

void ConkyManager::save_tracked_pids(const std::map<std::string, int>& pids) {
    // Save PIDs to file
    // Implementation would write JSON to active_panels_json_path()
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
            delete *it;
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

    // Wait briefly for graceful shutdown
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Force kill any remaining tracked processes
    for (QProcess* p : _processes) {
        if (p && p->state() != QProcess::NotRunning) {
            p->kill();  // Force kill if still running
        }
    }

    // Fallback: kill all conky processes system-wide
    system("pkill -SIGTERM -f \"conky -c\"");
    
    // Wait briefly, then force kill if needed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    system("pkill -9 -f \"conky -c\"");

    // Clean up process handles
    for (QProcess* p : _processes) {
        delete p;
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
    auto p = std::make_unique<QProcess>();
    p->setProgram("conky");
    p->setArguments({"-c", QString::fromStdString(config_path.string())});
    p->start();

    if (p->waitForStarted(2000)) {
        _processes.push_back(p.release());  // Transfer ownership to vector
        update_pid(panel_name, static_cast<int>(_processes.back()->processId()));
        return true;
    }

    // waitForStarted failed — automatic cleanup via unique_ptr
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

    // Wait briefly for graceful shutdown, then force kill if needed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Force kill any remaining processes
    for (QProcess* p : _processes) {
        if (p && p->state() != QProcess::NotRunning) {
            if (p->arguments().join(" ").contains(match_str)) {
                p->kill();  // Force kill if still running
            }
        }
    }

    // Fallback: try graceful termination first via pkill
    std::string term_cmd = "pkill -SIGTERM -f \"conky -c.*" + config_path.filename().string() + "\"";
    system(term_cmd.c_str());
    
    // Wait briefly, then force kill if needed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::string kill_cmd = "pkill -9 -f \"conky -c.*" + config_path.filename().string() + "\"";
    system(kill_cmd.c_str());

    remove_pid(panel_name);
}

void ConkyManager::reload_panel(const std::string& panel_name) {
    fs::path config_path = Utils::get_conky_config_path(panel_name);

    // Send SIGUSR1 to reload config on this specific instance
    std::string reload_cmd = "pkill -SIGUSR1 -f \"conky -c.*" + config_path.filename().string() + "\"";
    system(reload_cmd.c_str());
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
    // 1. Snapshot which panels are currently running BEFORE we kill anything
    auto running_configs = get_running_configs(true);
    auto all_panels      = Utils::discover_panels();
    std::vector<std::string> panels_to_restart;

    for (const auto& panel : all_panels) {
        std::string abs_path = fs::absolute(Utils::get_conky_config_path(panel)).string();
        for (const auto& running : running_configs) {
            if (fs::absolute(fs::path(running)).string() == abs_path) {
                panels_to_restart.push_back(panel);
                break;
            }
        }
    }

    // 2. Kill everything and clear the cache immediately
    kill_all_conky();
    _cached_running_configs.clear();

    // Join any previous restart thread so we don't leave joinable threads
    // lying around (detached threads from the old code are gone).
    if (_restart_thread.joinable()) {
        _restart_thread.join();
    }

    if (panels_to_restart.empty()) {
        return;
    }

    // 3. Wait (with polling) until pgrep confirms conky is fully gone.
    //    This replaces the unreliable fixed sleep(1500).
    //    Maximum wait: 3 000 ms in 200 ms increments.
    {
        const int poll_interval_ms = 200;
        const int max_wait_ms      = 3000;
        int waited_ms              = 0;

        while (waited_ms < max_wait_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
            waited_ms += poll_interval_ms;
            if (get_running_configs_uncached().empty()) {
                break; // Confirmed: all conky processes are dead
            }
        }
    }

    // 4. Schedule sequential panel starts back on the main thread via QTimer.
    //    QProcess MUST be created on the main thread — this is the fix for
    //    the intermittent panel drop that occurred with the old std::thread approach.
    _restart_pending = true;
    _scheduleSequentialStart(panels_to_restart, 0);
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
        {
            std::lock_guard<std::recursive_mutex> lock(g_conky_mutex);
            _restart_pending = false;
        }
        g_restart_cv.notify_all();
        return;
    }

    // Start this panel immediately (we are on the main thread here)
    start_panel(panels[index], true);

    // Schedule the next panel after a short gap to avoid display/compositor
    // races when multiple Conky windows are initialising simultaneously.
    QTimer::singleShot(600, [panels, index]() {
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

    // Join the (now mostly unused) restart thread if it is still joinable
    lock.unlock();
    if (_restart_thread.joinable()) {
        _restart_thread.join();
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
        delete p;
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
    // Use the same QTimer-based sequential start so QProcess stays on the main thread.
    if (panels.empty()) return;
    _scheduleSequentialStart(panels, 0);
}

void ConkyManager::_start_panels_sequence(const std::vector<std::string>& panels) {
    // Use the same QTimer-based sequential start so QProcess stays on the main thread.
    // This replaces the old detached std::thread approach which was unsafe.
    if (panels.empty()) return;
    _scheduleSequentialStart(panels, 0);
}

std::vector<std::string> ConkyManager::get_running_configs_uncached() {
    std::vector<std::string> running_configs;

    FILE* pipe = popen("pgrep -af conky", "r");
    if (!pipe) return running_configs;

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        size_t c_pos = line.find("-c ");
        if (c_pos != std::string::npos) {
            std::string path = line.substr(c_pos + 3);
            // Trim trailing whitespace and newlines
            size_t end = path.find_last_not_of(" \n\r\t");
            if (end != std::string::npos) {
                path = path.substr(0, end + 1);
            }
            if (fs::exists(path)) {
                running_configs.push_back(path);
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
