#include "config_parser.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string_view>

// Modern C++ improvements: use std::string_view for better performance
std::string_view trim_view(std::string_view str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// Modern C++ improvements: use constexpr for regex patterns
// Note: std::regex requires std::string, not std::string_view
const std::string GAP_X_PATTERN = "gap_x\\s*=\\s*(-?\\d+)";
const std::string GAP_Y_PATTERN = "gap_y\\s*=\\s*(-?\\d+)";
const std::string COLOR_PATTERN = "color(\\d+)\\s*=\\s*'([^']+)'";
const std::string META_NAME_PATTERN = "name\\s*=\\s*\"([^\"]+)\"";
const std::string META_CATEGORY_PATTERN = "category\\s*=\\s*\"([^\"]+)\"";

// Static member definitions for pre-compiled regex patterns
const std::regex ConfigParser::RE_GAP_X(GAP_X_PATTERN);
const std::regex ConfigParser::RE_GAP_Y(GAP_Y_PATTERN);
const std::regex ConfigParser::RE_COLOR(COLOR_PATTERN);
const std::regex ConfigParser::RE_META_NAME(META_NAME_PATTERN);
const std::regex ConfigParser::RE_META_CATEGORY(META_CATEGORY_PATTERN);
const std::regex ConfigParser::RE_CATEGORY_PATTERN("\\{\\s*\"([^\"]+)\"\\s*,\\s*\"[^\"]*\"\\s*\\}");

std::map<std::string, std::string> ConfigParser::parse_conky_config(const fs::path& config_path) {
    std::map<std::string, std::string> config;
    
    if (!fs::exists(config_path)) {
        throw std::runtime_error("Configuration file not found: " + config_path.string());
    }
    
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open configuration file: " + config_path.string());
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Modern C++ improvements: use string_view for better performance
        std::string_view line_view = line;
        size_t pos = line_view.find('=');
        if (pos != std::string_view::npos) {
            std::string_view key_view = trim_view(line_view.substr(0, pos));
            std::string_view value_view = trim_view(line_view.substr(pos + 1));
            
            config[std::string(key_view)] = std::string(value_view);
        }
    }
    
    file.close();
    return config;
}

bool ConfigParser::update_conky_config(const fs::path& config_path, const std::map<std::string, std::string>& updates) {
    if (!fs::exists(config_path)) {
        throw std::runtime_error("Configuration file not found: " + config_path.string());
    }
    
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open configuration file: " + config_path.string());
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    // Modern C++ improvements: use range-based for loop
    for (const auto& [key, value] : updates) {
        std::string pattern = std::string(key) + "\\s*=\\s*[^\\n]+";
        std::regex regex_pattern(pattern);
        std::string replacement = key + " = " + value;
        content = std::regex_replace(content, regex_pattern, replacement);
    }
    
    // Write back to file
    std::ofstream out_file(config_path);
    if (!out_file.is_open()) {
        throw std::runtime_error("Failed to write to configuration file: " + config_path.string());
    }
    
    out_file << content;
    out_file.close();
    return true;
}

int ConfigParser::get_gap_x(const fs::path& config_path) {
    if (!fs::exists(config_path)) {
        throw std::runtime_error("Configuration file not found: " + config_path.string());
    }
    
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open configuration file: " + config_path.string());
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, RE_GAP_X)) {
            file.close();
            return std::stoi(match[1]);
        }
    }
    
    file.close();
    throw std::runtime_error("Gap X value not found in configuration file: " + config_path.string());
}

int ConfigParser::get_gap_y(const fs::path& config_path) {
    if (!fs::exists(config_path)) {
        throw std::runtime_error("Configuration file not found: " + config_path.string());
    }
    
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open configuration file: " + config_path.string());
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, RE_GAP_Y)) {
            file.close();
            return std::stoi(match[1]);
        }
    }
    
    file.close();
    throw std::runtime_error("Gap Y value not found in configuration file: " + config_path.string());
}

bool ConfigParser::set_gap_values(const fs::path& config_path, int gap_x, int gap_y) {
    if (!fs::exists(config_path)) {
        throw std::runtime_error("Configuration file not found: " + config_path.string());
    }
    
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open configuration file: " + config_path.string());
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    // Modern C++ improvements: use std::regex_replace with pre-compiled patterns
    std::string new_gap_x = "gap_x = " + std::to_string(gap_x);
    std::string new_gap_y = "gap_y = " + std::to_string(gap_y);
    
    std::string replaced_x = std::regex_replace(content, RE_GAP_X, new_gap_x);
    std::string replaced_y = std::regex_replace(replaced_x, RE_GAP_Y, new_gap_y);
    
    // Validate that replacements actually occurred
    if (replaced_y == content) {
        std::cerr << "ERROR: Neither gap_x nor gap_y found in: " << config_path << std::endl;
        return false;
    }
    
    // Write back to file
    std::ofstream out_file(config_path);
    if (!out_file.is_open()) {
        throw std::runtime_error("Failed to write to configuration file: " + config_path.string());
    }
    
    out_file << replaced_y;
    out_file.close();
    return true;
}

