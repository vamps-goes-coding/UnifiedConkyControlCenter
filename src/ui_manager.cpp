#include "ui_manager.h"
#include "app_info.h"
#include "conky_manager.h"
#include "utils.h"
#include "config_parser.h"
#include "config_manager.h"
#include "theme_manager.h"
#include "in_app_editor.h"
#include "preferences_dialog.h"
#include <filesystem>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include <functional>
namespace fs = std::filesystem;

#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QSpinBox>
#include <QTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QIcon>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QDateTime>
#include <QGroupBox>
#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>
#include <QDesktopServices>
#include <QUrl>
#include <QLineEdit>
#include <QCheckBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QSplitter>
#include <QStatusBar>
#include <QMenuBar>
#include <QColorDialog>
#include <QInputDialog>
#include <QPainter>
#include <fstream>
#include <iostream>

// Custom QMainWindow subclass to handle cleanup on close
class ConkyControlCenterWindow : public QMainWindow {
public:
    ConkyControlCenterWindow(QWidget* parent = nullptr) : QMainWindow(parent) {}

protected:
    void closeEvent(QCloseEvent* event) override {
        // Wait for any pending panel restart operations to complete
        ConkyManager::wait_for_pending_restarts(3000);

        // Allow normal close without stopping panels
        QMainWindow::closeEvent(event);
    }
};

// Static member definitions
QApplication* UIManager::app_instance = nullptr;
QWidget* UIManager::main_window_instance = nullptr;
QSystemTrayIcon* UIManager::tray_icon_instance = nullptr;

// Application lifecycle
int UIManager::initialize_application(int argc, char* argv[]) {
    if (!app_instance) {
        static int static_argc = argc;
        static char** static_argv = argv;
        app_instance = new QApplication(static_argc, static_argv);
        
        // Set application metadata
        app_instance->setApplicationName(QString::fromUtf8(AppInfo::kInternalName()));
        app_instance->setApplicationDisplayName(QString::fromUtf8(AppInfo::kDisplayName()));
        app_instance->setApplicationVersion(QString::fromUtf8(AppInfo::kVersion()));
        app_instance->setOrganizationName(QString::fromUtf8(AppInfo::kOrganization()));
        
        // Set essential attributes for modern Qt6 behavior
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        app_instance->setAttribute(Qt::AA_EnableHighDpiScaling, true);
        app_instance->setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif
        // Set application icon
        QIcon app_icon(":/icons/conky-unified-center-icon.png");
        if (!app_icon.isNull()) {
            app_instance->setWindowIcon(app_icon);
        }
    }
    return 0;
}

// Helper function to read current Conky theme from file
std::string get_current_conky_theme() {
    fs::path current_theme_txt = Utils::themes_directory() / "current_theme.txt";
    if (fs::exists(current_theme_txt)) {
        std::ifstream file(current_theme_txt);
        if (file.is_open()) {
            std::string theme_name;
            std::getline(file, theme_name);
            file.close();
            if (!theme_name.empty()) {
                return theme_name;
            }
        }
    }
    return "Default";
}

void UIManager::run_application() {
    std::cerr << "DEBUG: run_application called, app_instance=" << app_instance << std::endl;
    if (app_instance) {
        std::cerr << "DEBUG: Calling verify_panel_state_on_startup" << std::endl;
        ConkyManager::verify_panel_state_on_startup();
        std::cerr << "DEBUG: verify done, calling create_main_window" << std::endl;
        
std::cerr << "DEBUG: Calling create_main_window" << std::endl;
create_main_window();
std::cerr << "DEBUG: create_main_window done, instance=" << main_window_instance << std::endl;
show_main_window();
std::cerr << "DEBUG: show_main_window done" << std::endl;
        
        // Update status bar with current Conky theme
        if (main_window_instance) {
            QLabel* theme_label = main_window_instance->findChild<QLabel*>("statusBarThemeLabel");
            if (theme_label) {
                std::string current_theme = get_current_conky_theme();
                theme_label->setText(QString("Theme: %1").arg(QString::fromStdString(current_theme)));
            }
        }
        
        // Trigger initial data load
        refresh_all_tabs();

        // ── App-wide heartbeat timer ──────────────────────────────────
        // Fires every 10 seconds while the app is open and idle.
        // Keeps the running config cache, zombie process handles, and
        // the status bar timestamp in sync with reality automatically —
        // no user interaction required.
        //
        // Note: the Start/Stop tab has its own 5s panel-card refresh
        // timer. This heartbeat is a lighter app-level complement that
        // also cleans up dead QProcess handles and updates the status bar.
        QTimer* heartbeat = new QTimer(app_instance);
        QObject::connect(heartbeat, &QTimer::timeout, []() {
            // Clean up any QProcess handles for processes that have died
            ConkyManager::reap_zombies();

            // Force a fresh pgrep scan so the cache reflects reality
            auto running_configs = ConkyManager::get_running_configs(true);

            // Update the status bar with current information
            if (main_window_instance) {
                // Update running panels count
                QLabel* running_label = main_window_instance->findChild<QLabel*>("statusBarRunningLabel");
                if (running_label) {
                    int running_count = running_configs.size();
                    running_label->setText(QString("Panels: %1 running").arg(running_count));
                }
                
                // Update last refresh time
                QLabel* refresh_label = main_window_instance->findChild<QLabel*>("statusBarRefreshLabel");
                if (refresh_label) {
                    double seconds = ConkyManager::seconds_since_refresh();
                    if (seconds < 0) {
                        refresh_label->setText("Last refresh: Never");
                    } else if (seconds < 60) {
                        refresh_label->setText(QString("Last refresh: %1s ago").arg(static_cast<int>(seconds)));
                    } else {
                        int minutes = static_cast<int>(seconds) / 60;
                        refresh_label->setText(QString("Last refresh: %1m ago").arg(minutes));
                    }
                }
                
                // Update current Conky theme
                QLabel* theme_label = main_window_instance->findChild<QLabel*>("statusBarThemeLabel");
                if (theme_label) {
                    std::string current_theme = get_current_conky_theme();
                    theme_label->setText(QString("Theme: %1").arg(QString::fromStdString(current_theme)));
                }
            }
        });
        heartbeat->start(10000); // every 10 seconds
        // ─────────────────────────────────────────────────────────────

        app_instance->exec();
    }
}

void UIManager::quit_application() {
    if (app_instance) {
        app_instance->quit();
    }
}

// Main window management
QWidget* UIManager::create_main_window() {
    if (main_window_instance) {
        return main_window_instance;
    }
    
    QMainWindow* window = new ConkyControlCenterWindow();
    window->setWindowTitle(QString::fromUtf8(AppInfo::kDisplayName()));
    window->setMinimumSize(900, 700);
    window->resize(1000, 750);
    
    // Create central widget with tab widget
    QWidget* central_widget = new QWidget();
    QVBoxLayout* main_layout = new QVBoxLayout(central_widget);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);
    
    // Create menu bar
    QMenuBar* menu_bar = new QMenuBar();
    QMenu* file_menu = menu_bar->addMenu("&File");
    file_menu->addAction("&Preferences...", [window]() {
        PreferencesDialog dialog(window);
        if (dialog.exec() == QDialog::Accepted) {
            refresh_all_tabs();
        }
    });
    file_menu->addSeparator();
    file_menu->addAction("&Restart App", []() {
        QString executable = QApplication::applicationFilePath();
        QStringList arguments = QApplication::arguments();
        if (!arguments.isEmpty()) {
            arguments.removeFirst(); // Remove the executable name
        }

        // Launch the new instance first, then wait a moment before quitting.
        // Without the delay, QApplication::quit() tears down the event loop
        // before the new process is fully on its feet, causing it to either
        // never appear or get silently killed with the parent.
        bool launched = QProcess::startDetached(executable, arguments, QDir::currentPath());
        if (launched) {
            QTimer::singleShot(300, []() {
                ConkyManager::cleanup_on_exit();
                QApplication::quit();
            });
        }
    });
    file_menu->addSeparator();
    file_menu->addAction("&Quit", window, &QMainWindow::close);
    
    QMenu* view_menu = menu_bar->addMenu("&View");
    view_menu->addAction("&Refresh All", []() {
        refresh_all_tabs();
    });
    
    QMenu* app_theme_menu = view_menu->addMenu("&App Theme");
    std::vector<std::string> app_themes = {"Default Light", "Dark Charcoal", "Dracula", "Nord", "Solarized Light", "Oceanic"};
    for (const auto& theme : app_themes) {
        app_theme_menu->addAction(QString::fromStdString(theme), [theme]() {
            apply_app_theme(theme);
        });
    }
    
    QMenu* help_menu = menu_bar->addMenu("&Help");
    help_menu->addAction("&About", [window]() {
        QMessageBox::about(window, QStringLiteral("About %1").arg(QString::fromUtf8(AppInfo::kDisplayName())),
            QStringLiteral("%1 v%2\n\n"
            "A graphical interface for managing Conky panels,\n"
            "themes, and configurations.")
                .arg(QString::fromUtf8(AppInfo::kDisplayName()))
                .arg(QString::fromUtf8(AppInfo::kVersion())));
    });
    
    window->setMenuBar(menu_bar);

    // Mode header similar to the Python implementation
    QFrame* mode_header = new QFrame();
    mode_header->setObjectName("modeHeader");
    mode_header->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    mode_header->setStyleSheet(
        "QFrame#modeHeader { "
        "  background-color: #24292e; "
        "  border-bottom: 1px solid #444c56; "
        "}"
    );
    QHBoxLayout* mode_layout = new QHBoxLayout(mode_header);
    
    // Create tab widget
    QLabel* mode_label = new QLabel("Mode:");
    mode_label->setStyleSheet("font-weight: bold; font-size: 10pt; color: #f0f6fc;");
    mode_layout->addWidget(mode_label);

    QPushButton* btn_panel_control = new QPushButton("Panel Control");
    btn_panel_control->setCheckable(true);
    btn_panel_control->setChecked(true);

    QPushButton* btn_theme_control = new QPushButton("Theme Control");
    btn_theme_control->setCheckable(true);

    mode_layout->addWidget(btn_panel_control);
    mode_layout->addWidget(btn_theme_control);
    mode_layout->addStretch();
    main_layout->addWidget(mode_header);
    
    // Single level tab widget that we will swap the content of
    QTabWidget* tab_widget = new QTabWidget(central_widget);
    tab_widget->setObjectName("mainTabs");
    main_layout->addWidget(tab_widget);

    // Define switching logic
    auto switch_to_panel_control = [=]() {
        btn_panel_control->setChecked(true);
        btn_theme_control->setChecked(false);
        tab_widget->clear();
        tab_widget->addTab(create_theme_tab(tab_widget), "Themes");
        tab_widget->addTab(create_gap_tab(tab_widget), "Gaps");
        tab_widget->addTab(create_start_stop_tab(tab_widget), "Start/Stop");
        tab_widget->addTab(create_editor_tab(tab_widget), "Editors");
    };

    auto switch_to_theme_control = [=]() {
        btn_panel_control->setChecked(false);
        btn_theme_control->setChecked(true);
        tab_widget->clear();
        tab_widget->addTab(create_theme_creator_tab(tab_widget), "Creator");
        tab_widget->addTab(create_theme_editor_tab(tab_widget), "Theme Editor");
        tab_widget->addTab(create_theme_manager_tab(tab_widget), "Theme Manager");
    };

    QObject::connect(btn_panel_control, &QPushButton::clicked, switch_to_panel_control);
    QObject::connect(btn_theme_control, &QPushButton::clicked, switch_to_theme_control);

    // Start in Panel Control mode
    switch_to_panel_control();
    
    // Create status bar at the bottom
    QFrame* status_bar = new QFrame();
    status_bar->setObjectName("mainStatusBar");
    status_bar->setFrameStyle(QFrame::StyledPanel);
    status_bar->setStyleSheet(
        "QFrame#mainStatusBar { "
        "  background-color: #2d333b; "
        "  border-top: 1px solid #444c56; "
        "  padding: 8px 15px; "
        "}"
    );
    status_bar->setFixedHeight(50);
    
    QHBoxLayout* status_layout = new QHBoxLayout(status_bar);
    status_layout->setContentsMargins(10, 5, 10, 5);
    status_layout->setSpacing(20);
    
    // Running panels indicator
    QLabel* running_icon = new QLabel("🔳");
    running_icon->setStyleSheet("font-size: 16px; background: transparent; border: none;");
    QLabel* running_label = new QLabel("Panels: 0 running");
    running_label->setObjectName("statusBarRunningLabel");
    running_label->setStyleSheet("color: #f0f6fc; font-size: 12px; font-weight: bold; background: transparent; border: none;");
    
    QHBoxLayout* running_layout = new QHBoxLayout();
    running_layout->setSpacing(5);
    running_layout->addWidget(running_icon);
    running_layout->addWidget(running_label);
    
    // Separator
    QFrame* sep1 = new QFrame();
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("background-color: #444c56; max-width: 1px;");
    
    // Theme indicator
    QLabel* theme_icon = new QLabel("🎨");
    theme_icon->setStyleSheet("font-size: 16px; background: transparent; border: none;");
    QLabel* theme_label = new QLabel("Theme: Default");
    theme_label->setObjectName("statusBarThemeLabel");
    theme_label->setStyleSheet("color: #f0f6fc; font-size: 12px; background: transparent; border: none;");
    
    QHBoxLayout* theme_layout = new QHBoxLayout();
    theme_layout->setSpacing(5);
    theme_layout->addWidget(theme_icon);
    theme_layout->addWidget(theme_label);
    
    // Separator
    QFrame* sep2 = new QFrame();
    sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet("background-color: #444c56; max-width: 1px;");
    
    // Last refreshed indicator
    QLabel* refresh_icon = new QLabel("🔄");
    refresh_icon->setStyleSheet("font-size: 16px; background: transparent; border: none;");
    QLabel* refresh_label = new QLabel("Last refresh: Never");
    refresh_label->setObjectName("statusBarRefreshLabel");
    refresh_label->setStyleSheet("color: #f0f6fc; font-size: 12px; background: transparent; border: none;");
    
    QHBoxLayout* refresh_layout = new QHBoxLayout();
    refresh_layout->setSpacing(5);
    refresh_layout->addWidget(refresh_icon);
    refresh_layout->addWidget(refresh_label);
    
    // Version indicator (Visible at all times - high contrast)
    QLabel* version_icon = new QLabel("ℹ");
    version_icon->setStyleSheet("font-size: 14px; color: #58a6ff; background: transparent; border: none;");
    QLabel* version_label = new QLabel(QString("v%1").arg(QString::fromUtf8(AppInfo::kVersion())));
    version_label->setObjectName("statusBarVersionLabel");
    version_label->setStyleSheet("color: #58a6ff; font-size: 12px; font-weight: bold; background: transparent; border: none;");
    version_label->setToolTip("Application Version");

    // Spacer
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    
    // Start Running Panels button
    QPushButton* start_running_btn = new QPushButton("▶ Start Mains");
    start_running_btn->setObjectName("statusBarStartBtn");
    start_running_btn->setStyleSheet(
        "QPushButton { "
        "  background-color: #1a7f37; "
        "  color: white; "
        "  border: none; "
        "  border-radius: 4px; "
        "  padding: 6px 12px; "
        "  font-size: 11px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { "
        "  background-color: #2ea043; "
        "} "
        "QPushButton:pressed { "
        "  background-color: #1a7f37; "
        "}"
    );
    start_running_btn->setToolTip("Start all main panels");
    QObject::connect(start_running_btn, &QPushButton::clicked, []() {
        try {
            // Use dynamic list from config instead of hardcoded values
            auto panels_to_start = ConfigManager::instance().get_ui_config().default_panels_to_start;
            
            // Start each panel
            for (const auto& panel : panels_to_start) {
                ConkyManager::start_panel(panel);
            }
            
            show_tray_message("Panels", "Starting all main panels...");
        } catch (const std::exception& e) {
            QMessageBox::warning(nullptr, "Startup Error", 
                QString("Failed to start one or more default panels: %1").arg(e.what()));
        }
        
        // Refresh after a delay
        QTimer::singleShot(2000, []() {
            refresh_all_tabs();
        });
    });
    
    // Add all elements to status bar
    status_layout->addLayout(running_layout);
    status_layout->addWidget(sep1);
    status_layout->addLayout(theme_layout);
    status_layout->addWidget(sep2);
    status_layout->addLayout(refresh_layout);
    status_layout->addWidget(sep2);
    status_layout->addWidget(version_icon);
    status_layout->addWidget(version_label);
    status_layout->addWidget(spacer);
    status_layout->addWidget(start_running_btn);
    
    main_layout->addWidget(status_bar);
    
    window->setCentralWidget(central_widget);
    
    // Setup system tray
    setup_system_tray();
    
    main_window_instance = window;
    return window;
}

