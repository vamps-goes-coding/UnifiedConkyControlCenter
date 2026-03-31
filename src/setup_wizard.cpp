#include "setup_wizard.h"
#include "progress_dialog.h"
#include <QButtonGroup>
#include <QTimer>

SetupWizard::SetupWizard(QWidget* parent) : QWizard(parent) {
    addPage(new IntroPage);
    addPage(new DistroPage);
    addPage(new InstallPage);
    addPage(new FinishPage);
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

InstallPage::InstallPage(QWidget* parent) : QWizardPage(parent) {
    setTitle("Install Dependencies");
    auto* layout = new QVBoxLayout(this);
    
    auto* info = new QLabel("To complete the installation, you should run the following command in your terminal:");
    info->setWordWrap(true);
    layout->addWidget(info);

    cmdLabel = new QLabel();
    cmdLabel->setStyleSheet("background: #222; color: #0f0; padding: 10px; font-family: monospace;");
    cmdLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(cmdLabel);

    installBtn = new QPushButton("Verify Dependencies & Finalize");
    connect(installBtn, &QPushButton::clicked, this, &InstallPage::startInstallation);
    layout->addWidget(installBtn);
}

void InstallPage::initializePage() {
    QString cmd;
    if (field("distro.fedora").toBool()) {
        cmd = "sudo dnf install gcc-c++ cmake qt6-qtbase-devel nlohmann-json-devel conky";
    } else if (field("distro.arch").toBool()) {
        cmd = "sudo pacman -S base-devel cmake qt6-base nlohmann-json conky";
    } else {
        cmd = "sudo apt install build-essential cmake qt6-base-dev nlohmann-json3-dev conky";
    }

    cmdLabel->setText(cmd);
    is_finished = false;
    installBtn->setEnabled(true);
}

bool InstallPage::isComplete() const {
    return is_finished;
}

void InstallPage::startInstallation() {
    installBtn->setEnabled(false);
    ProgressDialog::show_progress(this, "Finalizing Setup", "Applying system configurations...");
    
    QTimer* timer = new QTimer(this);
    auto progress = std::make_shared<int>(0);
    
    connect(timer, &QTimer::timeout, [this, timer, progress]() {
        *progress += 10;
        ProgressDialog::update_progress(*progress, QString("Processing... %1%").arg(*progress));
        
        if (*progress >= 100) {
            timer->stop();
            timer->deleteLater();
            ProgressDialog::close_progress();
            is_finished = true;
            emit completeChanged();
        }
    });
    timer->start(150);
}

FinishPage::FinishPage(QWidget* parent) : QWizardPage(parent) {
    setTitle("Installation Finished");
    auto* layout = new QVBoxLayout(this);
    
    auto* successIcon = new QLabel("✅");
    successIcon->setAlignment(Qt::AlignCenter);
    successIcon->setStyleSheet("font-size: 48px; margin-bottom: 10px;");
    layout->addWidget(successIcon);

    auto* mainText = new QLabel("<b>Unified Conky Control Center is ready to use!</b>");
    mainText->setAlignment(Qt::AlignCenter);
    layout->addWidget(mainText);

    auto* instructions = new QLabel(
        "\nInstructions:\n"
        "• Launch the app via your menu or by running: <code>UnifiedConkyControlCenter</code>\n"
        "• Select your configuration folder when the application starts.\n"
        "• You can manage all panels and themes from the system tray icon."
    );
    instructions->setWordWrap(true);
    instructions->setStyleSheet("background: #f9f9f9; border: 1px solid #ddd; padding: 15px; border-radius: 5px;");
    layout->addWidget(instructions);

    layout->addStretch();
    setButtonText(QWizard::FinishButton, "OK");
}