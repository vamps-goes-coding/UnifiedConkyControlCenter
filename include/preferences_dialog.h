#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QFontComboBox>

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    
    // Show preferences dialog
    static void show_preferences(QWidget* parent);

private slots:
    void save_preferences();
    void reset_defaults();
    void browse_log_folder();

private:
    void setup_ui();
    void load_preferences();
    void apply_preferences();
    
    // General tab
    QCheckBox* auto_start_checkbox_;
    QCheckBox* minimize_to_tray_checkbox_;
    QCheckBox* show_notifications_checkbox_;
    QComboBox* display_server_combo_;
    
    // Editor tab
    QFontComboBox* font_family_combo_;
    QSpinBox* font_size_spin_;
    QSpinBox* tab_size_spin_;
    QCheckBox* word_wrap_checkbox_;
    QCheckBox* auto_indent_checkbox_;
    QCheckBox* line_numbers_checkbox_;
    
    // Logging tab
    QComboBox* log_level_combo_;
    QCheckBox* log_to_file_checkbox_;
    QLineEdit* log_folder_edit_;
    QSpinBox* log_retention_spin_;
    
    // UI tab
    QComboBox* app_theme_combo_;
    QCheckBox* show_toolbar_checkbox_;
    QCheckBox* show_statusbar_checkbox_;
    QSpinBox* refresh_interval_spin_;
    
    QPushButton* save_button_;
    QPushButton* cancel_button_;
    QPushButton* reset_button_;
};