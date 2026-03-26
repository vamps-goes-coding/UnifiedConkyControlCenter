#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <filesystem>
#include <chrono>
#include <sstream>

namespace fs = std::filesystem;

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class Logger {
public:
    // Singleton pattern
    static Logger& instance();
    
    // Initialize logging system
    bool initialize(const fs::path& log_dir = "");
    
    // Logging methods
    void debug(const std::string& message, const std::string& component = "");
    void info(const std::string& message, const std::string& component = "");
    void warning(const std::string& message, const std::string& component = "");
    void error(const std::string& message, const std::string& component = "");
    void critical(const std::string& message, const std::string& component = "");
    
    // Generic log method
    void log(LogLevel level, const std::string& message, const std::string& component = "");
    
    // Get log file path
    fs::path get_log_file_path() const { return log_file_path_; }
    
    // Set minimum log level
    void set_min_level(LogLevel level) { min_level_ = level; }
    
    // Enable/disable console output
    void set_console_output(bool enabled) { console_output_ = enabled; }
    
    // Flush logs to disk
    void flush();
    
    // Clean up old log files (keep last N days)
    void cleanup_old_logs(int days_to_keep = 30);
    
private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string get_timestamp() const;
    std::string level_to_string(LogLevel level) const;
    void write_log(LogLevel level, const std::string& message, const std::string& component);
    
    fs::path log_dir_;
    fs::path log_file_path_;
    std::ofstream log_file_;
    std::mutex mutex_;
    LogLevel min_level_ = LogLevel::DEBUG;
    bool console_output_ = true;
    bool initialized_ = false;
};

// Convenience macros for logging
#define LOG_DEBUG(msg) Logger::instance().debug(msg, __FUNCTION__)
#define LOG_INFO(msg) Logger::instance().info(msg, __FUNCTION__)
#define LOG_WARNING(msg) Logger::instance().warning(msg, __FUNCTION__)
#define LOG_ERROR(msg) Logger::instance().error(msg, __FUNCTION__)
#define LOG_CRITICAL(msg) Logger::instance().critical(msg, __FUNCTION__)

// Component-specific logging macros
#define LOG_DEBUG_COMP(msg, comp) Logger::instance().debug(msg, comp)
#define LOG_INFO_COMP(msg, comp) Logger::instance().info(msg, comp)
#define LOG_WARNING_COMP(msg, comp) Logger::instance().warning(msg, comp)
#define LOG_ERROR_COMP(msg, comp) Logger::instance().error(msg, comp)
#define LOG_CRITICAL_COMP(msg, comp) Logger::instance().critical(msg, comp)