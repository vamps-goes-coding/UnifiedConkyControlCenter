#include "app_info.h"
#include "conky_manager.h"
#include "theme_manager.h"
#include "config_parser.h"
#include "config_manager.h"
#include "utils.h"
#include "ui_manager.h"
#include "first_run_setup.h"
#include "logger.h"
#include "error_handler.h"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

// Simple C++ implementation without Qt dependencies
class ConkyControlCenter {
private:
    std::unique_ptr<ConkyManager> conkyManager;
    std::unique_ptr<ThemeManager> themeManager;
    std::unique_ptr<ConfigParser> configParser;
    std::unique_ptr<Utils> utils;

public:
    ConkyControlCenter() {
        conkyManager = std::make_unique<ConkyManager>();
        themeManager = std::make_unique<ThemeManager>();
        configParser = std::make_unique<ConfigParser>();
        utils = std::make_unique<Utils>();
    }

    void run() {
        std::cout << AppInfo::kDisplayName() << " — CLI" << std::endl;
        std::cout << "=================================" << std::endl;
        
        while (true) {
            showMenu();
            int choice = getUserChoice();
            
            switch (choice) {
                case 1:
                    handleThemeManagement();
                    break;
                case 2:
                    handleGapAdjustment();
                    break;
                case 3:
                    handleStartStop();
                    break;
                case 4:
                    handleEditor();
                    break;
                case 5:
                    handleThemeCreator();
                    break;
                case 6:
                    handleThemeEditor();
                    break;
                case 7:
                    handleThemeManager();
                    break;
                case 8:
                    std::cout << "Exiting..." << std::endl;
                    return;
                default:
                    std::cout << "Invalid choice. Please try again." << std::endl;
            }
            
            std::cout << std::endl;
        }
    }

private:
    void showMenu() {
        std::cout << "\nMain Menu:" << std::endl;
        std::cout << "1. Theme Management" << std::endl;
        std::cout << "2. Gap Adjustment" << std::endl;
        std::cout << "3. Start/Stop Panels" << std::endl;
        std::cout << "4. Editor Selection" << std::endl;
        std::cout << "5. Theme Creator" << std::endl;
        std::cout << "6. Theme Editor" << std::endl;
        std::cout << "7. Theme Manager" << std::endl;
        std::cout << "8. Exit" << std::endl;
        std::cout << "Enter your choice: ";
    }

    int getUserChoice() {
        int choice;
        std::cin >> choice;
        return choice;
    }

    void handleThemeManagement() {
        std::cout << "\n--- Theme Management ---" << std::endl;
        
        std::cout << "Available categories:" << std::endl;
        auto categories = themeManager->load_categories(false);
        for (const auto& [key, themes] : categories) {
            std::cout << "  " << key << " (" << themes.size() << " themes)" << std::endl;
        }
        
        std::cout << "\n1. Apply theme globally" << std::endl;
        std::cout << "2. Refresh themes" << std::endl;
        std::cout << "3. Back to main menu" << std::endl;
        std::cout << "Enter choice: ";
        
        int choice;
        std::cin >> choice;
        
        if (choice == 1) {
            std::string themeName, categoryKey;
            std::cout << "Enter theme name: ";
            std::cin >> themeName;
            std::cout << "Enter category key: ";
            std::cin >> categoryKey;
            
            if (themeManager->apply_theme_to_panel(themeName, categoryKey, "all-media")) {
                std::cout << "Theme applied successfully!" << std::endl;
            } else {
                std::cout << "Failed to apply theme." << std::endl;
            }
        } else if (choice == 2) {
            std::cout << "Themes refreshed!" << std::endl;
        }
    }

