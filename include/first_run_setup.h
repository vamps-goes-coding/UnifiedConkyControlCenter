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