void UIManager::setup_main_window(QWidget* window) {
    // Additional setup if needed
}

void UIManager::show_main_window() {
    if (main_window_instance) {
        // Ensure window is visible and brought to front
        main_window_instance->show();
        main_window_instance->raise();
        main_window_instance->activateWindow();
        
        // On Wayland, additional handling may be needed
        // Force the window to be on top and focused
        main_window_instance->setWindowState(main_window_instance->windowState() & ~Qt::WindowMinimized);
        main_window_instance->setWindowState(main_window_instance->windowState() | Qt::WindowActive);
        
        // Ensure window is visible
        if (!main_window_instance->isVisible()) {
            main_window_instance->setVisible(true);
        }
        
        // Bring to front explicitly
        main_window_instance->raise();
        main_window_instance->activateWindow();
    }
}

void UIManager::hide_main_window() {
    if (main_window_instance) {
        main_window_instance->hide();
    }
}

void UIManager::apply_app_theme(const std::string& theme_name) {
    if (!app_instance) return;
    
    // Update status bar theme label
    if (main_window_instance) {
        QLabel* theme_label = main_window_instance->findChild<QLabel*>("statusBarThemeLabel");
        if (theme_label) {
            theme_label->setText(QString("Theme: %1").arg(QString::fromStdString(theme_name)));
        }
    }
    
    QString qss = "";
    if (theme_name == "Default Light") {
        qss = ""; // Reset to default
    } else if (theme_name == "Dark Charcoal") {
        qss = "QWidget { background-color: #1e1e1e; color: #e0e0e0; } "
              "QMenuBar { background-color: #252526; color: #cccccc; } "
              "QMenuBar::item:selected { background-color: #3e3e42; } "
              "QMenu { background-color: #252526; border: 1px solid #3e3e42; } "
              "QTabWidget::pane { border: 1px solid #333333; background: #1e1e1e; } "
              "QTabBar::tab { background: #2d2d2d; color: #cccccc; padding: 8px 15px; border: 1px solid #333333; } "
              "QTabBar::tab:selected { background: #1e1e1e; color: #ffffff; border-bottom-color: #1e1e1e; } "
              "QPushButton { background-color: #333333; border: 1px solid #555555; padding: 5px; border-radius: 3px; } "
              "QPushButton:hover { background-color: #444444; border-color: #007acc; } "
              "QListWidget, QTableWidget, QTextEdit, QComboBox, QSpinBox, QLineEdit { background-color: #252526; color: #d4d4d4; border: 1px solid #3e3e42; selection-background-color: #062f4a; } "
              "QGroupBox { border: 1px solid #3e3e42; margin-top: 1ex; padding-top: 10px; } "
              "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #007acc; } "
              "QScrollArea { background-color: transparent; border: none; } ";
    } else if (theme_name == "Dracula") {
        qss = "QWidget { background-color: #282a36; color: #f8f8f2; } "
              "QMenuBar { background-color: #21222c; color: #f8f8f2; } "
              "QMenuBar::item:selected { background-color: #44475a; } "
              "QMenu { background-color: #21222c; border: 1px solid #44475a; } "
              "QTabWidget::pane { border: 1px solid #44475a; background: #282a36; } "
              "QTabBar::tab { background: #44475a; color: #6272a4; padding: 8px 15px; border: 1px solid #44475a; } "
              "QTabBar::tab:selected { background: #282a36; color: #bd93f9; border-bottom-color: #282a36; } "
              "QPushButton { background-color: #44475a; border: 1px solid #6272a4; padding: 5px; border-radius: 3px; } "
              "QPushButton:hover { background-color: #6272a4; border-color: #ff79c6; } "
              "QListWidget, QTableWidget, QTextEdit, QComboBox, QSpinBox, QLineEdit { background-color: #21222c; color: #f8f8f2; border: 1px solid #44475a; selection-background-color: #44475a; } "
              "QGroupBox { border: 1px solid #6272a4; margin-top: 1ex; padding-top: 10px; } "
              "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #ff79c6; } "
              "QScrollArea { background-color: transparent; border: none; } ";
    } else if (theme_name == "Nord") {
        qss = "QWidget { background-color: #2e3440; color: #d8dee9; } "
              "QMenuBar { background-color: #242933; color: #d8dee9; } "
              "QMenuBar::item:selected { background-color: #434c5e; } "
              "QMenu { background-color: #242933; border: 1px solid #434c5e; } "
              "QTabWidget::pane { border: 1px solid #434c5e; background: #2e3440; } "
              "QTabBar::tab { background: #3b4252; color: #4c566a; padding: 8px 15px; border: 1px solid #434c5e; } "
              "QTabBar::tab:selected { background: #2e3440; color: #88c0d0; border-bottom-color: #2e3440; } "
              "QPushButton { background-color: #434c5e; border: 1px solid #4c566a; padding: 5px; border-radius: 3px; } "
              "QPushButton:hover { background-color: #4c566a; border-color: #81a1c1; } "
              "QListWidget, QTableWidget, QTextEdit, QComboBox, QSpinBox, QLineEdit { background-color: #3b4252; color: #eceff4; border: 1px solid #4c566a; selection-background-color: #4c566a; } "
              "QGroupBox { border: 1px solid #4c566a; margin-top: 1ex; padding-top: 10px; } "
              "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #81a1c1; } "
              "QScrollArea { background-color: transparent; border: none; } ";
    } else if (theme_name == "Solarized Light") {
        qss = "QWidget { background-color: #fdf6e3; color: #657b83; } "
              "QMenuBar { background-color: #eee8d5; color: #657b83; } "
              "QMenuBar::item:selected { background-color: #d33682; color: #fdf6e3; } "
              "QMenu { background-color: #eee8d5; border: 1px solid #93a1a1; } "
              "QTabWidget::pane { border: 1px solid #93a1a1; background: #fdf6e3; } "
              "QTabBar::tab { background: #eee8d5; color: #93a1a1; padding: 8px 15px; border: 1px solid #93a1a1; } "
              "QTabBar::tab:selected { background: #fdf6e3; color: #b58900; border-bottom-color: #fdf6e3; } "
              "QPushButton { background-color: #eee8d5; border: 1px solid #93a1a1; padding: 5px; border-radius: 3px; } "
              "QPushButton:hover { background-color: #93a1a1; color: #fdf6e3; } "
              "QListWidget, QTableWidget, QTextEdit, QComboBox, QSpinBox, QLineEdit { background-color: #fdf6e3; color: #657b83; border: 1px solid #93a1a1; selection-background-color: #eee8d5; } "
              "QGroupBox { border: 1px solid #93a1a1; margin-top: 1ex; padding-top: 10px; } "
              "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #268bd2; } "
              "QScrollArea { background-color: transparent; border: none; } ";
    } else if (theme_name == "Oceanic") {
        qss = "QWidget { background-color: #0f172a; color: #e2e8f0; } "
              "QMenuBar { background-color: #0b1120; color: #e2e8f0; } "
              "QMenuBar::item:selected { background-color: #1e293b; } "
              "QMenu { background-color: #0b1120; border: 1px solid #1e293b; } "
              "QTabWidget::pane { border: 1px solid #1e293b; background: #0f172a; } "
              "QTabBar::tab { background: #1e293b; color: #64748b; padding: 8px 15px; border: 1px solid #1e293b; } "
              "QTabBar::tab:selected { background: #0f172a; color: #38bdf8; border-bottom-color: #0f172a; } "
              "QPushButton { background-color: #1e293b; border: 1px solid #334155; padding: 5px; border-radius: 3px; } "
              "QPushButton:hover { background-color: #334155; border-color: #38bdf8; } "
              "QListWidget, QTableWidget, QTextEdit, QComboBox, QSpinBox, QLineEdit { background-color: #1e293b; color: #f8fafc; border: 1px solid #334155; selection-background-color: #334155; } "
              "QGroupBox { border: 1px solid #334155; margin-top: 1ex; padding-top: 10px; } "
              "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #38bdf8; } "
              "QScrollArea { background-color: transparent; border: none; } ";
    }
    
    app_instance->setStyleSheet(qss);
}

