#pragma once

#include <QDialog>
#include <QString>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>

class FirstRunSetup : public QDialog {
    Q_OBJECT

public:
    explicit FirstRunSetup(QWidget* parent = nullptr);
    
    QString getConkyConfigPath() const;
    QString getThemesPath() const;
    bool shouldCreateSampleConfig() const;
    QString getDisplayServer() const;
    
    static bool isFirstRun();
    static void markSetupComplete();

    // Creates the folder layout under the configured Conky folder and, when
    // that folder holds no panels yet, writes one minimal sample panel plus a
    // starter theme so a brand-new install has something to control.
    // Never overwrites an existing file. `messageOut` receives a human
    // description of what happened (also written to the log).
    static bool createSampleConfig(const QString& conkyRoot, QString* messageOut = nullptr);

private slots:
    void browseConkyConfig();
    void browseThemes();
    void accept() override;
    void reject() override;

private:
    void setupUI();
    void loadDefaults();
    bool validatePaths();
    
    QLineEdit* conkyConfigEdit_;
    QLineEdit* themesEdit_;
    QCheckBox* createSampleCheckbox_;
    QComboBox* displayServerCombo_;
    QLabel* statusLabel_;
    
    QString conkyConfigPath_;
    QString themesPath_;
    QString displayServer_;
};