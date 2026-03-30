#include "setup_wizard.h"
#include <QButtonGroup>

SetupWizard::SetupWizard(QWidget* parent) : QWizard(parent) {
    addPage(new IntroPage);
    addPage(new DistroPage);
    addPage(new InstallPage);
    setWindowTitle("System Setup Wizard");
}

IntroPage::IntroPage(QWidget* parent) : QWizardPage(parent) {
    setTitle("Welcome");
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("This wizard will help you prepare your system for the Unified Conky Control Center."));
}

DistroPage::DistroPage(QWidget* parent) : QWizardPage(parent) {
    setTitle("Select Distribution");
    auto* layout = new QVBoxLayout(this);
    
    DistroInfo detected = SystemDetector::detect();
    layout->addWidget(new QLabel(QString("Auto-detected: <b>%1</b>").arg(detected.name)));
    layout->addWidget(new QLabel("If this is incorrect, please select your OS manually:"));

    uBtn = new QRadioButton("Ubuntu / Debian / Mint");
    fBtn = new QRadioButton("Fedora / Red Hat");
    aBtn = new QRadioButton("Arch Linux / Manjaro");

    if (detected.distro == LinuxDistro::Fedora) fBtn->setChecked(true);
    else if (detected.distro == LinuxDistro::Arch) aBtn->setChecked(true);
    else uBtn->setChecked(true);

    layout->addWidget(uBtn);
    layout->addWidget(fBtn);
    layout->addWidget(aBtn);

    registerField("distro.ubuntu", uBtn);
    registerField("distro.fedora", fBtn);
    registerField("distro.arch", aBtn);
}

void InstallPage::initializePage() {
    setTitle("Install Dependencies");
    auto* layout = new QVBoxLayout(this);
    
    QString cmd;
    if (field("distro.fedora").toBool()) {
        cmd = "sudo dnf install gcc-c++ cmake qt6-qtbase-devel nlohmann-json-devel conky";
    } else if (field("distro.arch").toBool()) {
        cmd = "sudo pacman -S base-devel cmake qt6-base nlohmann-json conky";
    } else {
        cmd = "sudo apt install build-essential cmake qt6-base-dev nlohmann-json3-dev conky";
    }

    auto* info = new QLabel("To complete the installation, run the following command in your terminal:");
    info->setWordWrap(true);
    layout->addWidget(info);

    cmdLabel = new QLabel(cmd);
    cmdLabel->setStyleSheet("background: #222; color: #0f0; padding: 10px; font-family: monospace;");
    cmdLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(cmdLabel);
}

InstallPage::InstallPage(QWidget* parent) : QWizardPage(parent) {}