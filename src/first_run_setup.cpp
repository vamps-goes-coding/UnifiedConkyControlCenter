#include "first_run_setup.h"
#include "config_manager.h"
#include "utils.h"
#include "display_server.h"
#include "logger.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

FirstRunSetup::FirstRunSetup(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    loadDefaults();
}

void FirstRunSetup::setupUI() {
    setWindowTitle("First Run Setup - Unified Conky Control Center");
    setMinimumWidth(600);
    setMinimumHeight(400);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Welcome message
    QLabel* welcomeLabel = new QLabel(
        "<h2>Welcome to Unified Conky Control Center!</h2>"
        "<p>This appears to be your first time running the application. "
        "Please configure the paths to your Conky configuration and themes folders.</p>"
    );
    welcomeLabel->setWordWrap(true);
    mainLayout->addWidget(welcomeLabel);
    
    // Conky Config Path
    QGroupBox* configGroup = new QGroupBox("Conky Configuration Folder");
    QVBoxLayout* configLayout = new QVBoxLayout(configGroup);
    
    QLabel* configDesc = new QLabel(
        "Select the folder containing your Conky panel configuration files "
        "(.conf files). This is typically where your conky-wayland or conky-x11 folders are located."
    );
    configDesc->setWordWrap(true);
    configLayout->addWidget(configDesc);
    
    QHBoxLayout* configPathLayout = new QHBoxLayout();
    conkyConfigEdit_ = new QLineEdit();
    conkyConfigEdit_->setPlaceholderText("Select Conky configuration folder...");
    configPathLayout->addWidget(conkyConfigEdit_);
    
    QPushButton* browseConfigBtn = new QPushButton("Browse...");
    connect(browseConfigBtn, &QPushButton::clicked, this, &FirstRunSetup::browseConkyConfig);
    configPathLayout->addWidget(browseConfigBtn);
    
    configLayout->addLayout(configPathLayout);
    mainLayout->addWidget(configGroup);
    
    // Themes Path
    QGroupBox* themesGroup = new QGroupBox("Themes Folder");
    QVBoxLayout* themesLayout = new QVBoxLayout(themesGroup);
    
    QLabel* themesDesc = new QLabel(
        "Select the folder containing your Conky themes (.lua files). "
        "This is typically a 'themes' subfolder within your Conky configuration directory."
    );
    themesDesc->setWordWrap(true);
    themesLayout->addWidget(themesDesc);
    
    QHBoxLayout* themesPathLayout = new QHBoxLayout();
    themesEdit_ = new QLineEdit();
    themesEdit_->setPlaceholderText("Select themes folder...");
    themesPathLayout->addWidget(themesEdit_);
    
    QPushButton* browseThemesBtn = new QPushButton("Browse...");
    connect(browseThemesBtn, &QPushButton::clicked, this, &FirstRunSetup::browseThemes);
    themesPathLayout->addWidget(browseThemesBtn);
    
    themesLayout->addLayout(themesPathLayout);
    mainLayout->addWidget(themesGroup);
    
    // Display server selection
    QGroupBox* displayGroup = new QGroupBox("Display Server");
    QVBoxLayout* displayLayout = new QVBoxLayout(displayGroup);
    
    QLabel* displayDesc = new QLabel(
        "Select your display server or let the application auto-detect it. "
        "This helps the application find the correct Conky configuration files."
    );
    displayDesc->setWordWrap(true);
    displayLayout->addWidget(displayDesc);
    
    displayServerCombo_ = new QComboBox();
    displayServerCombo_->addItem("Auto-detect (Recommended)", "auto");
    displayServerCombo_->addItem("X11", "x11");
    displayServerCombo_->addItem("Wayland", "wayland");
    displayServerCombo_->setCurrentIndex(0);  // Default to auto-detect
    displayLayout->addWidget(displayServerCombo_);
    
    mainLayout->addWidget(displayGroup);
    
    // Sample config option
    createSampleCheckbox_ = new QCheckBox("Create sample configuration files if they don't exist");
    createSampleCheckbox_->setChecked(true);
    mainLayout->addWidget(createSampleCheckbox_);
    
    // Status label
    statusLabel_ = new QLabel();
    statusLabel_->setStyleSheet("color: #666; font-style: italic;");
    mainLayout->addWidget(statusLabel_);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    QPushButton* cancelButton = new QPushButton("Cancel");
    connect(cancelButton, &QPushButton::clicked, this, &FirstRunSetup::reject);
    buttonLayout->addWidget(cancelButton);
    
    QPushButton* okButton = new QPushButton("Continue");
    okButton->setDefault(true);
    connect(okButton, &QPushButton::clicked, this, &FirstRunSetup::accept);
    buttonLayout->addWidget(okButton);
    
    mainLayout->addLayout(buttonLayout);
}