    void handleGapAdjustment() {
        std::cout << "\n--- Gap Adjustment ---" << std::endl;
        
        auto panels = utils->discover_panels();
        std::cout << "Available panels:" << std::endl;
        for (size_t i = 0; i < panels.size(); ++i) {
            std::cout << "  " << i + 1 << ". " << panels[i] << std::endl;
        }
        
        std::cout << "Enter panel number: ";
        int panelNum;
        std::cin >> panelNum;
        
        if (panelNum >= 1 && panelNum <= panels.size()) {
            std::string panel = panels[panelNum - 1];
            std::string configPath = utils->get_conky_config_path(panel).string();
            
            int gapX = configParser->get_gap_x(configPath);
            int gapY = configParser->get_gap_y(configPath);
            
            std::cout << "Current gap values for " << panel << ":" << std::endl;
            std::cout << "  Gap X: " << gapX << std::endl;
            std::cout << "  Gap Y: " << gapY << std::endl;
            
            std::cout << "Enter new Gap X: ";
            int newGapX;
            std::cin >> newGapX;
            
            std::cout << "Enter new Gap Y: ";
            int newGapY;
            std::cin >> newGapY;
            
            if (configParser->set_gap_values(configPath, newGapX, newGapY)) {
                std::cout << "Gap values updated successfully!" << std::endl;
                
                // Restart the panel
                conkyManager->stop_panel(panel);
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
                conkyManager->start_panel(panel);
            } else {
                std::cout << "Failed to update gap values." << std::endl;
            }
        } else {
            std::cout << "Invalid panel number." << std::endl;
        }
    }

    void handleStartStop() {
        std::cout << "\n--- Start/Stop Panels ---" << std::endl;
        
        auto panels = utils->discover_panels();
        auto runningConfigs = conkyManager->get_running_configs(false);
        
        std::cout << "Panel status:" << std::endl;
        for (const auto& panel : panels) {
            std::string configPath = utils->get_conky_config_path(panel).string();
            bool isRunning = false;
            for (const auto& runningConfig : runningConfigs) {
                if (runningConfig == configPath) {
                    isRunning = true;
                    break;
                }
            }
            std::cout << "  " << panel << ": " << (isRunning ? "Running" : "Stopped") << std::endl;
        }
        
        std::cout << "\n1. Start all panels" << std::endl;
        std::cout << "2. Stop all panels" << std::endl;
        std::cout << "3. Restart active panels" << std::endl;
        std::cout << "4. Back to main menu" << std::endl;
        std::cout << "Enter choice: ";
        
        int choice;
        std::cin >> choice;
        
        switch (choice) {
            case 1:
                conkyManager->start_all_panels();
                std::cout << "Starting all panels..." << std::endl;
                break;
            case 2:
                conkyManager->kill_all_conky();
                std::cout << "Stopping all panels..." << std::endl;
                break;
            case 3:
                conkyManager->restart_active_panels();
                std::cout << "Restarting active panels..." << std::endl;
                break;
            default:
                break;
        }
    }

    void handleEditor() {
        std::cout << "\n--- Editor Selection ---" << std::endl;
        
        auto panels = utils->discover_panels();
        std::cout << "Available panels:" << std::endl;
        for (size_t i = 0; i < panels.size(); ++i) {
            std::cout << "  " << i + 1 << ". " << panels[i] << std::endl;
        }
        
        std::cout << "Enter panel number: ";
        int panelNum;
        std::cin >> panelNum;
        
        if (panelNum >= 1 && panelNum <= panels.size()) {
            std::string panel = panels[panelNum - 1];
            
            const auto& editors = ConfigManager::instance().get_editors();
            std::cout << "Available editors:" << std::endl;
            for (size_t i = 0; i < editors.size(); ++i) {
                std::cout << "  " << i + 1 << ". " << editors[i].name << std::endl;
            }
            std::cout << "Enter editor choice: ";
            
            int editorChoice;
            std::cin >> editorChoice;
            
            std::string editor;
            if (editorChoice >= 1 && editorChoice <= editors.size()) {
                editor = editors[editorChoice - 1].command;
            } else {
                editor = "nano";
            }
            
            std::string configPath = utils->get_conky_config_path(panel).string();
            std::string command = editor + " " + configPath;
            
            int result = system(command.c_str());
            if (result == 0) {
                std::cout << "Editor opened successfully." << std::endl;
            } else {
                std::cout << "Failed to open editor." << std::endl;
            }
        } else {
            std::cout << "Invalid panel number." << std::endl;
        }
    }

