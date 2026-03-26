#include "preferences_dialog.h"
#include "config_manager.h"
#include "logger.h"
#include "display_server.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QApplication>

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setup_ui();
    load_preferences();
}

void PreferencesDialog::show_preferences(QWidget* parent) {
    PreferencesDialog dialog(parent);
    dialog.exec();
}

void PreferencesDialog::setup_ui() {
    setWindowTitle("Preferences");
    setMinimumWidth(600);
    setMinimumHeight(500);
    
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    
    // Tab widget
    QTabWidget* tab_widget = new QTabWidget();
    
    // General tab
    QWidget* general_tab = new QWidget();
    QVBoxLayout* general_layout = new QVBoxLayout(general_tab);
    
    QGroupBox* startup_group = new QGroupBox("Startup");
    QVBoxLayout* startup_layout = new QVBoxLayout(startup_group);
    auto_start_checkbox_ = new QCheckBox("Start automatically on system login");
    minimize_to_tray_checkbox_ = new QCheckBox("Minimize to system tray on close");
    show_notifications_checkbox_ = new QCheckBox("Show system notifications");
    startup_layout->addWidget(auto_start_checkbox_);
    startup_layout->addWidget(minimize_to_tray_checkbox_);
    startup_layout->addWidget(show_notifications_checkbox_);
    general_layout->addWidget(startup_group);
    
    QGroupBox* display_group = new QGroupBox("Display Server");
    QVBoxLayout* display_layout = new QVBoxLayout(display_group);
    display_layout->addWidget(new QLabel("Display server (requires restart):"));
    display_server_combo_ = new QComboBox();
    display_server_combo_->addItem("Auto-detect", "auto");
    display_server_combo_->addItem("X11", "x11");
    display_server_combo_->addItem("Wayland", "wayland");
    display_layout->addWidget(display_server_combo_);
    general_layout->addWidget(display_group);
    
    general_layout->addStretch();
    tab_widget->addTab(general_tab, "General");
    
    // Editor tab
    QWidget* editor_tab = new QWidget();
    QVBoxLayout* editor_layout = new QVBoxLayout(editor_tab);
    
    QGroupBox* font_group = new QGroupBox("Font");
    QGridLayout* font_layout = new QGridLayout(font_group);
    font_layout->addWidget(new QLabel("Font family:"), 0, 0);
    font_family_combo_ = new QFontComboBox();
    font_layout->addWidget(font_family_combo_, 0, 1);
    font_layout->addWidget(new QLabel("Font size:"), 1, 0);
    font_size_spin_ = new QSpinBox();
    font_size_spin_->setRange(8, 24);
    font_size_spin_->setValue(10);
    font_layout->addWidget(font_size_spin_, 1, 1);
    editor_layout->addWidget(font_group);
    
    QGroupBox* editor_options_group = new QGroupBox("Editor Options");
    QVBoxLayout* editor_options_layout = new QVBoxLayout(editor_options_group);
    tab_size_spin_ = new QSpinBox();
    tab_size_spin_->setRange(2, 8);
    tab_size_spin_->setValue(4);
    editor_options_layout->addWidget(new QLabel("Tab size:"));
    editor_options_layout->addWidget(tab_size_spin_);
    word_wrap_checkbox_ = new QCheckBox("Enable word wrap");
    auto_indent_checkbox_ = new QCheckBox("Enable auto-indent");
    line_numbers_checkbox_ = new QCheckBox("Show line numbers");
    editor_options_layout->addWidget(word_wrap_checkbox_);
    editor_options_layout->addWidget(auto_indent_checkbox_);
    editor_options_layout->addWidget(line_numbers_checkbox_);
    editor_layout->addWidget(editor_options_group);
    
    editor_layout->addStretch();
    tab_widget->addTab(editor_tab, "Editor");
    
    // Logging tab
    QWidget* logging_tab = new QWidget();
    QVBoxLayout* logging_layout = new QVBoxLayout(logging_tab);
    
    QGroupBox* log_settings_group = new QGroupBox("Log Settings");
    QVBoxLayout* log_settings_layout = new QVBoxLayout(log_settings_group);
    log_settings_layout->addWidget(new QLabel("Log level:"));
    log_level_combo_ = new QComboBox();
    log_level_combo_->addItem("Debug", "DEBUG");
    log_level_combo_->addItem("Info", "INFO");
    log_level_combo_->addItem("Warning", "WARNING");
    log_level_combo_->addItem("Error", "ERROR");
    log_level_combo_->addItem("Critical", "CRITICAL");
    log_level_combo_->setCurrentIndex(1);  // Info
    log_settings_layout->addWidget(log_level_combo_);
    log_to_file_checkbox_ = new QCheckBox("Log to file");
    log_to_file_checkbox_->setChecked(true);
    log_settings_layout->addWidget(log_to_file_checkbox_);
    logging_layout->addWidget(log_settings_group);
    
    QGroupBox* log_folder_group = new QGroupBox("Log Folder");
    QHBoxLayout* log_folder_layout = new QHBoxLayout(log_folder_group);
    log_folder_edit_ = new QLineEdit();
    log_folder_edit_->setReadOnly(true);
    log_folder_layout->addWidget(log_folder_edit_);
    QPushButton* browse_log_btn = new QPushButton("Browse...");
    connect(browse_log_btn, &QPushButton::clicked, this, &PreferencesDialog::browse_log_folder);
    log_folder_layout->addWidget(browse_log_btn);
    logging_layout->addWidget(log_folder_group);
    
    QGroupBox* retention_group = new QGroupBox("Log Retention");
    QVBoxLayout* retention_layout = new QVBoxLayout(retention_group);
    retention_layout->addWidget(new QLabel("Keep logs for (days):"));
    log_retention_spin_ = new QSpinBox();
    log_retention_spin_->setRange(1, 365);
    log_retention_spin_->setValue(30);
    retention_layout->addWidget(log_retention_spin_);
    logging_layout->addWidget(retention_group);
    
    logging_layout->addStretch();
    tab_widget->addTab(logging_tab, "Logging");
    
    // UI tab
    QWidget* ui_tab = new QWidget();
    QVBoxLayout* ui_layout = new QVBoxLayout(ui_tab);
    
    QGroupBox* appearance_group = new QGroupBox("Appearance");
    QVBoxLayout* appearance_layout = new QVBoxLayout(appearance_group);
    appearance_layout->addWidget(new QLabel("Application theme:"));
    app_theme_combo_ = new QComboBox();
    app_theme_combo_->addItems({"Default Light", "Dark Charcoal", "Dracula", "Nord", "Solarized Light", "Oceanic"});
    appearance_layout->addWidget(app_theme_combo_);
    ui_layout->addWidget(appearance_group);
    
    QGroupBox* interface_group = new QGroupBox("Interface");
    QVBoxLayout* interface_layout = new QVBoxLayout(interface_group);
    show_toolbar_checkbox_ = new QCheckBox("Show toolbar");
    show_toolbar_checkbox_->setChecked(true);
    show_statusbar_checkbox_ = new QCheckBox("Show status bar");
    show_statusbar_checkbox_->setChecked(true);
    interface_layout->addWidget(show_toolbar_checkbox_);
    interface_layout->addWidget(show_statusbar_checkbox_);
    ui_layout->addWidget(interface_group);
    
    QGroupBox* refresh_group = new QGroupBox("Refresh");
    QVBoxLayout* refresh_layout = new QVBoxLayout(refresh_group);
    refresh_layout->addWidget(new QLabel("Panel status refresh interval (seconds):"));
    refresh_interval_spin_ = new QSpinBox();
    refresh_interval_spin_->setRange(1, 60);
    refresh_interval_spin_->setValue(5);
    refresh_layout->addWidget(refresh_interval_spin_);
    ui_layout->addWidget(refresh_group);
    
    ui_layout->addStretch();
    tab_widget->addTab(ui_tab, "UI");
    
    main_layout->addWidget(tab_widget);
    
    // Buttons
    QHBoxLayout* button_layout = new QHBoxLayout();
    button_layout->addStretch();
    
    reset_button_ = new QPushButton("Reset to Defaults");
    connect(reset_button_, &QPushButton::clicked, this, &PreferencesDialog::reset_defaults);
    button_layout->addWidget(reset_button_);
    
    cancel_button_ = new QPushButton("Cancel");
    connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);
    button_layout->addWidget(cancel_button_);
    
    save_button_ = new QPushButton("Save");
    save_button_->setDefault(true);
    connect(save_button_, &QPushButton::clicked, this, &PreferencesDialog::save_preferences);
    button_layout->addWidget(save_button_);
    
    main_layout->addLayout(button_layout);
}