std::vector<std::string> ConfigParser::extract_colors_from_theme(const fs::path& theme_path) {
    std::vector<std::string> colors;
    
    if (!fs::exists(theme_path)) {
        throw std::runtime_error("Theme file not found: " + theme_path.string());
    }
    
    std::ifstream file(theme_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open theme file: " + theme_path.string());
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, std::regex(COLOR_PATTERN))) {
            std::string color = match[2];
            colors.push_back(color);
        }
    }
    
    file.close();
    return colors;
}

bool ConfigParser::update_theme_colors(const fs::path& theme_path, const std::vector<std::string>& colors) {
    if (!fs::exists(theme_path) || colors.size() < 4) {
        throw std::invalid_argument("Invalid theme file or insufficient colors provided");
    }
    
    std::ifstream file(theme_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open theme file: " + theme_path.string());
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    // Modern C++ improvements: use range-based for loop with index
    for (size_t i = 0; i < 4; ++i) {
        std::string pattern = "color" + std::to_string(i + 1) + "\\s*=\\s*'[^']*'";
        std::regex regex_pattern(pattern);
        std::string replacement = "color" + std::to_string(i + 1) + " = '" + colors[i] + "'";
        content = std::regex_replace(content, regex_pattern, replacement);
    }
    
    // Write back to file
    std::ofstream out_file(theme_path);
    if (!out_file.is_open()) {
        throw std::runtime_error("Failed to write to theme file: " + theme_path.string());
    }
    
    out_file << content;
    out_file.close();
    return true;
}

std::map<std::string, std::string> ConfigParser::extract_theme_metadata(const fs::path& theme_file) {
    std::map<std::string, std::string> metadata;
    metadata["name"] = theme_file.stem().string();
    metadata["category"] = "Uncategorized";
    
    if (fs::exists(theme_file)) {
        std::ifstream file(theme_file);
        if (file.is_open()) {
            std::string line;
            
            while (std::getline(file, line)) {
                std::smatch match;
                
                if (std::regex_search(line, match, std::regex(META_NAME_PATTERN))) {
                    metadata["name"] = match[1];
                }
                
                if (std::regex_search(line, match, std::regex(META_CATEGORY_PATTERN))) {
                    metadata["category"] = match[1];
                }
            }
            file.close();
        }
    }
    
    return metadata;
}

std::map<std::string, std::vector<std::string>> ConfigParser::parse_categories_lua(const fs::path& categories_path) {
    std::map<std::string, std::vector<std::string>> categories;
    
    if (!fs::exists(categories_path)) {
        throw std::runtime_error("Categories file not found: " + categories_path.string());
    }
    
    std::ifstream file(categories_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open categories file: " + categories_path.string());
    }
    
    std::string content;
    std::string line;
    while (std::getline(file, line)) {
        content += line + "\n";
    }
    file.close();
    
    // Simple parsing of Lua table structure
    std::smatch match;
    std::string::const_iterator search_start(content.cbegin());
    
    while (std::regex_search(search_start, content.cend(), match, std::regex(RE_CATEGORY_PATTERN))) {
        std::string category_name = match[1];
        categories[category_name] = {}; // Empty vector for now
        search_start = match.suffix().first;
    }
    
    return categories;
}

bool ConfigParser::update_categories_lua(const fs::path& categories_path, const std::string& category_name, const std::vector<std::string>& themes, bool add_to_existing) {
    if (!fs::exists(categories_path)) {
        throw std::runtime_error("Categories file not found: " + categories_path.string());
    }
    
    std::ifstream file(categories_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open categories file: " + categories_path.string());
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    // This is a simplified implementation
    // In a real implementation, you'd need to properly parse and modify the Lua table structure
    if (add_to_existing) {
        // Add themes to existing category
        std::string pattern = "\"" + category_name + "\"";
        if (content.find(pattern) != std::string::npos) {
            // Category exists, add themes
            for (const auto& theme : themes) {
                content += "    {\"" + theme + "\", \"\"},\n";
            }
        }
    } else {
        // Create new category or replace existing
        std::string new_category = "    {\"" + category_name + "\", \"\"},\n";
        for (const auto& theme : themes) {
            new_category += "    {\"" + theme + "\", \"\"},\n";
        }
        
        // Remove existing category if it exists
        std::string pattern = "\\{[^}]*\"" + category_name + "\"[^}]*\\}";
        content = std::regex_replace(content, std::regex(pattern), "");
        
        // Add new category
        content += new_category;
    }
    
    // Write back to file
    std::ofstream out_file(categories_path);
    if (!out_file.is_open()) {
        throw std::runtime_error("Failed to write to categories file: " + categories_path.string());
    }
    
    out_file << content;
    out_file.close();
    return true;
}

std::vector<std::map<std::string, std::string>> ConfigParser::parse_csv_file(const fs::path& csv_path) {
    std::vector<std::map<std::string, std::string>> data;
    
    if (!fs::exists(csv_path)) {
        throw std::runtime_error("CSV file not found: " + csv_path.string());
    }
    
    std::ifstream file(csv_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open CSV file: " + csv_path.string());
    }
    
    std::string line;
    std::vector<std::string> headers;
    
    // Read headers
    if (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string header;
        while (std::getline(ss, header, ',')) {
            headers.push_back(std::string(trim_view(header)));
        }
    }
    
    // Read data rows
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string value;
        std::map<std::string, std::string> row;
        
        size_t col = 0;
        while (std::getline(ss, value, ',') && col < headers.size()) {
            row[headers[col]] = std::string(trim_view(value));
            col++;
        }
        
        if (!row.empty()) {
            data.push_back(row);
        }
    }
    
    file.close();
    return data;
}

bool ConfigParser::write_csv_file(const fs::path& csv_path, const std::vector<std::map<std::string, std::string>>& data, const std::vector<std::string>& headers) {
    std::ofstream file(csv_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open CSV file for writing: " + csv_path.string());
    }
    
    // Write headers
    for (size_t i = 0; i < headers.size(); ++i) {
        file << headers[i];
        if (i < headers.size() - 1) {
            file << ",";
        }
    }
    file << "\n";
    
    // Write data
    for (const auto& row : data) {
        for (size_t i = 0; i < headers.size(); ++i) {
            file << (row.count(headers[i]) ? row.at(headers[i]) : "");
            if (i < headers.size() - 1) {
                file << ",";
            }
        }
        file << "\n";
    }
    
    file.close();
    return true;
}

bool ConfigParser::is_valid_color(const std::string& color_str) {
    return is_valid_hex_color(color_str);
}

bool ConfigParser::is_valid_hex_color(const std::string& color_str) {
    if (color_str.empty() || color_str[0] != '#') {
        return false;
    }
    
    std::string hex = color_str.substr(1);
    if (hex.length() != 6 && hex.length() != 3) {
        return false;
    }
    
    for (char c : hex) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }
    
    return true;
}