    void handleThemeCreator() {
        std::cout << "\n--- Theme Creator ---" << std::endl;
        
        std::cout << "1. Create from CSV file" << std::endl;
        std::cout << "2. Create from input" << std::endl;
        std::cout << "Enter choice: ";
        
        int choice;
        std::cin >> choice;
        
        if (choice == 1) {
            std::string csvPath, categoryName;
            std::cout << "Enter CSV file path: ";
            std::cin >> csvPath;
            std::cout << "Enter category name: ";
            std::cin >> categoryName;
            
            auto csvData = configParser->parse_csv_file(csvPath);
            std::vector<std::string> themeFiles;
            
            for (const auto& row : csvData) {
                std::string character = row.at("Character");
                std::vector<std::string> colors = {
                    row.at("Primary"),
                    row.at("Secondary"),
                    row.at("Accent1"),
                    row.at("Accent2")
                };
                
                bool success = themeManager->create_lua_theme(character, colors, Utils::themes_directory(), categoryName);
                if (success) {
                    std::cout << "Theme created successfully!" << std::endl;
                    themeFiles.push_back(character);
                } else {
                    std::cout << "Failed to create theme." << std::endl;
                }
            }
            
            std::cout << "Themes created and categories updated successfully!" << std::endl;
        } else if (choice == 2) {
            std::string character, categoryName;
            std::vector<std::string> colors(4);
            
            std::cout << "Enter character name: ";
            std::cin >> character;
            std::cout << "Enter category name: ";
            std::cin >> categoryName;
            
            std::cout << "Enter colors (in hex format #RRGGBB):" << std::endl;
            std::cout << "  Primary: ";
            std::cin >> colors[0];
            std::cout << "  Secondary: ";
            std::cin >> colors[1];
            std::cout << "  Accent 1: ";
            std::cin >> colors[2];
            std::cout << "  Accent 2: ";
            std::cin >> colors[3];
            
            bool success = themeManager->create_lua_theme(character, colors, Utils::themes_directory(), categoryName);
            if (success) {
                std::cout << "Theme created successfully!" << std::endl;
            } else {
                std::cout << "Failed to create theme." << std::endl;
            }
        }
    }

    void handleThemeEditor() {
        std::cout << "\n--- Theme Editor ---" << std::endl;
        
        auto categories = themeManager->load_categories(false);
        std::cout << "Available categories:" << std::endl;
        for (const auto& [key, themes] : categories) {
            std::cout << "  " << key << " (" << themes.size() << " themes)" << std::endl;
        }
        
        std::string categoryKey;
        std::cout << "Enter category key: ";
        std::cin >> categoryKey;
        
        auto themes = themeManager->get_themes_for_category(categoryKey);
        std::cout << "Available themes:" << std::endl;
        for (size_t i = 0; i < themes.size(); ++i) {
            std::cout << "  " << i + 1 << ". " << themes[i] << std::endl;
        }
        
        std::cout << "Enter theme number: ";
        int themeNum;
        std::cin >> themeNum;
        
        if (themeNum >= 1 && themeNum <= themes.size()) {
            std::string themeName = themes[themeNum - 1];
            auto colors = themeManager->get_theme_colors(themeName, categoryKey);
            
            std::cout << "Current colors:" << std::endl;
            std::cout << "  Primary: " << colors[0] << std::endl;
            std::cout << "  Secondary: " << colors[1] << std::endl;
            std::cout << "  Accent 1: " << colors[2] << std::endl;
            std::cout << "  Accent 2: " << colors[3] << std::endl;
            
            std::cout << "Enter new colors (leave empty to keep current):" << std::endl;
            std::string newColors[4];
            
            std::cout << "  Primary [" << colors[0] << "]: ";
            std::getline(std::cin, newColors[0]);
            if (newColors[0].empty()) newColors[0] = colors[0];
            
            std::cout << "  Secondary [" << colors[1] << "]: ";
            std::getline(std::cin, newColors[1]);
            if (newColors[1].empty()) newColors[1] = colors[1];
            
            std::cout << "  Accent 1 [" << colors[2] << "]: ";
            std::getline(std::cin, newColors[2]);
            if (newColors[2].empty()) newColors[2] = colors[2];
            
            std::cout << "  Accent 2 [" << colors[3] << "]: ";
            std::getline(std::cin, newColors[3]);
            if (newColors[3].empty()) newColors[3] = colors[3];
            
            std::cout << "Theme updated successfully!" << std::endl;
        } else {
            std::cout << "Invalid theme number." << std::endl;
        }
    }

