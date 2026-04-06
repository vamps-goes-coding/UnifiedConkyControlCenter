#pragma once

#include <QWizard>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QPushButton>
#include "system_detector.h"

class SetupWizard : public QWizard {
    Q_OBJECT
public:
    explicit SetupWizard(QWidget* parent = nullptr);
};

class IntroPage : public QWizardPage { public: IntroPage(QWidget* parent = nullptr); };
class DistroPage : public QWizardPage { public: DistroPage(QWidget* parent = nullptr); private: QRadioButton *uBtn, *fBtn, *aBtn; };

class InstallPage : public QWizardPage { 
    Q_OBJECT
public: 
    InstallPage(QWidget* parent = nullptr); 
    void initializePage() override; 
    bool isComplete() const override;

private slots:
    void startInstallation();

private: 
    QLabel* cmdLabel; 
    QPushButton* installBtn;
    bool is_finished = false;
};

class FinishPage : public QWizardPage { public: FinishPage(QWidget* parent = nullptr); };