std::string ConfigParser::normalize_color(const std::string& color_str) {
    if (color_str.empty()) {
        return "#FFFFFF";
    }
    
    if (color_str[0] == '#') {
        return color_str;
    }
    
    return "#" + color_str;
}

std::string ConfigParser::create_lua_theme_content(const std::string& character_name, const std::vector<std::string>& colors, const std::string& category) {
    std::string content = "theme = {\n";
    content += "    name = \"" + character_name + "\",\n";
    content += "    category = \"" + category + "\",\n";
    
    for (size_t i = 0; i < colors.size() && i < 4; ++i) {
        content += "    color" + std::to_string(i + 1) + " = '" + colors[i] + "',\n";
    }
    
    content += "}\n";
    return content;
}

std::string ConfigParser::normalize_filename(const std::string& name) {
    std::string result = name;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    std::replace(result.begin(), result.end(), ' ', '-');
    result.erase(std::remove(result.begin(), result.end(), '('), result.end());
    result.erase(std::remove(result.begin(), result.end(), ')'), result.end());
    result.erase(std::remove(result.begin(), result.end(), '.'), result.end());
    return result;
}

// Qt-style interface implementations
bool ConfigParser::loadConfig(const std::string& configPath) {
    auto config = parse_conky_config(fs::path(configPath));
    return !config.empty();
}

std::string ConfigParser::getValue(const std::string& key) {
    // This would need to be implemented with proper config storage
    return "";
}

bool ConfigParser::setValue(const std::string& key, const std::string& value) {
    // This would need to be implemented with proper config storage
    return true;
}

bool ConfigParser::saveConfig(const std::string& configPath) {
    // This would need to be implemented with proper config storage
    return true;
}
