#pragma once

#include <string>
#include <vector>
#include <map>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

class ConfigParser {
public:
    // Configuration file parsing
    static std::map<std::string, std::string> parse_conky_config(const fs::path& config_path);
    static bool update_conky_config(const fs::path& config_path, const std::map<std::string, std::string>& updates);
    
    // Gap value extraction and modification
    static int get_gap_x(const fs::path& config_path);
    static int get_gap_y(const fs::path& config_path);
    static bool set_gap_values(const fs::path& config_path, int gap_x, int gap_y);
    
    // Color extraction from Lua themes
    static std::vector<std::string> extract_colors_from_theme(const fs::path& theme_path);
    static bool update_theme_colors(const fs::path& theme_path, const std::vector<std::string>& colors);
    
    // Theme metadata extraction
    static std::map<std::string, std::string> extract_theme_metadata(const fs::path& theme_path);
    
    // Categories.lua parsing
    static std::map<std::string, std::vector<std::string>> parse_categories_lua(const fs::path& categories_path);
    static bool update_categories_lua(const fs::path& categories_path, const std::string& category_name, const std::vector<std::string>& themes, bool add_to_existing = false);
    
    // CSV file parsing
    static std::vector<std::map<std::string, std::string>> parse_csv_file(const fs::path& csv_path);
    static bool write_csv_file(const fs::path& csv_path, const std::vector<std::map<std::string, std::string>>& data, const std::vector<std::string>& headers);
    
    // Validation
    static bool is_valid_color(const std::string& color_str);
    static bool is_valid_hex_color(const std::string& color_str);
    
    // Qt-style interface for main.cpp
    bool loadConfig(const std::string& configPath);
    std::string getValue(const std::string& key);
    bool setValue(const std::string& key, const std::string& value);
    bool saveConfig(const std::string& configPath);
    
private:
    // Pre-compiled regex patterns (equivalent to Python script)
    static const std::regex RE_GAP_X;
    static const std::regex RE_GAP_Y;
    static const std::regex RE_COLOR;
    static const std::regex RE_META_NAME;
    static const std::regex RE_META_CATEGORY;
    static const std::regex RE_CATEGORY_PATTERN;
    
    static std::string normalize_color(const std::string& color_str);
    static std::string create_lua_theme_content(const std::string& character_name, const std::vector<std::string>& colors, const std::string& category);
    static std::string normalize_filename(const std::string& name);
};