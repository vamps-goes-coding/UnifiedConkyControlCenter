#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace fs = std::filesystem;

class ThemeManager {
public:
    // Theme discovery and management
    static std::map<std::string, std::vector<std::string>> load_categories(bool scan_metadata = false);
    static std::vector<std::string> get_themes_for_category(const std::string& category_key);
    static std::vector<std::string> get_theme_colors(const std::string& theme_name, const std::string& category_key);
    static std::map<std::string, std::string> get_theme_metadata(const fs::path& theme_file);
    
    // Theme operations
    static bool apply_theme_to_panel(const std::string& theme_name, const std::string& category_key, const std::string& panel_name);
    static bool apply_global_theme(const std::string& theme_name, const std::string& category_key);
    static bool create_lua_theme(const std::string& character_name, const std::vector<std::string>& colors, const fs::path& output_dir, const std::string& category_name);
    
    // Theme file operations
    static bool copy_theme_to_panel(const fs::path& source_theme, const fs::path& destination_theme);
    static bool delete_theme(const std::string& theme_name, const std::string& category_key);
    static bool move_theme(const std::string& theme_name, const std::string& source_category, const std::string& target_category);
    
    // Category operations
    static bool create_category(const std::string& category_name);
    static bool delete_category(const std::string& category_name);
    static bool rename_category(const std::string& old_name, const std::string& new_name);
    
    // CSV operations
    static bool sync_category_with_csv(const std::string& category_name, const fs::path& csv_path);
    static bool export_category_to_csv(const std::string& category_name, const fs::path& csv_path);
    
    // Validation
    static bool validate_theme_file(const fs::path& theme_path);
    static bool validate_category_name(const std::string& category_name);
    
    // Cache control
    static void invalidate_cache();

    /// Resolved path to a theme .lua (handles category "Root" for loose files in themes/).
    static fs::path get_theme_file_path(const std::string& theme_name, const std::string& category_key);
    
    // Qt-style interface for main.cpp
    void applyCurrentTheme();
    void refreshThemes();
    
private:
    static fs::path get_category_directory(const std::string& category_key);
    static std::string normalize_category_key(const std::string& category_name);
    static std::string normalize_theme_filename(const std::string& theme_name);
    
    // Caching for improved performance
    static std::map<std::string, std::vector<std::string>> _cached_categories;
    static bool _categories_loaded;
};