// Tab management
QTabWidget* UIManager::create_tab_widget(QWidget* /*parent*/) {
    // This functionality has been moved to create_main_window for mode switching.
    return nullptr;
}

void UIManager::add_tab(QTabWidget* tab_widget, QWidget* tab_content, const std::string& tab_name) {
    if (tab_widget && tab_content) {
        tab_widget->addTab(tab_content, QString::fromStdString(tab_name));
    }
}

void UIManager::remove_tab(QTabWidget* tab_widget, int index) {
    if (tab_widget && index >= 0 && index < tab_widget->count()) {
        tab_widget->removeTab(index);
    }
}

// Start/Stop tab functionality
QWidget* UIManager::create_start_stop_tab(QWidget* parent) {
    QWidget* tab = new QWidget(parent);
    QVBoxLayout* main_layout = new QVBoxLayout(tab);
    main_layout->setSpacing(15);
    main_layout->setContentsMargins(20, 20, 20, 20);
    
    // Title
    QLabel* title = new QLabel("Start/Stop");
    title->setStyleSheet("font-size: 18px; font-weight: bold; margin-bottom: 10px;");
    main_layout->addWidget(title);
    
    // Panel status grid in a scroll area
    QGroupBox* status_group = new QGroupBox("Panel Status");
    QVBoxLayout* status_layout = new QVBoxLayout(status_group);
    
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background: transparent;");
    
    QWidget* grid_container = new QWidget();
    grid_container->setObjectName("statusGridContainer");
    QGridLayout* status_grid = new QGridLayout(grid_container);
    status_grid->setObjectName("statusGrid");
    status_grid->setSpacing(15);
    status_grid->setContentsMargins(5, 5, 5, 5);
    
    scroll->setWidget(grid_container);
    status_layout->addWidget(scroll);
    main_layout->addWidget(status_group, 1);
    
    // Global controls
    QGroupBox* global_group = new QGroupBox("Global Controls");
    QHBoxLayout* global_layout = new QHBoxLayout(global_group);
    
    QPushButton* start_all_btn = new QPushButton("Start All");
    start_all_btn->setStyleSheet("QPushButton { padding: 10px 20px; font-weight: bold; }");
    QObject::connect(start_all_btn, &QPushButton::clicked, []() {
        start_all_panels();
    });
    
    QPushButton* stop_all_btn = new QPushButton("Stop All");
    stop_all_btn->setStyleSheet("QPushButton { padding: 10px 20px; font-weight: bold; }");
    QObject::connect(stop_all_btn, &QPushButton::clicked, []() {
        stop_all_panels();
    });
    
    QPushButton* restart_active_btn = new QPushButton("Restart Active");
    restart_active_btn->setStyleSheet("QPushButton { padding: 10px 20px; font-weight: bold; }");
    QObject::connect(restart_active_btn, &QPushButton::clicked, []() {
        restart_active_panels();
    });
    
    QPushButton* refresh_btn = new QPushButton("Refresh");
    refresh_btn->setStyleSheet("QPushButton { padding: 10px 20px; }");
    QObject::connect(refresh_btn, &QPushButton::clicked, [status_grid]() {
        refresh_panel_status(nullptr, status_grid);
    });
    
    global_layout->addWidget(start_all_btn);
    global_layout->addWidget(stop_all_btn);
    global_layout->addWidget(restart_active_btn);
    global_layout->addWidget(refresh_btn);
    global_layout->addStretch();
    
    main_layout->addWidget(global_group);
    
    // Quick stats
    QGroupBox* stats_group = new QGroupBox("Quick Stats");
    QHBoxLayout* stats_layout = new QHBoxLayout(stats_group);
    
    QLabel* running_label = new QLabel("Running: 0");
    running_label->setObjectName("runningLabel");
    QLabel* stopped_label = new QLabel("Stopped: 0");
    stopped_label->setObjectName("stoppedLabel");
    QLabel* total_label = new QLabel("Total: 0");
    total_label->setObjectName("totalLabel");
    QLabel* refresh_label = new QLabel("Last Refresh: Never");
    refresh_label->setObjectName("refreshLabel");
    
    stats_layout->addWidget(running_label);
    stats_layout->addWidget(new QLabel("|"));
    stats_layout->addWidget(stopped_label);
    stats_layout->addWidget(new QLabel("|"));
    stats_layout->addWidget(total_label);
    stats_layout->addStretch();
    stats_layout->addWidget(refresh_label);
    
    main_layout->addWidget(stats_group);
    
    // Initial refresh
    QTimer::singleShot(100, [status_grid]() {
        refresh_panel_status(nullptr, status_grid);
    });
    
    // Auto-refresh timer
    QTimer* refresh_timer = new QTimer(tab);
    QObject::connect(refresh_timer, &QTimer::timeout, [status_grid]() {
        refresh_panel_status(nullptr, status_grid);
    });
    refresh_timer->start(5000); // Refresh every 5 seconds
    
    return tab;
}

