#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

// Forward declarations for Qt classes to avoid including Qt headers in header file
class QWidget;
class QApplication;
class QMainWindow;
class QTabWidget;
class QSystemTrayIcon;
class QMenu;
class QAction;
class QTimer;
class QLabel;
class QPushButton;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QTextEdit;
class QCheckBox;
class QRadioButton;
class QSpinBox;
class QGridLayout;
class QVBoxLayout;
class QHBoxLayout;
class QFrame;
class QFileDialog;
class QMessageBox;
class QColorDialog;
class QDesktopServices;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;
class QHeaderView;
class QSplitter;
class QStatusBar;
class QMenuBar;
class QGroupBox;
class QScrollArea;
class QPainter;
class QPixmap;
class QDateTime;
class QProcess;
class QUrl;

class UIManager {
public:
    // Application lifecycle
    static int initialize_application(int argc, char* argv[]);
    static void run_application();
    static void quit_application();
    
    // Main window management
    static QWidget* create_main_window();
    static void setup_main_window(QWidget* window);
    static void show_main_window();
    static void hide_main_window();
    
    // Application styling
    static void apply_app_theme(const std::string& theme_name);
    
    // Tab management
    static QTabWidget* create_tab_widget(QWidget* parent);
    static void add_tab(QTabWidget* tab_widget, QWidget* tab_content, const std::string& tab_name);
    static void remove_tab(QTabWidget* tab_widget, int index);
    
    // Theme tab functionality
    static QWidget* create_theme_tab(QWidget* parent);
    static void refresh_categories(QListWidget* category_list, QListWidget* theme_list = nullptr, bool scan_metadata = false);
    static void load_themes_for_category(QListWidget* theme_list, const std::string& category_key);
    static void update_theme_preview(QWidget* preview_container, const std::vector<std::string>& colors);
    static void apply_global_theme(const std::string& theme_name, const std::string& category_key);
    
    // Gap adjustment tab functionality
    static QWidget* create_gap_tab(QWidget* parent);
    static void refresh_panels(QComboBox* panel_combo);
    static void load_gap_values(QSpinBox* gap_x_spin, QSpinBox* gap_y_spin, const std::string& panel_name);
    static void apply_gap_changes(const std::string& panel_name, int gap_x, int gap_y);
    
    // Start/Stop tab functionality
    static QWidget* create_start_stop_tab(QWidget* parent);
    static void refresh_panel_status(QTableWidget* panel_table, QGridLayout* status_grid);
    static void start_all_panels();
    static void stop_all_panels();
    static void restart_active_panels();
    
    // Editor tab functionality
    static QWidget* create_editor_tab(QWidget* parent);
    static void refresh_editors(QGridLayout* editors_layout);
    static void open_editor(const std::string& command, const std::string& panel_name);
    
    // Theme creator tab
    static QWidget* create_theme_creator_tab(QWidget* parent);
    static void preview_conversion(QWidget* dialog);
    static bool convert_themes(QWidget* dialog);
    static void clear_theme_creator_fields(QWidget* dialog);
    
    // Theme editor tab
    static QWidget* create_theme_editor_tab(QWidget* parent);
    static void load_theme_colors(QComboBox* category_combo, QComboBox* theme_combo, std::vector<QLineEdit*> color_inputs);
    static bool save_theme(const std::string& category_key, const std::string& theme_name, const std::vector<std::string>& colors);
    static void delete_theme(const std::string& category_key, const std::string& theme_name);
    
    // Theme manager tab
    static QWidget* create_theme_manager_tab(QWidget* parent);
    static void move_themes(const std::vector<std::string>& theme_names, const std::string& source_category, const std::string& target_category);
    static void delete_themes(const std::vector<std::string>& theme_names, const std::string& category);
    static void sync_category_with_csv(const std::string& category, const std::string& csv_path);
    static void export_category_to_csv(const std::string& category, const std::string& csv_path);
    
    // System tray
    static void setup_system_tray();
    static void show_tray_message(const std::string& title, const std::string& message);
    
    // Status bar
    static QWidget* create_status_bar(QWidget* parent);
    static void update_status_bar(QLabel* status_label, double seconds_since_refresh);
    
    // Dialogs and messages
    static int show_message_box(const std::string& title, const std::string& message, int buttons = 0);
    static std::string show_input_dialog(const std::string& title, const std::string& label);
    static std::string show_file_dialog(const std::string& title, const std::string& filter, bool save = false);
    static std::string show_color_picker(const std::string& title, const std::string& initial_color);
    
    // Utility functions
    static void set_widget_enabled(QWidget* widget, bool enabled);
    static void set_widget_visible(QWidget* widget, bool visible);
    static std::string get_widget_text(QWidget* widget);
    static void set_widget_text(QWidget* widget, const std::string& text);
    static void connect_signal(QWidget* sender, const std::string& signal, std::function<void()> slot);
    
    // Qt-style interface for main.cpp
    static void createMainWindow();
    static void createThemeTab();
    static void createGapTab();
    static void createControlTab();
    static void updateStatus();
    static void showTrayMessage(const std::string& message);
    
private:
    static QApplication* app_instance;
    static QWidget* main_window_instance;
    static QSystemTrayIcon* tray_icon_instance;
    
    static void initialize_qt_application(int argc, char* argv[]);
    static QWidget* create_control_center_ui();
    static QWidget* create_theme_center_ui();
    static void setup_mode_switching();
    static void refresh_all_tabs();
};