void FirstRunSetup::loadDefaults() {
    // Try to find default Conky config path
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    
    // Detect display server and suggest appropriate path
    DisplayServerType detected = DisplayServer::get_type();
    QString suggestedPath;
    
    if (detected == DisplayServerType::Wayland) {
        suggestedPath = homePath + "/conky-confs/conky-wayland";
        displayServerCombo_->setCurrentIndex(2);  // Wayland
    } else if (detected == DisplayServerType::X11) {
        suggestedPath = homePath + "/conky-confs/conky-x11";
        displayServerCombo_->setCurrentIndex(1);  // X11
    } else {
        suggestedPath = homePath + "/conky-confs/conky-wayland";
        displayServerCombo_->setCurrentIndex(0);  // Auto-detect
    }
    
    // Common locations for Conky configs
    QStringList possibleConfigPaths = {
        homePath + "/.config/conky",
        suggestedPath,
        homePath + "/conky-confs/conky-wayland",
        homePath + "/conky-confs/conky-x11",
        configPath + "/conky",
        homePath + "/conky",
        homePath + "/.conky"
    };
    
    for (const QString& path : possibleConfigPaths) {
        if (QDir(path).exists()) {
            conkyConfigEdit_->setText(path);
            conkyConfigPath_ = path;
            break;
        }
    }
    
    // If no default found, use suggested path
    if (conkyConfigPath_.isEmpty()) {
        conkyConfigPath_ = suggestedPath;
        conkyConfigEdit_->setText(suggestedPath);
    }
    
    // Set themes path as subfolder of config path
    QString defaultThemesPath = conkyConfigPath_ + "/themes";
    if (QDir(defaultThemesPath).exists()) {
        themesEdit_->setText(defaultThemesPath);
        themesPath_ = defaultThemesPath;
    } else {
        themesEdit_->setText(conkyConfigPath_);
        themesPath_ = conkyConfigPath_;
    }
    
    // Update status with detection info
    QString displayServerName = DisplayServer::get_type_string().c_str();
    statusLabel_->setText(QString("Detected display server: %1. Please verify the paths above and click Continue.").arg(displayServerName));
}

void FirstRunSetup::browseConkyConfig() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Conky Configuration Folder",
        conkyConfigEdit_->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty()) {
        conkyConfigEdit_->setText(dir);
        conkyConfigPath_ = dir;
        
        // Auto-update themes path if it's a subfolder
        QString themesPath = dir + "/themes";
        if (QDir(themesPath).exists()) {
            themesEdit_->setText(themesPath);
            themesPath_ = themesPath;
        }
    }
}

void FirstRunSetup::browseThemes() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Themes Folder",
        themesEdit_->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty()) {
        themesEdit_->setText(dir);
        themesPath_ = dir;
    }
}

QString FirstRunSetup::getConkyConfigPath() const {
    return conkyConfigPath_;
}

QString FirstRunSetup::getThemesPath() const {
    return themesPath_;
}

bool FirstRunSetup::shouldCreateSampleConfig() const {
    return createSampleCheckbox_->isChecked();
}

QString FirstRunSetup::getDisplayServer() const {
    return displayServerCombo_->currentData().toString();
}

bool FirstRunSetup::validatePaths() {
    if (conkyConfigPath_.isEmpty()) {
        QMessageBox::warning(this, "Invalid Path", "Please select a Conky configuration folder.");
        return false;
    }
    
    if (themesPath_.isEmpty()) {
        QMessageBox::warning(this, "Invalid Path", "Please select a themes folder.");
        return false;
    }
    
    return true;
}

void FirstRunSetup::accept() {
    if (validatePaths()) {
        // Ensure directories exist before closing the dialog
        try {
            fs::create_directories(conkyConfigPath_.toStdString());
            fs::create_directories(themesPath_.toStdString());
            
            if (shouldCreateSampleConfig()) {
                LOG_INFO("Creating initial directory structure at " + conkyConfigPath_.toStdString());
            }
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "IO Error", 
                "Could not create directories: " + QString::fromStdString(e.what()));
            return;
        }

        // Save display server selection
        displayServer_ = displayServerCombo_->currentData().toString();
        LOG_INFO("Display server selected: " + displayServer_.toStdString());
        QDialog::accept();
    }
}

void FirstRunSetup::reject() {
    QDialog::reject();
}

bool FirstRunSetup::isFirstRun() {
    QSettings settings;
    return !settings.value("setup_complete", false).toBool();
}

void FirstRunSetup::markSetupComplete() {
    QSettings settings;
    settings.setValue("setup_complete", true);
}