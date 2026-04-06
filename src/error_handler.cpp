#include "error_handler.h"
#include <iostream>

// Static member initialization
std::string ErrorHandler::last_error_;

void ErrorHandler::handle_error(const std::exception& e, const std::string& component) {
    std::string error_message = e.what();
    last_error_ = error_message;
    
    // Log the error
    if (component.empty()) {
        LOG_ERROR(error_message);
    } else {
        LOG_ERROR_COMP(error_message, component);
    }
    
    // Show error to user (could be enhanced with GUI dialog)
    std::cerr << "Error: " << error_message << std::endl;
}

void ErrorHandler::handle_warning(const std::string& message, const std::string& component) {
    // Log the warning
    if (component.empty()) {
        LOG_WARNING(message);
    } else {
        LOG_WARNING_COMP(message, component);
    }
    
    // Show warning to user (could be enhanced with GUI dialog)
    std::cout << "Warning: " << message << std::endl;
}

void ErrorHandler::check_result(bool success, const std::string& error_message) {
    if (!success) {
        last_error_ = error_message;
        LOG_ERROR(error_message);
        throw ConkyControlException(error_message);
    }
}