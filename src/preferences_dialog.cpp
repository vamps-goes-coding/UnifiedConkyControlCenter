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

PreferencesDialog::PreferencesDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Preferences");
    setMinimumSize(650, 500);
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
        editorsTable->setItem(row, 2, new QTableWidgetItem("📝"));
    });
    connect(remEditor, &QPushButton::clicked, [this]() {
        editorsTable->removeRow(editorsTable->currentRow());
    });

    // --- App Info Tab ---
    auto* infoTab = new QWidget();
    auto* infoLayout = new QVBoxLayout(infoTab);
    appNameEdit = new QLineEdit();
    infoLayout->addWidget(new QLabel("Application Display Name:"));
    infoLayout->addWidget(appNameEdit);
    infoLayout->addStretch();

    tabs->addTab(pathsTab, "Paths");
    tabs->addTab(panelsTab, "Start Mains");
    tabs->addTab(editorsTab, "Editors");
    tabs->addTab(infoTab, "General");

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
    
    conkyPathEdit->setText(QString::fromStdString(config.get_conky_wayland_directory().string()));
    themesPathEdit->setText(QString::fromStdString(config.get_themes_directory().string()));
    appNameEdit->setText(QString::fromStdString(config.get_app_config().display_name));

    for (const auto& panel : config.get_ui_config().default_panels_to_start) {
        panelsList->addItem(QString::fromStdString(panel));
    }

    for (const auto& editor : config.get_editors()) {
        int row = editorsTable->rowCount();
        editorsTable->insertRow(row);
        editorsTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(editor.name)));
        editorsTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(editor.command)));
        editorsTable->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(editor.icon)));
    }
}

void PreferencesDialog::saveAndAccept() {
    auto& config = ConfigManager::instance();
    
    // Update Paths
    config.set_conky_config_path(conkyPathEdit->text().toStdString());
    config.set_themes_path(themesPathEdit->text().toStdString());
    
    // Update App Info
    config.get_app_config().display_name = appNameEdit->text().toStdString();
    
    // Update Panels
    config.get_ui_config().default_panels_to_start.clear();
    for(int i = 0; i < panelsList->count(); ++i) {
        config.get_ui_config().default_panels_to_start.push_back(panelsList->item(i)->text().toStdString());
    }
    
    // Update Editors
    config.get_editors().clear();
    for(int i = 0; i < editorsTable->rowCount(); ++i) {
        config.get_editors().push_back({
            editorsTable->item(i, 0)->text().toStdString(),
            editorsTable->item(i, 1)->text().toStdString(),
            editorsTable->item(i, 2)->text().toStdString()
        });
    }

    if (config.save_config()) {
        accept();
    } else {
        QMessageBox::critical(this, "Error", "Failed to save configuration to app_config.json");
    }
}