void PreferencesDialog::load_preferences() {
    QSettings settings;
    
    // General
    auto_start_checkbox_->setChecked(settings.value("auto_start", false).toBool());
    minimize_to_tray_checkbox_->setChecked(settings.value("minimize_to_tray", true).toBool());
    show_notifications_checkbox_->setChecked(settings.value("show_notifications", true).toBool());
    
    QString display_server = QString::fromStdString(ConfigManager::instance().get_display_server());
    int display_index = display_server_combo_->findData(display_server);
    if (display_index >= 0) {
        display_server_combo_->setCurrentIndex(display_index);
    }
    
    // Editor
    font_family_combo_->setCurrentFont(QFont(settings.value("editor_font_family", "Monospace").toString()));
    font_size_spin_->setValue(settings.value("editor_font_size", 10).toInt());
    tab_size_spin_->setValue(settings.value("editor_tab_size", 4).toInt());
    word_wrap_checkbox_->setChecked(settings.value("editor_word_wrap", false).toBool());
    auto_indent_checkbox_->setChecked(settings.value("editor_auto_indent", true).toBool());
    line_numbers_checkbox_->setChecked(settings.value("editor_line_numbers", true).toBool());
    
    // Logging
    QString log_level = settings.value("log_level", "INFO").toString();
    int log_index = log_level_combo_->findData(log_level);
    if (log_index >= 0) {
        log_level_combo_->setCurrentIndex(log_index);
    }
    log_to_file_checkbox_->setChecked(settings.value("log_to_file", true).toBool());
    log_folder_edit_->setText(settings.value("log_folder", "").toString());
    log_retention_spin_->setValue(settings.value("log_retention_days", 30).toInt());
    
    // UI
    app_theme_combo_->setCurrentText(settings.value("app_theme", "Default Light").toString());
    show_toolbar_checkbox_->setChecked(settings.value("show_toolbar", true).toBool());
    show_statusbar_checkbox_->setChecked(settings.value("show_statusbar", true).toBool());
    refresh_interval_spin_->setValue(settings.value("refresh_interval", 5).toInt());
}

