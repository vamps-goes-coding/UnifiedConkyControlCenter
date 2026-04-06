#pragma once

#include <string>
#include <exception>
#include <stdexcept>
#include <functional>
#include <optional>
#include "logger.h"

// Custom exception classes
class ConkyControlException : public std::runtime_error {
public:
    explicit ConkyControlException(const std::string& message)
        : std::runtime_error(message) {}
};

class ConfigException : public ConkyControlException {
public:
    explicit ConfigException(const std::string& message)
        : ConkyControlException("Configuration error: " + message) {}
};

class PanelException : public ConkyControlException {
public:
    explicit PanelException(const std::string& message)
        : ConkyControlException("Panel error: " + message) {}
};

class ThemeException : public ConkyControlException {
public:
    explicit ThemeException(const std::string& message)
        : ConkyControlException("Theme error: " + message) {}
};

class FileException : public ConkyControlException {
public:
    explicit FileException(const std::string& message)
        : ConkyControlException("File error: " + message) {}
};

// Error handling utilities
class ErrorHandler {
public:
    // Execute function with error handling and logging
    template<typename Func, typename... Args>
    static auto execute_with_handling(Func&& func, Args&&... args)
        -> std::optional<decltype(func(std::forward<Args>(args)...))>;
    
    // Execute function with error handling, returning default value on error
    template<typename Func, typename DefaultType, typename... Args>
    static auto execute_with_default(Func&& func, DefaultType default_value, Args&&... args)
        -> decltype(func(std::forward<Args>(args)...));
    
    // Execute function with error handling, calling error callback on failure
    template<typename Func, typename ErrorCallback, typename... Args>
    static bool execute_with_callback(Func&& func, ErrorCallback error_callback, Args&&... args);
    
    // Log and show error to user
    static void handle_error(const std::exception& e, const std::string& component = "");
    
    // Log and show warning to user
    static void handle_warning(const std::string& message, const std::string& component = "");
    
    // Check if operation succeeded, log and throw if not
    static void check_result(bool success, const std::string& error_message);
    
    // Get last error message
    static std::string get_last_error() { return last_error_; }
    
    // Clear last error
    static void clear_last_error() { last_error_.clear(); }
    
private:
    static std::string last_error_;
};

// Template implementations
template<typename Func, typename... Args>
auto ErrorHandler::execute_with_handling(Func&& func, Args&&... args)
    -> std::optional<decltype(func(std::forward<Args>(args)...))>
{
    try {
        return func(std::forward<Args>(args)...);
    } catch (const std::exception& e) {
        handle_error(e);
        return std::nullopt;
    }
}

template<typename Func, typename DefaultType, typename... Args>
auto ErrorHandler::execute_with_default(Func&& func, DefaultType default_value, Args&&... args)
    -> decltype(func(std::forward<Args>(args)...))
{
    try {
        return func(std::forward<Args>(args)...);
    } catch (const std::exception& e) {
        handle_error(e);
        return default_value;
    }
}

template<typename Func, typename ErrorCallback, typename... Args>
bool ErrorHandler::execute_with_callback(Func&& func, ErrorCallback error_callback, Args&&... args) {
    try {
        func(std::forward<Args>(args)...);
        return true;
    } catch (const std::exception& e) {
        handle_error(e);
        error_callback(e);
        return false;
    }
}

// Macro for easy error handling
#define HANDLE_ERRORS(component) \
    try {

#define END_HANDLE_ERRORS \
    } catch (const std::exception& e) { \
        ErrorHandler::handle_error(e, __FUNCTION__); \
    }

// Macro for checking results
#define CHECK_RESULT(success, message) \
    ErrorHandler::check_result(success, message)
