#pragma once

#include "config_manager.h"
#include <string>

namespace AppInfo {

/// Shown in window title, About, tray, and .desktop Name=
inline std::string get_display_name() {
    return ConfigManager::instance().get_display_name();
}

/// QSettings / internal id (no spaces)
inline std::string get_internal_name() {
    return ConfigManager::instance().get_internal_name();
}

inline std::string get_version() {
    return APP_VERSION_STR; // Remove fallback to ConfigManager
}

inline std::string get_organization() {
    return ConfigManager::instance().get_organization();
}

// Legacy compatibility - these return const char* for backward compatibility
inline const char* kDisplayName() {
    static std::string name = get_display_name();
    return name.c_str();
}

inline const char* kInternalName() {
    static std::string name = get_internal_name();
    return name.c_str();
}

inline const char* kVersion() {
    return APP_VERSION_STR; // Force compile-time constant
}

inline const char* kOrganization() {
    static std::string org = get_organization();
    return org.c_str();
}

}  // namespace AppInfo