#include "logger.h"
#include "app_info.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <ctime>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

bool Logger::initialize(const fs::path& log_dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return true;
    }
    
    // Determine log directory
    if (log_dir.empty()) {
        const char* home = std::getenv("HOME");
        if (home) {
            log_dir_ = fs::path(home) / ".local" / "share" / AppInfo::get_internal_name() / "logs";
        } else {
            log_dir_ = fs::temp_directory_path() / AppInfo::get_internal_name() / "logs";
        }
    } else {
        log_dir_ = log_dir;
    }
    
    // Create log directory if it doesn't exist
    try {
        fs::create_directories(log_dir_);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Failed to create log directory: " << e.what() << std::endl;
        return false;
    }
    
    // Generate log file name with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time_t);
    
    std::stringstream ss;
    ss << AppInfo::get_internal_name() << "_"
       << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".log";
    
    log_file_path_ = log_dir_ / ss.str();
    
    // Open log file
    log_file_.open(log_file_path_, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << log_file_path_ << std::endl;
        return false;
    }
    
    initialized_ = true;
    
    // Log initialization
    info("Logging system initialized", "Logger");
    info("Log file: " + log_file_path_.string(), "Logger");
    
    return true;
}

void Logger::debug(const std::string& message, const std::string& component) {
    log(LogLevel::DEBUG, message, component);
}

void Logger::info(const std::string& message, const std::string& component) {
    log(LogLevel::INFO, message, component);
}

void Logger::warning(const std::string& message, const std::string& component) {
    log(LogLevel::WARNING, message, component);
}

void Logger::error(const std::string& message, const std::string& component) {
    log(LogLevel::ERROR, message, component);
}

void Logger::critical(const std::string& message, const std::string& component) {
    log(LogLevel::CRITICAL, message, component);
}

void Logger::log(LogLevel level, const std::string& message, const std::string& component) {
    if (level < min_level_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        // If not initialized, output to console only
        if (console_output_) {
            std::cerr << "[" << level_to_string(level) << "] ";
            if (!component.empty()) {
                std::cerr << "[" << component << "] ";
            }
            std::cerr << message << std::endl;
        }
        return;
    }
    
    write_log(level, message, component);
}

void Logger::write_log(LogLevel level, const std::string& message, const std::string& component) {
    std::string timestamp = get_timestamp();
    std::string level_str = level_to_string(level);
    
    // Format: [TIMESTAMP] [LEVEL] [COMPONENT] MESSAGE
    std::stringstream log_entry;
    log_entry << "[" << timestamp << "] "
              << "[" << level_str << "] ";
    
    if (!component.empty()) {
        log_entry << "[" << component << "] ";
    }
    
    log_entry << message << std::endl;
    
    // Write to file
    if (log_file_.is_open()) {
        log_file_ << log_entry.str();
        log_file_.flush();
    }
    
    // Write to console if enabled
    if (console_output_) {
        if (level >= LogLevel::ERROR) {
            std::cerr << log_entry.str();
        } else {
            std::cout << log_entry.str();
        }
    }
}

std::string Logger::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm = *std::localtime(&time_t);
    
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
       << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string Logger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARNING:  return "WARNING";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default:                 return "UNKNOWN";
    }
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_file_.is_open()) {
        log_file_.flush();
    }
}

void Logger::cleanup_old_logs(int days_to_keep) {
    if (!fs::exists(log_dir_)) {
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::hours(24 * days_to_keep);
    
    try {
        for (const auto& entry : fs::directory_iterator(log_dir_)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            
            // Check if file is a log file
            std::string filename = entry.path().filename().string();
            if (filename.find(AppInfo::get_internal_name()) == std::string::npos ||
                filename.find(".log") == std::string::npos) {
                continue;
            }
            
            // Get file modification time
            auto ftime = fs::last_write_time(entry.path());
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            
            // Delete if older than cutoff
            if (sctp < cutoff) {
                fs::remove(entry.path());
                info("Cleaned up old log file: " + entry.path().filename().string(), "Logger");
            }
        }
    } catch (const fs::filesystem_error& e) {
        error("Failed to cleanup old logs: " + std::string(e.what()), "Logger");
    }
}