void UIManager::refresh_panel_status(QTableWidget* /*deprecated_table*/, QGridLayout* status_grid_arg) {
    // Find the status grid if it's not provided
    QGridLayout* status_grid = status_grid_arg;
    if (!status_grid && main_window_instance) {
        status_grid = main_window_instance->findChild<QGridLayout*>("statusGrid");
    }
    
    if (!status_grid) {
        return;
    }
    
    // Get panels and running configs
    auto panels = Utils::discover_panels();
    auto running_configs_raw = ConkyManager::get_running_configs(true);
    
    // Pre-calculate absolute paths for running configs
    std::vector<std::string> running_configs;
    for (const auto& r : running_configs_raw) {
        try {
            running_configs.push_back(fs::absolute(fs::path(r)).string());
        } catch (...) {
            running_configs.push_back(r);
        }
    }

    // Check if we need to completely rebuild the grid
    bool needs_rebuild = true;
    if (status_grid->count() == static_cast<int>(panels.size())) {
        needs_rebuild = false;
        for (int i = 0; i < status_grid->count(); ++i) {
            QWidget* widget = status_grid->itemAt(i)->widget();
            if (!widget || widget->property("panelName").toString().toStdString() != panels[i]) {
                needs_rebuild = true;
                break;
            }
        }
    }
    
    if (needs_rebuild) {
        // Clear the grid first
        QLayoutItem* item;
        while ((item = status_grid->takeAt(0)) != nullptr) {
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
    }

    int running_count = 0;
    int stopped_count = 0;
    
    int row = 0;
    int col = 0;
    const int max_cols = 3; // 3 columns as requested
    
    for (size_t i = 0; i < panels.size(); ++i) {
        const std::string& panel = panels[i];
        std::string config_path = Utils::get_conky_config_path(panel).string();
        std::string abs_config_path;
        try {
            abs_config_path = fs::absolute(fs::path(config_path)).string();
        } catch (...) {
            abs_config_path = config_path;
        }
        
        bool is_running = false;
        for (const auto& abs_running : running_configs) {
            if (abs_running == abs_config_path) {
                is_running = true;
                break;
            }
        }
        
        if (is_running) running_count++;
        else stopped_count++;
        
        if (!needs_rebuild) {
            QWidget* card = status_grid->itemAt(i)->widget();
            QLabel* icon = card->findChild<QLabel*>("iconLabel");
            if (icon) icon->setText(is_running ? "🔳" : "🔲");
            
            QLabel* status = card->findChild<QLabel*>("statusLabel");
            if (status) {
                status->setText(is_running ? "Running" : "Stopped");
                status->setStyleSheet(is_running ? "color: #3fb950; font-size: 11px; font-weight: bold;" : "color: #8b949e; font-size: 11px;");
            }
            
            QPushButton* start_btn = card->findChild<QPushButton*>("startBtn");
            if (start_btn) start_btn->setVisible(!is_running);
            
            QPushButton* stop_btn = card->findChild<QPushButton*>("stopBtn");
            if (stop_btn) stop_btn->setVisible(is_running);
            
            QPushButton* restart_btn = card->findChild<QPushButton*>("restartBtn");
            if (restart_btn) restart_btn->setVisible(is_running);
        } else {
            // Create Card Frame
            QFrame* card = new QFrame();
            card->setProperty("panelName", QString::fromStdString(panel));
            card->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
            card->setObjectName("panelCard");
            card->setStyleSheet(
                "QFrame#panelCard { "
                "  background-color: #2d333b; " // Charcoal dark gray
                "  border: 1px solid #444c56; " // Dark border
                "  border-radius: 8px; "
                "  padding: 6px; "
                "} "
                "QFrame#panelCard:hover { "
                "  border-color: #58a6ff; "     // Soft blue accent on hover
                "  background-color: #373e47; " // Slightly lighter charcoal on hover
                "}"
            );
            
            QVBoxLayout* card_layout = new QVBoxLayout(card);
            card_layout->setSpacing(4); // Reduced internal spacing
            
            // Header (Icon + Name)
            QHBoxLayout* header = new QHBoxLayout();
            QLabel* icon = new QLabel(is_running ? "🔳" : "🔲");
            icon->setObjectName("iconLabel");
            icon->setStyleSheet("font-size: 14px;"); // Smaller icon
            QLabel* name = new QLabel(QString::fromStdString(panel));
            name->setStyleSheet("font-weight: 700; font-size: 13px; color: #f0f6fc;"); // Soft white text for dark mode
            header->addWidget(icon);
            header->addWidget(name);
            header->addStretch();
            
            // Status indicator
            QLabel* status = new QLabel(is_running ? "Running" : "Stopped");
            status->setObjectName("statusLabel");
            status->setStyleSheet(is_running ? "color: #3fb950; font-size: 11px; font-weight: bold;" : "color: #8b949e; font-size: 11px;");
            
            // Buttons
            QHBoxLayout* btns = new QHBoxLayout();
            
            QPushButton* start_btn = new QPushButton("Start");
            start_btn->setObjectName("startBtn");
            start_btn->setStyleSheet("background-color: #1a7f37; color: white; border-radius: 4px; padding: 3px 8px; font-size: 11px;");
            QObject::connect(start_btn, &QPushButton::clicked, [panel, status_grid]() {
                ConkyManager::start_panel(panel);
                QTimer::singleShot(500, [status_grid]() { refresh_panel_status(nullptr, status_grid); });
            });
            
            QPushButton* stop_btn = new QPushButton("Stop");
            stop_btn->setObjectName("stopBtn");
            stop_btn->setStyleSheet("background-color: #cf222e; color: white; border-radius: 4px; padding: 3px 8px; font-size: 11px;");
            QObject::connect(stop_btn, &QPushButton::clicked, [panel, status_grid]() {
                ConkyManager::stop_panel(panel);
                QTimer::singleShot(500, [status_grid]() { refresh_panel_status(nullptr, status_grid); });
            });
            
            QPushButton* restart_btn = new QPushButton("🔄"); // Compact icon for restart
            restart_btn->setObjectName("restartBtn");
            restart_btn->setStyleSheet("background-color: #9a6700; color: white; border-radius: 4px; padding: 3px 8px; font-size: 11px;");
            restart_btn->setToolTip("Restart Panel");
            QObject::connect(restart_btn, &QPushButton::clicked, [panel]() {
                ConkyManager::reload_panel(panel);
            });
            
            start_btn->setVisible(!is_running);
            stop_btn->setVisible(is_running);
            restart_btn->setVisible(is_running);
            
            btns->addWidget(start_btn);
            btns->addWidget(stop_btn);
            btns->addWidget(restart_btn);
            btns->addStretch();
            
            card_layout->addLayout(header);
            card_layout->addWidget(status);
            card_layout->addLayout(btns);
            
            status_grid->addWidget(card, row, col);
            
            col++;
            if (col >= max_cols) {
                col = 0;
                row++;
            }
        }
    }
    
    // Update stats
    QLabel* running_label = main_window_instance->findChild<QLabel*>("runningLabel");
    QLabel* stopped_label = main_window_instance->findChild<QLabel*>("stoppedLabel");
    QLabel* total_label = main_window_instance->findChild<QLabel*>("totalLabel");
    
    if (running_label) running_label->setText(QString("Running: %1").arg(running_count));
    if (stopped_label) stopped_label->setText(QString("Stopped: %1").arg(stopped_count));
    if (total_label) total_label->setText(QString("Total: %1").arg(panels.size()));
    
    QLabel* refresh_label = main_window_instance->findChild<QLabel*>("refreshLabel");
    if (refresh_label) {
        QDateTime now = QDateTime::currentDateTime();
        refresh_label->setText(QString("Last Refresh: %1").arg(now.toString("hh:mm:ss")));
    }
}

void UIManager::start_all_panels() {
    try {
        ConkyManager::start_all_panels();
        show_tray_message("Panels", "Starting all panels...");
        
        // Refresh after a delay
        QTimer::singleShot(1500, []() {
            refresh_all_tabs();
        });
    } catch (const std::exception& e) {
        QMessageBox::warning(nullptr, "Error Starting Panels", 
            QString("Failed to start panels: %1").arg(e.what()));
    }
}

void UIManager::stop_all_panels() {
    try {
        ConkyManager::kill_all_conky();
        show_tray_message("Panels", "Stopping all panels...");
        
        // Refresh after a delay
        QTimer::singleShot(1000, []() {
            refresh_all_tabs();
        });
    } catch (const std::exception& e) {
        QMessageBox::warning(nullptr, "Error Stopping Panels", 
            QString("Failed to stop panels: %1").arg(e.what()));
    }
}

void UIManager::restart_active_panels() {
    try {
        ConkyManager::restart_active_panels();
        show_tray_message("Panels", "Restarting active panels...");
        
        // Refresh after a delay
        QTimer::singleShot(2000, []() {
            refresh_all_tabs();
        });
    } catch (const std::exception& e) {
        QMessageBox::warning(nullptr, "Error Restarting Panels", 
            QString("Failed to restart panels: %1").arg(e.what()));
    }
}

// Editor tab functionality
QWidget* UIManager::create_editor_tab(QWidget* parent) {
    QWidget* tab = new QWidget(parent);
    QVBoxLayout* main_layout = new QVBoxLayout(tab);
    main_layout->setSpacing(15);
    main_layout->setContentsMargins(20, 20, 20, 20);
    
    // Title
    QLabel* title = new QLabel("Config Editor");
    title->setStyleSheet("font-size: 18px; font-weight: bold; margin-bottom: 10px;");
    main_layout->addWidget(title);
    
    // Panel selector
    QGroupBox* selector_group = new QGroupBox("Select Panel");
    QHBoxLayout* selector_layout = new QHBoxLayout(selector_group);
    
    QLabel* panel_label = new QLabel("Panel:");
    QComboBox* panel_combo = new QComboBox();
    panel_combo->setObjectName("editorPanelCombo");
    panel_combo->setMinimumWidth(200);
    
    selector_layout->addWidget(panel_label);
    selector_layout->addWidget(panel_combo);
    selector_layout->addStretch();
    
    main_layout->addWidget(selector_group);
    
    // Config path display
    QGroupBox* config_group = new QGroupBox("Configuration File");
    QVBoxLayout* config_layout = new QVBoxLayout(config_group);
    
    QLabel* config_path_label = new QLabel("Config File: (Select a panel)");
    config_path_label->setObjectName("configPathLabel");
    config_path_label->setStyleSheet("padding: 10px; background-color: #f5f5f5; border-radius: 5px;");
    config_path_label->setWordWrap(true);
    
    config_layout->addWidget(config_path_label);
    main_layout->addWidget(config_group);
    
    // Available editors
    QGroupBox* editors_group = new QGroupBox("Available Editors");
    QGridLayout* editors_layout = new QGridLayout(editors_group);
    editors_layout->setSpacing(15);
    
    // Define ALL known editors to probe — only those found on the system will be shown
    struct Editor {
        std::string name;
        std::string command;
        std::string icon;
    };
    
    // Comprehensive candidate list (GUI + terminal editors)
    std::vector<Editor> candidates = {
        {"VS Code",     "code",          "💠"},
        {"VSCodium",    "codium",        "🔷"},
        {"Sublime",     "subl",          "📑"},
        {"Atom",        "atom",          "⚛️"},
        {"Brackets",    "brackets",      "📐"},
        {"Kate",        "kate",          "📝"},
        {"KWrite",      "kwrite",        "✏️"},
        {"Gedit",       "gedit",         "📄"},
        {"Mousepad",    "mousepad",      "🖱️"},
        {"Pluma",       "pluma",         "📋"},
        {"Xed",         "xed",           "📎"},
        {"Geany",       "geany",         "🔧"},
        {"Featherpad",  "featherpad",    "🪶"},
        {"Notepadqq",   "notepadqq",     "📓"},
        {"Neovim",      "nvim",          "🟩"},
        {"Vim",         "vim",           "⚙️"},
        {"Emacs",       "emacs",         "🐃"},
        {"Nano",        "nano",          "⌨️"},
        {"Micro",       "micro",         "🔬"},
        {"Ne",          "ne",            "📟"}
    };
    
    // Scan system: keep only editors actually installed
    std::vector<Editor> editors;
    for (const auto& candidate : candidates) {
        bool found = !QStandardPaths::findExecutable(QString::fromStdString(candidate.command)).isEmpty();
        if (found) {
            editors.push_back(candidate);
        }
    }
    
    // If nothing found, fall back to nano/vim as guaranteed built-ins
    if (editors.empty()) {
        editors = {{"Nano", "nano", "⌨️"}, {"Vim", "vim", "⚙️"}};
    }
    
    int row = 0;
    int col = 0;
    
    for (const auto& editor : editors) {
        QPushButton* editor_btn = new QPushButton();
        editor_btn->setMinimumSize(140, 100);
        editor_btn->setStyleSheet(
            "QPushButton { "
            "  border: 2px solid #ddd; "
            "  border-radius: 12px; "
            "  background-color: #ffffff; "
            "  padding: 10px; "
            "} "
            "QPushButton:hover { "
            "  border-color: #2196F3; "
            "  background-color: #f0f7ff; "
            "} "
            "QPushButton:pressed { "
            "  background-color: #e3f2fd; "
            "}"
        );
        
        QVBoxLayout* btn_layout = new QVBoxLayout(editor_btn);
        btn_layout->setContentsMargins(5, 5, 5, 5);
        btn_layout->setSpacing(8);
        
        QLabel* icon_label = new QLabel(QString::fromStdString(editor.icon));
        icon_label->setAlignment(Qt::AlignCenter);
        icon_label->setStyleSheet("font-size: 32px; background: transparent; border: none;");
        icon_label->setAttribute(Qt::WA_TransparentForMouseEvents);
        
        QLabel* name_label = new QLabel(QString::fromStdString(editor.name));
        name_label->setAlignment(Qt::AlignCenter);
        name_label->setStyleSheet("font-weight: bold; font-size: 13px; color: #333; background: transparent; border: none;");
        name_label->setAttribute(Qt::WA_TransparentForMouseEvents);
        
        btn_layout->addWidget(icon_label);
        btn_layout->addWidget(name_label);
        
        // Connect click handler
        QObject::connect(editor_btn, &QPushButton::clicked, [editor, panel_combo]() {
            QString panel = panel_combo->currentText();
            if (panel.isEmpty()) {
                QMessageBox::warning(nullptr, "No Panel Selected", "Please select a panel first.");
                return;
            }
            
            std::string config_path = Utils::get_conky_config_path(panel.toStdString()).string();
            open_editor(editor.command, config_path);
        });
        
        editors_layout->addWidget(editor_btn, row, col);
        
        col++;
        if (col >= 3) {
            col = 0;
            row++;
        }
    }
    
    main_layout->addWidget(editors_group);
    
    // In-App Editor
    QGroupBox* in_app_group = new QGroupBox("In-App Editor");
    QVBoxLayout* in_app_layout = new QVBoxLayout(in_app_group);
    
    QLabel* in_app_desc = new QLabel("Use the built-in editor with Conky syntax highlighting.");
    in_app_desc->setWordWrap(true);
    in_app_layout->addWidget(in_app_desc);
    
    QPushButton* in_app_btn = new QPushButton("Open in App Editor");
    in_app_btn->setStyleSheet(
        "QPushButton { "
        "  padding: 12px 24px; "
        "  background-color: #4CAF50; "
        "  color: white; "
        "  border: none; "
        "  border-radius: 6px; "
        "  font-weight: bold; "
        "  font-size: 14px; "
        "} "
        "QPushButton:hover { "
        "  background-color: #45a049; "
        "} "
        "QPushButton:pressed { "
        "  background-color: #3d8b40; "
        "}"
    );
    in_app_btn->setToolTip("Open the selected panel's config in the built-in editor");
    QObject::connect(in_app_btn, &QPushButton::clicked, [panel_combo]() {
        QString panel = panel_combo->currentText();
        if (panel.isEmpty()) {
            QMessageBox::warning(nullptr, "No Panel Selected", "Please select a panel first.");
            return;
        }
        
        std::string config_path = Utils::get_conky_config_path(panel.toStdString()).string();
        
        InAppEditor editor;
        editor.openFile(QString::fromStdString(config_path));
        editor.exec();
    });
    
    in_app_layout->addWidget(in_app_btn);
    main_layout->addWidget(in_app_group);
    
    // Custom editor
    QGroupBox* custom_group = new QGroupBox("Custom Editor");
    QHBoxLayout* custom_layout = new QHBoxLayout(custom_group);
    
    QLabel* custom_label = new QLabel("Command:");
    QLineEdit* custom_input = new QLineEdit();
    custom_input->setObjectName("customEditorInput");
    custom_input->setPlaceholderText("Enter custom editor command (e.g., kate, gedit)");
    
    QPushButton* custom_btn = new QPushButton("Open");
    custom_btn->setStyleSheet("QPushButton { padding: 8px 20px; }");
    QObject::connect(custom_btn, &QPushButton::clicked, [custom_input, panel_combo]() {
        QString panel = panel_combo->currentText();
        if (panel.isEmpty()) {
            QMessageBox::warning(nullptr, "No Panel Selected", "Please select a panel first.");
            return;
        }
        
        QString editor_cmd = custom_input->text().trimmed();
        if (editor_cmd.isEmpty()) {
            QMessageBox::warning(nullptr, "No Command", "Please enter a custom editor command.");
            return;
        }
        
        std::string config_path = Utils::get_conky_config_path(panel.toStdString()).string();
        open_editor(editor_cmd.toStdString(), config_path);
    });
    
    custom_layout->addWidget(custom_label);
    custom_layout->addWidget(custom_input);
    custom_layout->addWidget(custom_btn);
    
    main_layout->addWidget(custom_group);
    main_layout->addStretch();
    
    // Populate panels
    QTimer::singleShot(100, [panel_combo, config_path_label]() {
        refresh_editors(nullptr);
        
        auto panels = Utils::discover_panels();
        panel_combo->clear();
        for (const auto& panel : panels) {
            panel_combo->addItem(QString::fromStdString(panel));
        }
        
        // Connect panel selection to config path update
        QObject::connect(panel_combo, &QComboBox::currentTextChanged, [config_path_label](const QString& panel) {
            if (!panel.isEmpty()) {
                std::string config_path = Utils::get_conky_config_path(panel.toStdString()).string();
                config_path_label->setText(QString::fromStdString("Config File: " + config_path));
            } else {
                config_path_label->setText("Config File: (Select a panel)");
            }
        });
        
        // Trigger initial update
        if (panel_combo->count() > 0) {
            std::string config_path = Utils::get_conky_config_path(panel_combo->currentText().toStdString()).string();
            config_path_label->setText(QString::fromStdString("Config File: " + config_path));
        }
    });
    
    return tab;
}

void UIManager::refresh_editors(QGridLayout* editors_layout) {
    if (!main_window_instance) return;
    
    // Find all editor buttons
    auto buttons = main_window_instance->findChildren<QPushButton*>();
    
    struct EditorCheck {
        std::string command;
        std::string name;
    };
    
    std::vector<EditorCheck> checks = {
        {"code", "VS Code"},
        {"codium", "VSCodium"},
        {"subl", "Sublime"},
        {"nano", "Nano"},
        {"vim", "Vim"}
    };
    
    for (const auto& check : checks) {
        // Simple check if command exists
        bool exists = !QStandardPaths::findExecutable(QString::fromStdString(check.command)).isEmpty();
        
        // Find button for this editor by searching through its child labels
        for (auto btn : buttons) {
            auto labels = btn->findChildren<QLabel*>();
            for (auto label : labels) {
                if (label->text().toStdString() == check.name) {
                    btn->setEnabled(exists);
                    if (!exists) {
                        btn->setToolTip("Editor not found in PATH");
                        btn->setStyleSheet(btn->styleSheet() + " QPushButton { opacity: 0.5; background-color: #f0f0f0; }");
                    }
                    break;
                }
            }
        }
    }
}

void UIManager::open_editor(const std::string& command, const std::string& config_path) {
    // Check if file exists
    if (!Utils::file_exists(config_path)) {
        QMessageBox::warning(nullptr, "File Not Found", 
            QString::fromStdString("Configuration file not found:\n" + config_path));
        return;
    }
    
    // Build command
    QString full_command = QString::fromStdString(command + " " + config_path);
    
    // Start the editor process
    QProcess* process = new QProcess();
    QObject::connect(process,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        process, &QObject::deleteLater);
    QObject::connect(process, &QProcess::errorOccurred, [process, command](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            QMessageBox::warning(nullptr, "Editor Not Found",
                QString::fromStdString("Could not start editor: " + command + "\n\n"
                    "Make sure the editor is installed and available in your PATH."));
        }
        process->deleteLater();
    });
    
    // startCommand is Qt6 only — use start() with split args for Qt5 compatibility
    QStringList args = QProcess::splitCommand(full_command);
    QString program = args.takeFirst();
    process->start(program, args);
    
    if (process->waitForStarted(3000)) {
        show_tray_message("Editor", "Opened " + command + " for editing");
    }
}