void PreferencesDialog::save_preferences() {
    QSettings settings;
    
    // General
    settings.setValue("auto_start", auto_start_checkbox_->isChecked());
    settings.setValue("minimize_to_tray", minimize_to_tray_checkbox_->isChecked());
    settings.setValue("show_notifications", show_notifications_checkbox_->isChecked());
    
    // Save display server to config
    ConfigManager::instance().set_display_server(display_server_combo_->currentData().toString().toStdString());
    
    // Editor
    settings.setValue("editor_font_family", font_family_combo_->currentFont().family());
    settings.setValue("editor_font_size", font_size_spin_->value());
    settings.setValue("editor_tab_size", tab_size_spin_->value());
    settings.setValue("editor_word_wrap", word_wrap_checkbox_->isChecked());
    settings.setValue("editor_auto_indent", auto_indent_checkbox_->isChecked());
    settings.setValue("editor_line_numbers", line_numbers_checkbox_->isChecked());
    
    // Logging
    settings.setValue("log_level", log_level_combo_->currentData().toString());
    settings.setValue("log_to_file", log_to_file_checkbox_->isChecked());
    settings.setValue("log_folder", log_folder_edit_->text());
    settings.setValue("log_retention_days", log_retention_spin_->value());
    
    // UI
    settings.setValue("app_theme", app_theme_combo_->currentText());
    settings.setValue("show_toolbar", show_toolbar_checkbox_->isChecked());
    settings.setValue("show_statusbar", show_statusbar_checkbox_->isChecked());
    settings.setValue("refresh_interval", refresh_interval_spin_->value());
    
    // Save config
    ConfigManager::instance().save_config();
    
    LOG_INFO("Preferences saved");
    QMessageBox::information(this, "Preferences", "Preferences saved successfully. Some changes may require a restart.");
    
    accept();
}

void PreferencesDialog::reset_defaults() {
    if (QMessageBox::question(this, "Reset Preferences", 
        "Are you sure you want to reset all preferences to defaults?") == QMessageBox::Yes) {
        
        QSettings settings;
        settings.clear();
        
        load_preferences();
        LOG_INFO("Preferences reset to defaults");
    }
}

void PreferencesDialog::browse_log_folder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Log Folder", log_folder_edit_->text());
    if (!dir.isEmpty()) {
        log_folder_edit_->setText(dir);
    }
}

void PreferencesDialog::apply_preferences() {
    // Apply preferences immediately
    // This would be called when preferences change
}