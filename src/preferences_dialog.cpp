#include "preferences_dialog.h"
#include "config_manager.h"
#include "ui_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QSpinBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <vector>
#include <string>

PreferencesDialog::PreferencesDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Preferences");
    setMinimumSize(700, 550);
    setupUI();
    loadCurrentConfig();
}

void PreferencesDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget();

    // --- Paths Tab ---
    auto* pathsTab = new QWidget();
    auto* pathsLayout = new QVBoxLayout(pathsTab);
    
    conkyPathEdit = new QLineEdit();
    auto* conkyBrowse = new QPushButton("Browse...");
    auto* cpLayout = new QHBoxLayout();
    cpLayout->addWidget(conkyPathEdit);
    cpLayout->addWidget(conkyBrowse);
    pathsLayout->addWidget(new QLabel("Conky Configurations Directory:"));
    pathsLayout->addLayout(cpLayout);

    themesPathEdit = new QLineEdit();
    auto* themesBrowse = new QPushButton("Browse...");
    auto* tpLayout = new QHBoxLayout();
    tpLayout->addWidget(themesPathEdit);
    tpLayout->addWidget(themesBrowse);
    pathsLayout->addWidget(new QLabel("Themes (.lua) Directory:"));
    pathsLayout->addLayout(tpLayout);
    pathsLayout->addStretch();

    connect(conkyBrowse, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Conky Dir", conkyPathEdit->text());
        if (!dir.isEmpty()) conkyPathEdit->setText(dir);
    });
    connect(themesBrowse, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Themes Dir", themesPathEdit->text());
        if (!dir.isEmpty()) themesPathEdit->setText(dir);
    });

    // --- "Start Mains" Panels Tab ---
    auto* panelsTab = new QWidget();
    auto* panelsLayout = new QVBoxLayout(panelsTab);
    panelsLayout->addWidget(new QLabel("Panels started by 'Start Mains' button:"));
    
    panelsList = new QListWidget();
    panelsLayout->addWidget(panelsList);
    
    auto* pButtons = new QHBoxLayout();
    auto* addPanel = new QPushButton("Add Panel");
    auto* remPanel = new QPushButton("Remove Selected");
    pButtons->addWidget(addPanel);
    pButtons->addWidget(remPanel);
    panelsLayout->addLayout(pButtons);

    connect(addPanel, &QPushButton::clicked, [this]() {
        QString name = QString::fromStdString(UIManager::show_input_dialog("Add Panel", "Enter panel config name (without .conf):"));
        if (!name.isEmpty()) panelsList->addItem(name);
    });
    connect(remPanel, &QPushButton::clicked, [this]() {
        delete panelsList->currentItem();
    });

    // --- Editors Tab ---
    auto* editorsTab = new QWidget();
    auto* editorsLayout = new QVBoxLayout(editorsTab);
    editorsTable = new QTableWidget(0, 3);
    editorsTable->setHorizontalHeaderLabels({"Name", "Command", "Icon"});
    editorsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    editorsLayout->addWidget(editorsTable);

    auto* eButtons = new QHBoxLayout();
    auto* addEditor = new QPushButton("Add Editor");
    auto* remEditor = new QPushButton("Remove Selected");
    eButtons->addWidget(addEditor);
    eButtons->addWidget(remEditor);
    editorsLayout->addLayout(eButtons);

    connect(addEditor, &QPushButton::clicked, [this]() {
        int row = editorsTable->rowCount();
        editorsTable->insertRow(row);
        editorsTable->setItem(row, 0, new QTableWidgetItem("New Editor"));
        editorsTable->setItem(row, 1, new QTableWidgetItem("command"));
        editorsTable->setItem(row, 2, new QTableWidgetItem("text"));
    });
    connect(remEditor, &QPushButton::clicked, [this]() {
        editorsTable->removeRow(editorsTable->currentRow());
    });

    // --- Panel Discovery Tab ---
    auto* discoveryTab = new QWidget();
    auto* discoveryLayout = new QVBoxLayout(discoveryTab);
    
    auto* prefixGroup = new QGroupBox("Config File Naming");
    auto* prefixLayout = new QVBoxLayout(prefixGroup);
    configPrefixEdit = new QLineEdit();
    configPrefixEdit->setPlaceholderText("e.g., conky-wayland-");
    prefixLayout->addWidget(new QLabel("Config File Prefix:"));
    prefixLayout->addWidget(configPrefixEdit);
    
    configExtensionEdit = new QLineEdit();
    configExtensionEdit->setPlaceholderText("e.g., .conf");
    prefixLayout->addWidget(new QLabel("Config File Extension:"));
    prefixLayout->addWidget(configExtensionEdit);
    
    excludedFilesEdit = new QPlainTextEdit();
    excludedFilesEdit->setPlaceholderText("Enter filenames to exclude, one per line\ne.g., conky-wayland-exclude.conf");
    prefixLayout->addWidget(new QLabel("Excluded Files (one per line):"));
    prefixLayout->addWidget(excludedFilesEdit);
    
    discoveryLayout->addWidget(prefixGroup);
    discoveryLayout->addStretch();

    // --- Refresh & Window Tab ---
    auto* refreshTab = new QWidget();
    auto* refreshLayout = new QVBoxLayout(refreshTab);
    
    auto* refreshGroup = new QGroupBox("Refresh Intervals");
    auto* refreshGroupLayout = new QVBoxLayout(refreshGroup);
    auto* heartbeatLayout = new QHBoxLayout();
    heartbeatLayout->addWidget(new QLabel("Heartbeat Interval:"));
    heartbeatSpin = new QSpinBox();
    heartbeatSpin->setRange(1, 3600);
    heartbeatLayout->addWidget(heartbeatSpin);
    heartbeatLayout->addWidget(new QLabel("seconds"));
    heartbeatLayout->addStretch();
    refreshGroupLayout->addLayout(heartbeatLayout);
    
    auto* panelStatusLayout = new QHBoxLayout();
    panelStatusLayout->addWidget(new QLabel("Panel Status Check:"));
    panelStatusSpin = new QSpinBox();
    panelStatusSpin->setRange(1, 3600);
    panelStatusLayout->addWidget(panelStatusSpin);
    panelStatusLayout->addWidget(new QLabel("seconds"));
    panelStatusLayout->addStretch();
    refreshGroupLayout->addLayout(panelStatusLayout);
    refreshLayout->addWidget(refreshGroup);
    
    auto* windowGroup = new QGroupBox("Window Dimensions");
    auto* windowGroupLayout = new QVBoxLayout(windowGroup);
    
    auto* minSizeLayout = new QHBoxLayout();
    minSizeLayout->addWidget(new QLabel("Minimum Width:"));
    minWidthSpin = new QSpinBox();
    minWidthSpin->setRange(400, 3840);
    minSizeLayout->addWidget(minWidthSpin);
    minSizeLayout->addWidget(new QLabel("Height:"));
    minHeightSpin = new QSpinBox();
    minHeightSpin->setRange(300, 2160);
    minSizeLayout->addWidget(minHeightSpin);
    minSizeLayout->addWidget(new QLabel("px"));
    minSizeLayout->addStretch();
    windowGroupLayout->addLayout(minSizeLayout);
    
    auto* defaultSizeLayout = new QHBoxLayout();
    defaultSizeLayout->addWidget(new QLabel("Default Width:"));
    defaultWidthSpin = new QSpinBox();
    defaultWidthSpin->setRange(400, 3840);
    defaultSizeLayout->addWidget(defaultWidthSpin);
    defaultSizeLayout->addWidget(new QLabel("Height:"));
    defaultHeightSpin = new QSpinBox();
    defaultHeightSpin->setRange(300, 2160);
    defaultSizeLayout->addWidget(defaultHeightSpin);
    defaultSizeLayout->addWidget(new QLabel("px"));
    defaultSizeLayout->addStretch();
    windowGroupLayout->addLayout(defaultSizeLayout);
    refreshLayout->addWidget(windowGroup);
    refreshLayout->addStretch();

    // --- Display Server Tab ---
    auto* displayTab = new QWidget();
    auto* displayLayout = new QVBoxLayout(displayTab);
    
    auto* serverGroup = new QGroupBox("Display Server Mode");
    auto* serverGroupLayout = new QVBoxLayout(serverGroup);
    serverGroupLayout->addWidget(new QLabel("Select the display server to use:"));
    displayServerCombo = new QComboBox();
    displayServerCombo->addItem("Auto-detect", "auto");
    displayServerCombo->addItem("X11", "x11");
    displayServerCombo->addItem("Wayland", "wayland");
    serverGroupLayout->addWidget(displayServerCombo);
    
    auto* serverInfo = new QLabel(
        "<b>Auto-detect:</b> Automatically detect your display server<br>"
        "<b>X11:</b> Force X11 mode<br>"
        "<b>Wayland:</b> Force Wayland mode"
    );
    serverInfo->setStyleSheet("color: #666; font-size: 11px;");
    serverGroupLayout->addWidget(serverInfo);
    serverGroupLayout->addStretch();
    
    displayLayout->addWidget(serverGroup);
    displayLayout->addStretch();

    // --- Theme Settings Tab ---
    auto* themeTab = new QWidget();
    auto* themeLayout = new QVBoxLayout(themeTab);
    
    auto* themeFilesGroup = new QGroupBox("Theme File Names");
    auto* themeFilesLayout = new QVBoxLayout(themeFilesGroup);
    
    themeExtensionEdit = new QLineEdit();
    themeExtensionEdit->setPlaceholderText("e.g., .lua");
    themeFilesLayout->addWidget(new QLabel("Theme File Extension:"));
    themeFilesLayout->addWidget(themeExtensionEdit);
    
    currentThemeFileEdit = new QLineEdit();
    currentThemeFileEdit->setPlaceholderText("e.g., current.lua");
    themeFilesLayout->addWidget(new QLabel("Current Theme File Name:"));
    themeFilesLayout->addWidget(currentThemeFileEdit);
    
    themeFilesLayout->addStretch();
    themeLayout->addWidget(themeFilesGroup);
    themeLayout->addStretch();

    // Add all tabs
    tabs->addTab(pathsTab, "Paths");
    tabs->addTab(panelsTab, "Start Mains");
    tabs->addTab(editorsTab, "Editors");
    tabs->addTab(discoveryTab, "Panel Discovery");
    tabs->addTab(refreshTab, "Refresh & Window");
    tabs->addTab(displayTab, "Display Server");
    tabs->addTab(themeTab, "Themes");

    mainLayout->addWidget(tabs);

    // Bottom Buttons
    auto* bottomButtons = new QHBoxLayout();
    auto* saveBtn = new QPushButton("Save & Apply");
    saveBtn->setStyleSheet("background-color: #1a7f37; color: white; font-weight: bold; padding: 8px;");
    auto* cancelBtn = new QPushButton("Cancel");
    bottomButtons->addStretch();
    bottomButtons->addWidget(cancelBtn);
    bottomButtons->addWidget(saveBtn);
    mainLayout->addLayout(bottomButtons);

    connect(saveBtn, &QPushButton::clicked, this, &PreferencesDialog::saveAndAccept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void PreferencesDialog::loadCurrentConfig() {
    auto& config = ConfigManager::instance();
    
    // Paths
    conkyPathEdit->setText(QString::fromStdString(config.get_conky_wayland_directory().string()));
    themesPathEdit->setText(QString::fromStdString(config.get_themes_directory().string()));
    
    // Panels
    for (const auto& panel : config.get_ui_config().default_panels_to_start) {
        panelsList->addItem(QString::fromStdString(panel));
    }
    
    // Editors
    for (const auto& editor : config.get_editors()) {
        int row = editorsTable->rowCount();
        editorsTable->insertRow(row);
        editorsTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(editor.name)));
        editorsTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(editor.command)));
        editorsTable->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(editor.icon)));
    }
    
    // Panel Discovery
    configPrefixEdit->setText(QString::fromStdString(config.get_panel_discovery_config().config_prefix));
    configExtensionEdit->setText(QString::fromStdString(config.get_panel_discovery_config().config_extension));
    
    QString excludedFiles;
    for (const auto& file : config.get_panel_discovery_config().excluded_files) {
        excludedFiles += QString::fromStdString(file) + "\n";
    }
    excludedFilesEdit->setPlainText(excludedFiles.trimmed());
    
    // Refresh & Window
    heartbeatSpin->setValue(config.get_heartbeat_interval());
    panelStatusSpin->setValue(config.get_panel_status_interval());
    minWidthSpin->setValue(config.get_min_window_width());
    minHeightSpin->setValue(config.get_min_window_height());
    defaultWidthSpin->setValue(config.get_default_window_width());
    defaultHeightSpin->setValue(config.get_default_window_height());
    
    // Display Server
    QString currentServer = QString::fromStdString(config.get_display_server());
    int serverIndex = displayServerCombo->findData(currentServer);
    if (serverIndex >= 0) {
        displayServerCombo->setCurrentIndex(serverIndex);
    }
    
    // Theme Settings
    themeExtensionEdit->setText(QString::fromStdString(config.get_theme_extension()));
    currentThemeFileEdit->setText(QString::fromStdString(config.get_current_theme_file()));
}