// Theme tab functionality
QWidget* UIManager::create_theme_tab(QWidget* parent) {
    QWidget* tab = new QWidget(parent);
    QVBoxLayout* main_layout = new QVBoxLayout(tab);
    main_layout->setSpacing(15);
    main_layout->setContentsMargins(20, 20, 20, 20);
    
    // Title
    QLabel* title = new QLabel("Theme Management");
    title->setStyleSheet("font-size: 18px; font-weight: bold; margin-bottom: 10px;");
    main_layout->addWidget(title);
    
    // Lists and Preview Layout
    QHBoxLayout* top_layout = new QHBoxLayout();
    top_layout->setSpacing(15);
    
    // Category List
    QGroupBox* cat_group = new QGroupBox("Categories");
    QVBoxLayout* cat_layout = new QVBoxLayout(cat_group);
    QListWidget* cat_list = new QListWidget();
    cat_list->setObjectName("categoryList");
    cat_layout->addWidget(cat_list);
    top_layout->addWidget(cat_group, 1);
    
    // Theme List
    QGroupBox* theme_group = new QGroupBox("Themes");
    QVBoxLayout* theme_layout = new QVBoxLayout(theme_group);
    QListWidget* theme_list = new QListWidget();
    theme_list->setObjectName("themeList");
    theme_layout->addWidget(theme_list);
    top_layout->addWidget(theme_group, 1); // Equal width now 1:1
    
    main_layout->addLayout(top_layout, 1);
    
    // Preview Section
    QGroupBox* preview_group = new QGroupBox("Theme Preview");
    preview_group->setMinimumHeight(150);
    QVBoxLayout* preview_layout = new QVBoxLayout(preview_group);
    
    QWidget* swatches_container = new QWidget();
    swatches_container->setObjectName("previewSwatches");
    QHBoxLayout* swatches_layout = new QHBoxLayout(swatches_container);
    swatches_layout->setSpacing(20);
    
    auto create_swatch = [&](const QString& label) {
        QWidget* container = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        
        QFrame* swatch = new QFrame();
        swatch->setFixedSize(80, 80);
        swatch->setFrameStyle(QFrame::Box | QFrame::Plain);
        swatch->setLineWidth(1);
        swatch->setStyleSheet("background-color: #f0f0f0; border: 1px solid #ddd; border-radius: 5px;");
        
        QLabel* name = new QLabel(label);
        name->setAlignment(Qt::AlignCenter);
        name->setStyleSheet("font-size: 10px;");
        
        layout->addWidget(swatch);
        layout->addWidget(name);
        return swatch;
    };
    
    QFrame* p_swatch = create_swatch("Primary");
    QFrame* s_swatch = create_swatch("Secondary");
    QFrame* a1_swatch = create_swatch("Accent 1");
    QFrame* a2_swatch = create_swatch("Accent 2");
    
    p_swatch->setObjectName("primarySwatch");
    s_swatch->setObjectName("secondarySwatch");
    a1_swatch->setObjectName("accent1Swatch");
    a2_swatch->setObjectName("accent2Swatch");
    
    swatches_layout->addWidget(p_swatch);
    swatches_layout->addWidget(s_swatch);
    swatches_layout->addWidget(a1_swatch);
    swatches_layout->addWidget(a2_swatch);
    swatches_layout->addStretch();
    
    preview_layout->addWidget(swatches_container);
    main_layout->addWidget(preview_group);
    
    // Controls
    QHBoxLayout* controls_layout = new QHBoxLayout();
    
    QLabel* panel_label = new QLabel("Apply to Panel:");
    QComboBox* panel_selector = new QComboBox();
    panel_selector->setObjectName("applyPanelCombo");
    panel_selector->setMinimumWidth(150);
    
    QPushButton* apply_btn = new QPushButton("Apply to Selected");
    apply_btn->setStyleSheet("QPushButton { padding: 8px 15px; background-color: #2196F3; color: white; border: none; border-radius: 3px; font-weight: bold; }");
    
    QPushButton* apply_global_btn = new QPushButton("Apply Globally");
    apply_global_btn->setStyleSheet("QPushButton { padding: 8px 15px; background-color: #4CAF50; color: white; border: none; border-radius: 3px; font-weight: bold; }");
    
    QPushButton* refresh_btn = new QPushButton("Refresh Themes");
    
    controls_layout->addWidget(panel_label);
    controls_layout->addWidget(panel_selector);
    controls_layout->addSpacing(10);
    controls_layout->addWidget(apply_btn);
    controls_layout->addWidget(apply_global_btn);
    controls_layout->addStretch();
    controls_layout->addWidget(refresh_btn);
    
    main_layout->addLayout(controls_layout);
    
    // Event handling
    QObject::connect(cat_list, &QListWidget::currentTextChanged, [theme_list](const QString& category) {
        if (!category.isEmpty()) {
            load_themes_for_category(theme_list, category.toStdString());
        }
    });
    
    QObject::connect(theme_list, &QListWidget::currentTextChanged, [cat_list, swatches_container](const QString& theme) {
        if (!theme.isEmpty() && cat_list->currentItem()) {
            std::string category = cat_list->currentItem()->text().toStdString();
            auto colors = ThemeManager::get_theme_colors(theme.toStdString(), category);
            update_theme_preview(swatches_container, colors);
        }
    });
    
    QObject::connect(apply_btn, &QPushButton::clicked, [theme_list, cat_list, panel_selector]() {
        QString theme = theme_list->currentItem() ? theme_list->currentItem()->text() : "";
        QString panel = panel_selector->currentText();
        if (theme.isEmpty() || panel.isEmpty() || !cat_list->currentItem()) {
            QMessageBox::warning(nullptr, "Selection Required", "Please select a theme and a panel.");
            return;
        }
        
        try {
            std::string category = cat_list->currentItem()->text().toStdString();
if (ThemeManager::apply_theme_to_panel(theme.toStdString(), category, panel.toStdString())) {
    show_tray_message("Theme Applied", QString("Applied %1 to %2").arg(theme, panel).toStdString());

    // Update status bar theme label
    QLabel* theme_label = main_window_instance->findChild<QLabel*>("statusBarThemeLabel");
    if (theme_label) {
        theme_label->setText(QString("Theme: %1").arg(theme));
    }

    // --- THE CRITICAL ADDITION ---
    Utils::signal_all_conky_instances();
    // -----------------------------

    // Stop the panel first
    ConkyManager::stop_panel(panel.toStdString());
                
                // Wait for panel to fully stop before restarting (increased timeout for reliability)
                QTimer::singleShot(1500, [panel]() {
                    try {
                        // Verify panel is actually stopped before restarting
                        auto running = ConkyManager::get_running_configs(true);
                        std::string abs_path;
                        try {
                            abs_path = fs::absolute(Utils::get_conky_config_path(panel.toStdString())).string();
                        } catch (...) {
                            abs_path = Utils::get_conky_config_path(panel.toStdString()).string();
                        }
                        
                        bool still_running = false;
                        for (const auto& r : running) {
                            try {
                                if (fs::absolute(fs::path(r)).string() == abs_path) {
                                    still_running = true;
                                    break;
                                }
                            } catch (...) {}
                        }
                    
                        if (!still_running) {
                            // Panel is fully stopped, safe to restart
                            ConkyManager::start_panel(panel.toStdString());
                        } else {
                            // Panel still running, try force kill and retry
                            std::string kill_cmd = "pkill -9 -f \"conky -c.*" + Utils::get_conky_config_path(panel.toStdString()).filename().string() + "\"";
                            if (system(kill_cmd.c_str()) != 0) { /* handle error if necessary */ }
                        
                            // Wait a bit more and try starting
                            QTimer::singleShot(500, [panel]() {
                                try {
                                    ConkyManager::start_panel(panel.toStdString());
                                } catch (const std::exception& e) {
                                    QMessageBox::warning(nullptr, "Error Restarting Panel",
                                        QString("Failed to restart panel after theme change: %1").arg(e.what()));
                                }
                            });
                        }
                    } catch (const std::exception& e) {
                        QMessageBox::warning(nullptr, "Error Restarting Panel",
                            QString("Failed to restart panel after theme change: %1").arg(e.what()));
                    }
                });
            }
        } catch (const std::exception& e) {
            QMessageBox::warning(nullptr, "Error Applying Theme",
                QString("Failed to apply theme: %1").arg(e.what()));
        }
    });
    
    QObject::connect(apply_global_btn, &QPushButton::clicked, [theme_list, cat_list]() {
        QString theme = theme_list->currentItem() ? theme_list->currentItem()->text() : "";
        if (theme.isEmpty() || !cat_list->currentItem()) {
            QMessageBox::warning(nullptr, "Selection Required", "Please select a theme.");
            return;
        }
        
                std::string category = cat_list->currentItem()->text().toStdString();
if (ThemeManager::apply_global_theme(theme.toStdString(), category)) {
    show_tray_message("Theme Applied Globally", "Theme applied to all running panels.");

    // Update status bar theme label
    QLabel* theme_label = main_window_instance->findChild<QLabel*>("statusBarThemeLabel");
    if (theme_label) {
        theme_label->setText(QString("Theme: %1").arg(theme));
    }

    // --- THE CRITICAL ADDITION ---
    Utils::signal_all_conky_instances();
    // -----------------------------

    ConkyManager::restart_active_panels();
}
    });
    
    QObject::connect(refresh_btn, &QPushButton::clicked, [cat_list, theme_list, panel_selector]() {
        refresh_categories(cat_list, theme_list, true);
        refresh_panels(panel_selector);
    });
    
    // Initial data load — always force a fresh scan so that a second instance
    // of this tab (e.g. Panel Control > Themes) doesn't get a stale/empty cache
    // from the first tab's load.  We also manually fire load_themes_for_category
    // after selecting the first row because currentTextChanged won't emit for a
    // programmatic setCurrentRow when the list was just populated.
    QTimer::singleShot(200, [cat_list, theme_list, panel_selector]() {
        ThemeManager::invalidate_cache(); // ensure we re-scan from disk
        refresh_categories(cat_list, theme_list, false);
        refresh_panels(panel_selector);
        // Manually trigger theme load for the first category
        if (cat_list->count() > 0 && theme_list->count() == 0) {
            load_themes_for_category(theme_list, cat_list->item(0)->text().toStdString());
        }
    });
    
    return tab;
}

