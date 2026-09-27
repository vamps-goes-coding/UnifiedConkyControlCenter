#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QTableWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QPlainTextEdit>

/**
 * @brief Dialog for managing application-wide preferences including paths,
 * default panels, editor configurations, panel discovery, refresh intervals,
 * display server, and window settings.
 */
class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

private slots:
    void saveAndAccept();
    void loadCurrentConfig();
    void loadServerOverrides();
    void updateResolvedPaths();

private:
    void setupUI();

    // Path settings
    QLineEdit* conkyPathEdit;
    QLineEdit* themesPathEdit;

    // Panel settings
    QListWidget* panelsList;
    QListWidget* availablePanelsList;

    // Editor settings
    QTableWidget* editorsTable;

    // Panel Discovery settings
    QLineEdit* configPrefixEdit;
    QLineEdit* configExtensionEdit;
    QPlainTextEdit* excludedFilesEdit;

    // Refresh & Window settings
    QSpinBox* heartbeatSpin;
    QSpinBox* panelStatusSpin;
    QSpinBox* minWidthSpin;
    QSpinBox* minHeightSpin;
    QSpinBox* defaultWidthSpin;
    QSpinBox* defaultHeightSpin;

    // Display Server settings
    QComboBox* displayServerCombo;
    QComboBox* serverKeyCombo;

    // Per-display-server overrides
    QLineEdit* configSubdirEdit;
    QLineEdit* themesSubdirEdit;
    QLineEdit* configPrefixEdit2;
    QLineEdit* configExtensionEdit2;

    // Live resolved paths
    QLineEdit* resolvedConkyPathEdit;
    QLineEdit* resolvedThemesPathEdit;

    // Theme settings
    QLineEdit* themeExtensionEdit;
    QLineEdit* currentThemeFileEdit;

    // General settings
};

#endif // PREFERENCES_DIALOG_H