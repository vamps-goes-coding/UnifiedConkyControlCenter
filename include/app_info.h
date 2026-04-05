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
#ifdef APP_VERSION_STR
    return APP_VERSION_STR;
#else
    return ConfigManager::instance().get_version();
#endif
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
#ifdef APP_VERSION_STR
    return APP_VERSION_STR;
#else
    static std::string v = get_version();
    return v.c_str();
#endif
}

inline const char* kOrganization() {
    static std::string org = get_organization();
    return org.c_str();
}

}  // namespace AppInfo