void UIManager::refresh_categories(QListWidget* category_list, QListWidget* theme_list, bool scan_metadata) {
    if (!category_list) return;
    
    auto categories = ThemeManager::load_categories(scan_metadata);
    category_list->clear();
    for (const auto& [name, themes] : categories) {
        category_list->addItem(QString::fromStdString(name));
    }
    
    if (category_list->count() > 0) {
        category_list->setCurrentRow(0);
        if (theme_list) {
            load_themes_for_category(theme_list, category_list->item(0)->text().toStdString());
        }
    }
}

void UIManager::load_themes_for_category(QListWidget* theme_list, const std::string& category_key) {
    if (!theme_list) return;
    
    auto themes = ThemeManager::get_themes_for_category(category_key);
    theme_list->clear();
    for (const auto& theme : themes) {
        theme_list->addItem(QString::fromStdString(theme));
    }
    
    if (theme_list->count() > 0) {
        theme_list->setCurrentRow(0);
    }
}

void UIManager::update_theme_preview(QWidget* preview_container, const std::vector<std::string>& colors) {
    if (!preview_container || colors.size() < 4) return;
    
    QFrame* p = preview_container->findChild<QFrame*>("primarySwatch");
    QFrame* s = preview_container->findChild<QFrame*>("secondarySwatch");
    QFrame* a1 = preview_container->findChild<QFrame*>("accent1Swatch");
    QFrame* a2 = preview_container->findChild<QFrame*>("accent2Swatch");
    
    auto set_swatch_color = [](QFrame* swatch, const std::string& color) {
        if (swatch) {
            swatch->setStyleSheet(QString("background-color: %1; border: 1px solid #ddd; border-radius: 5px;").arg(QString::fromStdString(color)));
        }
    };
    
    set_swatch_color(p, colors[0]);
    set_swatch_color(s, colors[1]);
    set_swatch_color(a1, colors[2]);
    set_swatch_color(a2, colors[3]);
}

void UIManager::apply_global_theme(const std::string& theme_name, const std::string& category_key) {
    ThemeManager::apply_global_theme(theme_name, category_key);
}

// Gap adjustment tab functionality
QWidget* UIManager::create_gap_tab(QWidget* parent) {
    QWidget* tab = new QWidget(parent);
    QVBoxLayout* main_layout = new QVBoxLayout(tab);
    main_layout->setSpacing(15);
    main_layout->setContentsMargins(20, 20, 20, 20);
    
    // Title
    QLabel* title = new QLabel("Gap Adjustment");
    title->setStyleSheet("font-size: 18px; font-weight: bold; margin-bottom: 10px;");
    main_layout->addWidget(title);
    
    // Panel selector
    QGroupBox* select_group = new QGroupBox("Select Panel");
    QHBoxLayout* select_layout = new QHBoxLayout(select_group);
    QComboBox* panel_combo = new QComboBox();
    panel_combo->setObjectName("gapPanelCombo");
    panel_combo->setMinimumWidth(200);
    select_layout->addWidget(new QLabel("Panel:"));
    select_layout->addWidget(panel_combo);
    select_layout->addStretch();
    main_layout->addWidget(select_group);
    
    // Adjustment section
    QGroupBox* adjust_group = new QGroupBox("Current Configuration");
    QGridLayout* adjust_layout = new QGridLayout(adjust_group);
    
    QSpinBox* gap_x_spin = new QSpinBox();
    gap_x_spin->setRange(-5000, 5000);
    gap_x_spin->setObjectName("gapXSpin");
    
    QSpinBox* gap_y_spin = new QSpinBox();
    gap_y_spin->setRange(-5000, 5000);
    gap_y_spin->setObjectName("gapYSpin");
    
    adjust_layout->addWidget(new QLabel("Gap X (Horizontal):"), 0, 0);
    adjust_layout->addWidget(gap_x_spin, 0, 1);
    adjust_layout->addWidget(new QLabel("Gap Y (Vertical):"), 1, 0);
    adjust_layout->addWidget(gap_y_spin, 1, 1);
    
    adjust_layout->addWidget(new QLabel("Step Size:"), 2, 0);
    QComboBox* step_combo = new QComboBox();
    step_combo->addItems({"1", "5", "10", "20", "50", "100"});
    step_combo->setCurrentText("10");
    adjust_layout->addWidget(step_combo, 2, 1);

    gap_x_spin->setSingleStep(10);
    gap_y_spin->setSingleStep(10);

    QObject::connect(step_combo, &QComboBox::currentTextChanged, [gap_x_spin, gap_y_spin](const QString& step) {
        int s = step.toInt();
        gap_x_spin->setSingleStep(s);
        gap_y_spin->setSingleStep(s);
    });

    QLabel* path_label = new QLabel("Config Path: (Select a panel)");
    path_label->setObjectName("gapPathLabel");
    path_label->setStyleSheet("color: #666; font-size: 11px;");
    path_label->setWordWrap(true);
    adjust_layout->addWidget(path_label, 3, 0, 1, 2);
    
    main_layout->addWidget(adjust_group);
    
    // Buttons
    QHBoxLayout* btn_layout = new QHBoxLayout();
    QPushButton* apply_btn = new QPushButton("Apply Changes");
    apply_btn->setStyleSheet("QPushButton { padding: 8px 20px; background-color: #2196F3; color: white; border: none; border-radius: 3px; font-weight: bold; }");
    
    QPushButton* refresh_btn = new QPushButton("Refresh");
    
    btn_layout->addWidget(apply_btn);
    btn_layout->addWidget(refresh_btn);
    btn_layout->addStretch();
    
    main_layout->addLayout(btn_layout);
    main_layout->addStretch();
    
    // Event handling
    QObject::connect(panel_combo, &QComboBox::currentTextChanged, [gap_x_spin, gap_y_spin, path_label](const QString& panel) {
        if (!panel.isEmpty()) {
            load_gap_values(gap_x_spin, gap_y_spin, panel.toStdString());
            std::string path = Utils::get_conky_config_path(panel.toStdString()).string();
            path_label->setText(QString::fromStdString("Config Path: " + path));
        }
    });
    
    QObject::connect(apply_btn, &QPushButton::clicked, [panel_combo, gap_x_spin, gap_y_spin]() {
        QString panel = panel_combo->currentText();
        if (panel.isEmpty()) return;
        
        apply_gap_changes(panel.toStdString(), gap_x_spin->value(), gap_y_spin->value());
        show_tray_message("Gap Adjusted", QString("Updated gap for %1").arg(panel).toStdString());
    });
    
    QObject::connect(refresh_btn, &QPushButton::clicked, [panel_combo, gap_x_spin, gap_y_spin]() {
        refresh_panels(panel_combo);
    });
    
    // Initial data load
    QTimer::singleShot(200, [panel_combo]() {
        refresh_panels(panel_combo);
    });
    
    return tab;
}

void UIManager::refresh_panels(QComboBox* panel_combo) {
    if (!panel_combo) return;
    
    auto panels = Utils::discover_panels();
    panel_combo->clear();
    for (const auto& panel : panels) {
        panel_combo->addItem(QString::fromStdString(panel));
    }
}

void UIManager::load_gap_values(QSpinBox* gap_x_spin, QSpinBox* gap_y_spin, const std::string& panel_name) {
    if (!gap_x_spin || !gap_y_spin) return;
    
    std::string path = Utils::get_conky_config_path(panel_name).string();
    int x = ConfigParser::get_gap_x(path);
    int y = ConfigParser::get_gap_y(path);
    
    gap_x_spin->setValue(x);
    gap_y_spin->setValue(y);
}

void UIManager::apply_gap_changes(const std::string& panel_name, int gap_x, int gap_y) {
    std::string path = Utils::get_conky_config_path(panel_name).string();

    // 1. Save the new gap values to the config file first
    if (!ConfigParser::set_gap_values(path, gap_x, gap_y)) {
        QMessageBox::warning(nullptr, "Gap Adjustment Failed",
            QString::fromStdString("Could not write gap values to:\n" + path));
        return;
    }

    // 2. Stop the panel
    ConkyManager::stop_panel(panel_name);

    // 3. Poll until the panel process is confirmed dead (max 3 seconds),
    //    then restart it with skip_check=true so we don't get fooled by
    //    a not-yet-dead ghost process reporting as still running.
    //    Each poll fires on the main thread via QTimer so QProcess stays safe.
    auto poll_and_start = std::make_shared<std::function<void(int)>>();
    *poll_and_start = [panel_name, poll_and_start](int attempts_left) {
        auto running = ConkyManager::get_running_configs(true);
        std::string abs_path;
        try {
            abs_path = fs::absolute(Utils::get_conky_config_path(panel_name)).string();
        } catch (...) {
            abs_path = Utils::get_conky_config_path(panel_name).string();
        }

        bool still_running = false;
        for (const auto& r : running) {
            try {
                if (fs::absolute(fs::path(r)).string() == abs_path) {
                    still_running = true;
                    break;
                }
            } catch (...) {}
        }

        if (!still_running) {
            // Panel is fully dead — safe to start with new gap values
            ConkyManager::start_panel(panel_name, true);
        } else if (attempts_left > 0) {
            // Not dead yet — check again in 200ms
            QTimer::singleShot(200, [panel_name, poll_and_start, attempts_left]() {
                (*poll_and_start)(attempts_left - 1);
            });
        } else {
            // Timed out — force start anyway
            ConkyManager::start_panel(panel_name, true);
        }
    };

    // Start polling after an initial 200ms pause
    QTimer::singleShot(200, [poll_and_start]() {
        (*poll_and_start)(14); // up to 14 x 200ms = ~3 seconds max wait
    });
}

