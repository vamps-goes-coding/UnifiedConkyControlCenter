#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QTableWidget>

/**
 * @brief Dialog for managing application-wide preferences including paths,
 * default panels, and editor configurations.
 */
class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

private slots:
    void saveAndAccept();
    void loadCurrentConfig();

private:
    void setupUI();

    // Path settings
    QLineEdit* conkyPathEdit;
    QLineEdit* themesPathEdit;

    // Panel settings
    QListWidget* panelsList;

    // Editor settings
    QTableWidget* editorsTable;

    // General settings
    QLineEdit* appNameEdit;
};

#endif // PREFERENCES_DIALOG_H