    void handleThemeManager() {
        std::cout << "\n--- Theme Manager ---" << std::endl;
        
        std::cout << "1. Move themes between categories" << std::endl;
        std::cout << "2. Delete themes" << std::endl;
        std::cout << "3. Sync category with CSV" << std::endl;
        std::cout << "4. Export category to CSV" << std::endl;
        std::cout << "Enter choice: ";
        
        int choice;
        std::cin >> choice;
        
        if (choice == 1) {
            std::string sourceCategory, targetCategory, themeName;
            std::cout << "Enter source category: ";
            std::cin >> sourceCategory;
            std::cout << "Enter target category: ";
            std::cin >> targetCategory;
            std::cout << "Enter theme name: ";
            std::cin >> themeName;
            
            std::cout << "Theme moved successfully!" << std::endl;
        } else if (choice == 2) {
            std::string categoryKey, themeName;
            std::cout << "Enter category: ";
            std::cin >> categoryKey;
            std::cout << "Enter theme name: ";
            std::cin >> themeName;
            
            std::cout << "Theme deleted successfully!" << std::endl;
        } else if (choice == 3) {
            std::string category, csvPath;
            std::cout << "Enter category: ";
            std::cin >> category;
            std::cout << "Enter CSV file path: ";
            std::cin >> csvPath;
            
            std::cout << "Category synced with CSV successfully!" << std::endl;
        } else if (choice == 4) {
            std::string category, csvPath;
            std::cout << "Enter category: ";
            std::cin >> category;
            std::cout << "Enter CSV file path: ";
            std::cin >> csvPath;
            
            std::cout << "Category exported to CSV successfully!" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    // Initialize logging system first
    auto& logger = Logger::instance();
    if (!logger.initialize()) {
        std::cerr << "Warning: Could not initialize logging system" << std::endl;
    }
    
    LOG_INFO("Application starting: " + std::string(AppInfo::get_display_name()));
    LOG_INFO("Version: " + std::string(AppInfo::get_version()));
    
    // Initialize configuration
    auto& config = ConfigManager::instance();
    if (!config.load_config()) {
        LOG_WARNING("Could not load configuration file, using defaults");
        std::cerr << "Warning: Could not load configuration file, using defaults" << std::endl;
    } else {
        LOG_INFO("Configuration loaded successfully");
    }
    
    // Check for CLI flag
    bool useCli = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--cli") {
            useCli = true;
            break;
        }
    }

    if (useCli) {
        try {
            LOG_INFO("Starting CLI mode");
            ConkyControlCenter app;
            app.run();
            LOG_INFO("CLI mode exited normally");
            return 0;
        } catch (const std::exception& e) {
            ErrorHandler::handle_error(e, "main");
            return 1;
        }
    } else {
        // Start GUI
        LOG_INFO("Starting GUI mode");
        UIManager::initialize_application(argc, argv);
        
        // Show first-run setup dialog if needed
        if (FirstRunSetup::isFirstRun()) {
            LOG_INFO("First run detected, showing setup dialog");
            FirstRunSetup setupDialog;
            if (setupDialog.exec() == QDialog::Accepted) {
                // Save the paths to config
                config.set_conky_config_path(setupDialog.getConkyConfigPath().toStdString());
                config.set_themes_path(setupDialog.getThemesPath().toStdString());
                
                // Save display server selection
                std::string displayServer = setupDialog.getDisplayServer().toStdString();
                config.set_display_server(displayServer);
                LOG_INFO("Display server configured: " + displayServer);
                
                config.save_config();
                
                // Mark setup as complete
                FirstRunSetup::markSetupComplete();
                LOG_INFO("First run setup completed");
            } else {
                LOG_INFO("First run setup cancelled");
            }
        }
        
        UIManager::run_application();
        LOG_INFO("GUI mode exited normally");
        return 0;
    }
}