// Theme creator tab
QWidget* UIManager::create_theme_creator_tab(QWidget* parent) {
    QWidget* tab = new QWidget(parent);
    QVBoxLayout* main_layout = new QVBoxLayout(tab);
    main_layout->setSpacing(15);
    main_layout->setContentsMargins(20, 20, 20, 20);
    
    QLabel* title = new QLabel("Theme Creator");
    title->setStyleSheet("font-size: 18px; font-weight: bold;");
    main_layout->addWidget(title);
    
    QTabWidget* mode_tabs = new QTabWidget();
    
    // Manual Entry
    QWidget* manual_page = new QWidget();
    QGridLayout* manual_layout = new QGridLayout(manual_page);
    
    QLineEdit* name_input = new QLineEdit();
    QLineEdit* cat_input = new QLineEdit();
    
    std::vector<QLineEdit*> color_inputs;
    for (int i = 0; i < 4; ++i) {
        QHBoxLayout* hl = new QHBoxLayout();
        QLineEdit* le = new QLineEdit("#FFFFFF");
        le->setMaximumWidth(100);
        QPushButton* pb = new QPushButton("🎨");
        pb->setMaximumWidth(40);
        QObject::connect(pb, &QPushButton::clicked, [le]() {
            std::string c = show_color_picker("Select Color", le->text().toStdString());
            if (!c.empty()) le->setText(QString::fromStdString(c));
        });
        hl->addWidget(le);
        hl->addWidget(pb);
        hl->addStretch();
        color_inputs.push_back(le);
        manual_layout->addLayout(hl, 2 + i, 1);
    }
    
    manual_layout->addWidget(new QLabel("Character Name:"), 0, 0);
    manual_layout->addWidget(name_input, 0, 1);
    manual_layout->addWidget(new QLabel("Category:"), 1, 0);
    manual_layout->addWidget(cat_input, 1, 1);
    manual_layout->addWidget(new QLabel("Primary:"), 2, 0);
    manual_layout->addWidget(new QLabel("Secondary:"), 3, 0);
    manual_layout->addWidget(new QLabel("Accent 1:"), 4, 0);
    manual_layout->addWidget(new QLabel("Accent 2:"), 5, 0);
    
    QPushButton* create_btn = new QPushButton("Create Theme");
    create_btn->setStyleSheet("padding: 10px; background-color: #4CAF50; color: white; font-weight: bold;");
    manual_layout->addWidget(create_btn, 6, 1);
    
    QObject::connect(create_btn, &QPushButton::clicked, [=]() {
        if (name_input->text().isEmpty() || cat_input->text().isEmpty()) {
            QMessageBox::warning(nullptr, "Missing Data", "Name and Category are required.");
            return;
        }
        std::vector<std::string> colors;
        for (auto le : color_inputs) colors.push_back(le->text().toStdString());
        
        if (ThemeManager::create_lua_theme(name_input->text().toStdString(), colors, Utils::themes_directory(), cat_input->text().toStdString())) {
            QMessageBox::information(nullptr, "Success", "Theme created successfully!");
            name_input->clear();
        }
    });
    
    mode_tabs->addTab(manual_page, "Manual Entry");
    
    // CSV Import
    QWidget* csv_page = new QWidget();
    QVBoxLayout* csv_layout = new QVBoxLayout(csv_page);
    
    QHBoxLayout* file_hl = new QHBoxLayout();
    QLineEdit* csv_path_le = new QLineEdit();
    QPushButton* browse_btn = new QPushButton("Browse");
    file_hl->addWidget(new QLabel("CSV File:"));
    file_hl->addWidget(csv_path_le);
    file_hl->addWidget(browse_btn);
    csv_layout->addLayout(file_hl);
    
    QLineEdit* csv_cat_le = new QLineEdit();
    QHBoxLayout* cat_hl = new QHBoxLayout();
    cat_hl->addWidget(new QLabel("Category:"));
    cat_hl->addWidget(csv_cat_le);
    csv_layout->addLayout(cat_hl);
    
    QPushButton* import_btn = new QPushButton("Import All from CSV");
    import_btn->setStyleSheet("padding: 10px; background-color: #2196F3; color: white; font-weight: bold;");
    csv_layout->addWidget(import_btn);
    csv_layout->addStretch();
    
    QObject::connect(browse_btn, &QPushButton::clicked, [csv_path_le]() {
        std::string p = show_file_dialog("Select CSV", "CSV Files (*.csv)");
        if (!p.empty()) csv_path_le->setText(QString::fromStdString(p));
    });
    
    QObject::connect(import_btn, &QPushButton::clicked, [=]() {
        if (csv_path_le->text().isEmpty() || csv_cat_le->text().isEmpty()) return;
        if (ThemeManager::sync_category_with_csv(csv_cat_le->text().toStdString(), csv_path_le->text().toStdString())) {
            QMessageBox::information(nullptr, "Import Complete", "Themes imported from CSV.");
        }
    });
    
    mode_tabs->addTab(csv_page, "CSV Import");
    
    // Direct CSV Paste
    QWidget* paste_page = new QWidget();
    QVBoxLayout* paste_layout = new QVBoxLayout(paste_page);

    QLabel* paste_label = new QLabel("Paste CSV data below (Format: Name, Primary, Secondary, Accent 1, Accent 2):");
    paste_layout->addWidget(paste_label);

    QTextEdit* paste_input = new QTextEdit();
    paste_input->setPlaceholderText("Example:\nWarcraft,#32CD32,#4B0082,#000000,#ADFF2F");
    paste_layout->addWidget(paste_input);

    QLineEdit* paste_cat_le = new QLineEdit();
    QHBoxLayout* p_cat_hl = new QHBoxLayout();
    p_cat_hl->addWidget(new QLabel("Category:"));
    p_cat_hl->addWidget(paste_cat_le);
    paste_layout->addLayout(p_cat_hl);

    QPushButton* paste_btn = new QPushButton("Process Paste Data");
    paste_btn->setStyleSheet("padding: 10px; background-color: #673AB7; color: white; font-weight: bold;");
    paste_layout->addWidget(paste_btn);

    QObject::connect(paste_btn, &QPushButton::clicked, [=]() {
        QString text = paste_input->toPlainText();
        QString category = paste_cat_le->text().trimmed();
        if (text.isEmpty() || category.isEmpty()) {
            QMessageBox::warning(nullptr, "Missing Data", "Please paste CSV data and specify a category.");
            return;
        }

        QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        int count = 0;
        for (const QString& line : lines) {
            // Skip header if present
            if (line.contains("Theme", Qt::CaseInsensitive) || line.contains("Primary", Qt::CaseInsensitive)) continue;

            QStringList parts = line.split(',');
            if (parts.size() >= 5) {
                std::string name = parts[0].trimmed().toStdString();
                std::vector<std::string> colors = {
                    parts[1].trimmed().toStdString(),
                    parts[2].trimmed().toStdString(),
                    parts[3].trimmed().toStdString(),
                    parts[4].trimmed().toStdString()
                };
                if (ThemeManager::create_lua_theme(name, colors, Utils::themes_directory(), category.toStdString())) {
                    count++;
                }
            }
        }
        QMessageBox::information(nullptr, "Import Complete", QString("Successfully created %1 themes in category '%2'.").arg(count).arg(category));
        paste_input->clear();
    });

    mode_tabs->addTab(paste_page, "Direct CSV Paste");

    main_layout->addWidget(mode_tabs);
    return tab;
}

void UIManager::preview_conversion(QWidget* dialog) {}
bool UIManager::convert_themes(QWidget* dialog) { return false; }
void UIManager::clear_theme_creator_fields(QWidget* dialog) {}

// Theme editor tab
QWidget* UIManager::create_theme_editor_tab(QWidget* parent) {
    QWidget* tab = new QWidget(parent);
    QVBoxLayout* main_layout = new QVBoxLayout(tab);
    main_layout->setSpacing(15);
    main_layout->setContentsMargins(20, 20, 20, 20);
    
    QLabel* title = new QLabel("Theme Editor");
    title->setStyleSheet("font-size: 18px; font-weight: bold;");
    main_layout->addWidget(title);
    
    QHBoxLayout* sel_hl = new QHBoxLayout();
    QComboBox* cat_combo = new QComboBox();
    QComboBox* theme_combo = new QComboBox();
    sel_hl->addWidget(new QLabel("Category:"));
    sel_hl->addWidget(cat_combo);
    sel_hl->addWidget(new QLabel("Theme:"));
    sel_hl->addWidget(theme_combo);
    main_layout->addLayout(sel_hl);
    
    QGroupBox* color_group = new QGroupBox("Color Editor");
    QGridLayout* color_layout = new QGridLayout(color_group);
    
    std::vector<QLineEdit*> color_les;
    for (int i = 0; i < 4; ++i) {
        QLineEdit* le = new QLineEdit();
        QPushButton* pb = new QPushButton("🎨");
        color_layout->addWidget(new QLabel(QString("Color %1:").arg(i+1)), i, 0);
        color_layout->addWidget(le, i, 1);
        color_layout->addWidget(pb, i, 2);
        color_les.push_back(le);
        
        QObject::connect(pb, &QPushButton::clicked, [le]() {
            std::string c = show_color_picker("Edit Color", le->text().toStdString());
            if (!c.empty()) le->setText(QString::fromStdString(c));
        });
    }
    
    main_layout->addWidget(color_group);
    
    QHBoxLayout* btn_hl = new QHBoxLayout();
    QPushButton* save_btn = new QPushButton("Save Changes");
    save_btn->setStyleSheet("background-color: #4CAF50; color: white; padding: 10px;");
    QPushButton* del_btn = new QPushButton("Delete Theme");
    del_btn->setStyleSheet("background-color: #f44336; color: white; padding: 10px;");
    
    btn_hl->addWidget(save_btn);
    btn_hl->addWidget(del_btn);
    main_layout->addLayout(btn_hl);
    main_layout->addStretch();
    
    // Logic
    auto refresh_cats = [=]() {
        auto cats = ThemeManager::load_categories();
        cat_combo->clear();
        for (auto const& [k, v] : cats) cat_combo->addItem(QString::fromStdString(k));
    };
    
    QObject::connect(cat_combo, &QComboBox::currentTextChanged, [=](const QString& cat) {
        theme_combo->clear();
        if (cat.isEmpty()) return;
        auto themes = ThemeManager::get_themes_for_category(cat.toStdString());
        for (auto const& t : themes) theme_combo->addItem(QString::fromStdString(t));
    });
    
    QObject::connect(theme_combo, &QComboBox::currentTextChanged, [=](const QString& theme) {
        if (theme.isEmpty()) return;
        auto colors = ThemeManager::get_theme_colors(theme.toStdString(), cat_combo->currentText().toStdString());
        for (size_t i = 0; i < colors.size() && i < color_les.size(); ++i) {
            color_les[i]->setText(QString::fromStdString(colors[i]));
        }
    });
    
    QObject::connect(save_btn, &QPushButton::clicked, [=]() {
        std::vector<std::string> colors;
        for (auto le : color_les) colors.push_back(le->text().toStdString());
        if (save_theme(cat_combo->currentText().toStdString(), theme_combo->currentText().toStdString(), colors)) {
            QMessageBox::information(nullptr, "Saved", "Theme updated.");
        }
    });
    
    QObject::connect(del_btn, &QPushButton::clicked, [=]() {
        if (QMessageBox::question(nullptr, "Confirm", "Delete this theme?") == QMessageBox::Yes) {
            delete_theme(cat_combo->currentText().toStdString(), theme_combo->currentText().toStdString());
            refresh_cats();
        }
    });
    
    QTimer::singleShot(500, refresh_cats);
    
    return tab;
}

void UIManager::load_theme_colors(QComboBox* category_combo, QComboBox* theme_combo, std::vector<QLineEdit*> color_inputs) {}

bool UIManager::save_theme(const std::string& category_key, const std::string& theme_name, const std::vector<std::string>& colors) {
    fs::path theme_path = ThemeManager::get_theme_file_path(theme_name, category_key);
    return ConfigParser::update_theme_colors(theme_path, colors);
}

void UIManager::delete_theme(const std::string& category_key, const std::string& theme_name) {
    ThemeManager::delete_theme(theme_name, category_key);
}