void PreferencesDialog::saveAndAccept() {
    auto& config = ConfigManager::instance();
    
    // Update Paths
    config.set_conky_config_path(conkyPathEdit->text().toStdString());
    config.set_themes_path(themesPathEdit->text().toStdString());
    
    // Update Panels
    config.get_ui_config().default_panels_to_start.clear();
    for(int i = 0; i < panelsList->count(); ++i) {
        config.get_ui_config().default_panels_to_start.push_back(panelsList->item(i)->text().toStdString());
    }
    
    // Update Editors
    config.get_editors().clear();
    for(int i = 0; i < editorsTable->rowCount(); ++i) {
        auto* item0 = editorsTable->item(i, 0);
        auto* item1 = editorsTable->item(i, 1);
        auto* item2 = editorsTable->item(i, 2);

        if (item0 && item1 && item2) {
            config.get_editors().push_back({
                item0->text().toStdString(),
                item1->text().toStdString(),
                item2->text().toStdString()
            });
        }
    }
    
    // Update Panel Discovery
    config.get_panel_discovery_config().config_prefix = configPrefixEdit->text().toStdString();
    config.get_panel_discovery_config().config_extension = configExtensionEdit->text().toStdString();
    
    config.get_panel_discovery_config().excluded_files.clear();
    QStringList excludedList = excludedFilesEdit->toPlainText().split("\n", Qt::SkipEmptyParts);
    for (const QString& file : excludedList) {
        config.get_panel_discovery_config().excluded_files.push_back(file.trimmed().toStdString());
    }
    
    // Update Refresh & Window
    config.get_ui_config().refresh_intervals.heartbeat_seconds = heartbeatSpin->value();
    config.get_ui_config().refresh_intervals.panel_status_seconds = panelStatusSpin->value();
    config.get_ui_config().window.min_width = minWidthSpin->value();
    config.get_ui_config().window.min_height = minHeightSpin->value();
    config.get_ui_config().window.default_width = defaultWidthSpin->value();
    config.get_ui_config().window.default_height = defaultHeightSpin->value();
    
    // Update Display Server
    config.set_display_server(displayServerCombo->currentData().toString().toStdString());
    
    // Update Theme Settings
    config.get_themes_config().file_extension = themeExtensionEdit->text().toStdString();
    config.get_themes_config().current_theme_file = currentThemeFileEdit->text().toStdString();
    
    if (config.save_config()) {
        accept();
    } else {
        QMessageBox::critical(this, "Error", "Failed to save configuration to app_config.json");
    }
}