// Theme manager tab
QWidget* UIManager::create_theme_manager_tab(QWidget* parent) {
    QWidget* tab = new QWidget(parent);
    QVBoxLayout* main_layout = new QVBoxLayout(tab);
    
    QLabel* title = new QLabel("Theme Manager Operations");
    title->setStyleSheet("font-size: 18px; font-weight: bold; margin: 10px;");
    main_layout->addWidget(title);
    
    QGroupBox* move_group = new QGroupBox("Move Theme");
    QGridLayout* move_layout = new QGridLayout(move_group);
    QComboBox* m_src_cat = new QComboBox();
    QComboBox* m_target_cat = new QComboBox();
    QComboBox* m_theme = new QComboBox();
    QPushButton* m_btn = new QPushButton("Move");
    
    move_layout->addWidget(new QLabel("Source Category:"), 0, 0);
    move_layout->addWidget(m_src_cat, 0, 1);
    move_layout->addWidget(new QLabel("Theme:"), 1, 0);
    move_layout->addWidget(m_theme, 1, 1);
    move_layout->addWidget(new QLabel("Target Category:"), 2, 0);
    move_layout->addWidget(m_target_cat, 2, 1);
    move_layout->addWidget(m_btn, 3, 1);
    main_layout->addWidget(move_group);
    
    QGroupBox* csv_group = new QGroupBox("CSV Operations");
    QHBoxLayout* csv_hl = new QHBoxLayout(csv_group);
    QPushButton* export_btn = new QPushButton("Export Category to CSV");
    csv_hl->addWidget(export_btn);
    main_layout->addWidget(csv_group);
    
    main_layout->addStretch();
    
    auto refresh = [=]() {
        auto cats = ThemeManager::load_categories();
        m_src_cat->clear(); m_target_cat->clear();
        for (auto const& [k, v] : cats) {
            m_src_cat->addItem(QString::fromStdString(k));
            m_target_cat->addItem(QString::fromStdString(k));
        }
    };
    
    QObject::connect(m_src_cat, &QComboBox::currentTextChanged, [=](const QString& cat) {
        m_theme->clear();
        if (cat.isEmpty()) return;
        auto themes = ThemeManager::get_themes_for_category(cat.toStdString());
        for (auto const& t : themes) m_theme->addItem(QString::fromStdString(t));
    });
    
    QObject::connect(m_btn, &QPushButton::clicked, [=]() {
        if (ThemeManager::move_theme(m_theme->currentText().toStdString(), m_src_cat->currentText().toStdString(), m_target_cat->currentText().toStdString())) {
            QMessageBox::information(nullptr, "Moved", "Theme moved successfully.");
            refresh();
        }
    });
    
    QObject::connect(export_btn, &QPushButton::clicked, [=]() {
        std::string cat = m_src_cat->currentText().toStdString();
        std::string path = show_file_dialog("Export CSV", "CSV Files (*.csv)", true);
        if (!path.empty()) {
            if (ThemeManager::export_category_to_csv(cat, fs::path(path))) {
                QMessageBox::information(nullptr, "Exported", "Category exported to CSV.");
            }
        }
    });
    
    QTimer::singleShot(500, refresh);
    return tab;
}

void UIManager::move_themes(const std::vector<std::string>& theme_names, const std::string& source_category, const std::string& target_category) {}
void UIManager::delete_themes(const std::vector<std::string>& theme_names, const std::string& category) {}
void UIManager::sync_category_with_csv(const std::string& category, const std::string& csv_path) {}
void UIManager::export_category_to_csv(const std::string& category, const std::string& csv_path) {}

// System tray
void UIManager::setup_system_tray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    
    tray_icon_instance = new QSystemTrayIcon();
    
    // Set icon
    QIcon tray_icon(":/icons/conky-unified-center-icon.png");
    if (tray_icon.isNull()) {
        // Fallback to a simple icon
        QPixmap pixmap(64, 64);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setPen(QPen(Qt::blue, 3));
        painter.setBrush(Qt::white);
        painter.drawEllipse(10, 10, 44, 44);
        painter.drawText(10, 10, 44, 44, Qt::AlignCenter, "C");
        tray_icon = QIcon(pixmap);
    }
    
    tray_icon_instance->setIcon(tray_icon);
    tray_icon_instance->setToolTip(QString::fromUtf8(AppInfo::kDisplayName()));
    
    // Create tray menu
    QMenu* tray_menu = new QMenu();
    
    QAction* show_action = tray_menu->addAction("Show Control Center");
    QObject::connect(show_action, &QAction::triggered, []() {
        show_main_window();
    });
    
    tray_menu->addSeparator();
    
    QAction* start_all_action = tray_menu->addAction("Start All Panels");
    QObject::connect(start_all_action, &QAction::triggered, []() {
        start_all_panels();
    });
    
    QAction* stop_all_action = tray_menu->addAction("Stop All Panels");
    QObject::connect(stop_all_action, &QAction::triggered, []() {
        stop_all_panels();
    });
    
    QAction* restart_action = tray_menu->addAction("Restart Active Panels");
    QObject::connect(restart_action, &QAction::triggered, []() {
        restart_active_panels();
    });
    
    tray_menu->addSeparator();

    QAction* preferences_action = tray_menu->addAction("Preferences...");
    QObject::connect(preferences_action, &QAction::triggered, []() {
        PreferencesDialog dialog(main_window_instance);
        if (dialog.exec() == QDialog::Accepted) {
            refresh_all_tabs();
        }
    });

    tray_menu->addSeparator();

    QAction* quit_action = tray_menu->addAction("Exit");
    QObject::connect(quit_action, &QAction::triggered, []() {
        quit_application();
    });
    
    tray_icon_instance->setContextMenu(tray_menu);
    
    // Show tray icon
    tray_icon_instance->show();
    
    // Connect double-click to show window
    QObject::connect(tray_icon_instance, &QSystemTrayIcon::activated, [](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            show_main_window();
        }
    });
}

void UIManager::show_tray_message(const std::string& title, const std::string& message) {
    if (tray_icon_instance && tray_icon_instance->isVisible()) {
        tray_icon_instance->showMessage(
            QString::fromStdString(title),
            QString::fromStdString(message),
            QSystemTrayIcon::Information,
            3000
        );
    }
}

// Status bar
QWidget* UIManager::create_status_bar(QWidget* parent) {
    QFrame* status_bar = new QFrame(parent);
    status_bar->setFrameStyle(QFrame::StyledPanel);
    status_bar->setStyleSheet("QFrame { background-color: #f0f0f0; padding: 5px; }");
    
    QHBoxLayout* layout = new QHBoxLayout(status_bar);
    layout->setContentsMargins(10, 5, 10, 5);
    
    QLabel* status_label = new QLabel("Ready");
    status_label->setObjectName("statusBarLabel");
    
    layout->addWidget(status_label);
    layout->addStretch();
    
    return status_bar;
}

void UIManager::update_status_bar(QLabel* status_label, double seconds_since_refresh) {
    if (status_label) {
        if (seconds_since_refresh < 0) {
            status_label->setText("Ready");
        } else {
            status_label->setText(QString("Last refresh: %1 seconds ago").arg(seconds_since_refresh, 0, 'f', 1));
        }
    }
}

// Dialogs and messages
int UIManager::show_message_box(const std::string& title, const std::string& message, int buttons) {
    QMessageBox::StandardButton result = QMessageBox::information(
        nullptr,
        QString::fromStdString(title),
        QString::fromStdString(message),
        static_cast<QMessageBox::StandardButton>(buttons)
    );
    return static_cast<int>(result);
}

std::string UIManager::show_input_dialog(const std::string& title, const std::string& label) {
    bool ok;
    QString text = QInputDialog::getText(nullptr, 
        QString::fromStdString(title),
        QString::fromStdString(label),
        QLineEdit::Normal,
        "",
        &ok);
    
    return ok ? text.toStdString() : "";
}

std::string UIManager::show_file_dialog(const std::string& title, const std::string& filter, bool save) {
    QString file_path;
    
    if (save) {
        file_path = QFileDialog::getSaveFileName(nullptr,
            QString::fromStdString(title),
            QString(),
            QString::fromStdString(filter));
    } else {
        file_path = QFileDialog::getOpenFileName(nullptr,
            QString::fromStdString(title),
            QString(),
            QString::fromStdString(filter));
    }
    
    return file_path.toStdString();
}

std::string UIManager::show_color_picker(const std::string& title, const std::string& initial_color) {
    QColor initial = QColor(QString::fromStdString(initial_color));
    if (!initial.isValid()) {
        initial = Qt::white;
    }
    
    QColor color = QColorDialog::getColor(initial, nullptr, QString::fromStdString(title));
    
    return color.isValid() ? color.name().toStdString() : "";
}

// Utility functions
void UIManager::set_widget_enabled(QWidget* widget, bool enabled) {
    if (widget) {
        widget->setEnabled(enabled);
    }
}

void UIManager::set_widget_visible(QWidget* widget, bool visible) {
    if (widget) {
        widget->setVisible(visible);
    }
}

std::string UIManager::get_widget_text(QWidget* widget) {
    if (auto label = qobject_cast<QLabel*>(widget)) {
        return label->text().toStdString();
    } else if (auto line_edit = qobject_cast<QLineEdit*>(widget)) {
        return line_edit->text().toStdString();
    } else if (auto text_edit = qobject_cast<QTextEdit*>(widget)) {
        return text_edit->toPlainText().toStdString();
    }
    return "";
}

void UIManager::set_widget_text(QWidget* widget, const std::string& text) {
    if (auto label = qobject_cast<QLabel*>(widget)) {
        label->setText(QString::fromStdString(text));
    } else if (auto line_edit = qobject_cast<QLineEdit*>(widget)) {
        line_edit->setText(QString::fromStdString(text));
    } else if (auto text_edit = qobject_cast<QTextEdit*>(widget)) {
        text_edit->setPlainText(QString::fromStdString(text));
    }
}

void UIManager::connect_signal(QWidget* sender, const std::string& signal, std::function<void()> slot) {
    // This is a simplified implementation
    // In a real implementation, you would use Qt's signal/slot mechanism
    if (auto button = qobject_cast<QPushButton*>(sender)) {
        QObject::connect(button, &QPushButton::clicked, slot);
    }
}

// Qt-style interface implementations
void UIManager::createMainWindow() {
    create_main_window();
}

void UIManager::createThemeTab() {
    // Already handled in create_tab_widget
}

void UIManager::createGapTab() {
    // Already handled in create_tab_widget
}

void UIManager::createControlTab() {
    // Already handled in create_tab_widget
}

void UIManager::updateStatus() {
    // Update status bar with current information
    if (main_window_instance) {
        QLabel* status_label = main_window_instance->findChild<QLabel*>("statusBarLabel");
        if (status_label) {
            double seconds = ConkyManager::seconds_since_refresh();
            update_status_bar(status_label, seconds);
        }
    }
}

void UIManager::showTrayMessage(const std::string& message) {
    show_tray_message(AppInfo::kDisplayName(), message);
}

// Private helper functions
void UIManager::initialize_qt_application(int argc, char* argv[]) {
    initialize_application(argc, argv);
}

QWidget* UIManager::create_control_center_ui() {
    return create_main_window();
}

QWidget* UIManager::create_theme_center_ui() {
    // Could create a separate theme center window
    return nullptr;
}

void UIManager::setup_mode_switching() {
    // Could implement mode switching between control center and theme center
}

void UIManager::refresh_all_tabs() {
    if (!main_window_instance) return;
    
    // Refresh Panel Status
    QGridLayout* status_grid = main_window_instance->findChild<QGridLayout*>("statusGrid");
    if (status_grid) {
        refresh_panel_status(nullptr, status_grid);
    }
    
    // Refresh Themes
    QListWidget* cat_list = main_window_instance->findChild<QListWidget*>("categoryList");
    QListWidget* theme_list = main_window_instance->findChild<QListWidget*>("themeList");
    QComboBox* apply_panel_combo = main_window_instance->findChild<QComboBox*>("applyPanelCombo");
    if (cat_list) {
        ThemeManager::invalidate_cache(); // ensure we re-scan from disk
        refresh_categories(cat_list, theme_list, false);
    }
    if (apply_panel_combo) {
        refresh_panels(apply_panel_combo);
    }
    
    // Refresh Gaps
    QComboBox* gap_panel_combo = main_window_instance->findChild<QComboBox*>("gapPanelCombo");
    if (gap_panel_combo) {
        refresh_panels(gap_panel_combo);
        // Also trigger reload of current panel values
        QString current = gap_panel_combo->currentText();
        if (!current.isEmpty()) {
            QSpinBox* x = main_window_instance->findChild<QSpinBox*>("gapXSpin");
            QSpinBox* y = main_window_instance->findChild<QSpinBox*>("gapYSpin");
            if (x && y) load_gap_values(x, y, current.toStdString());
        }
    }
    
    // Refresh Editor Tab
    QComboBox* editor_panel_combo = main_window_instance->findChild<QComboBox*>("editorPanelCombo");
    if (editor_panel_combo) {
        auto panels = Utils::discover_panels();
        editor_panel_combo->clear();
        for (const auto& p : panels) editor_panel_combo->addItem(QString::fromStdString(p));
        refresh_editors(nullptr);
    }
}