#include "error_report_dialog.h"
#include "logger.h"
#include "app_info.h"
#include "config_manager.h"
#include "utils.h"

#include <QProcess>
#include <QSysInfo>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

ErrorReportDialog::ErrorReportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Report Issue");
    setMinimumWidth(600);
    setMinimumHeight(500);
}

void ErrorReportDialog::show_report_dialog(QWidget* parent, 
                                           const QString& error_message,
                                           const QString& error_details,
                                           const QString& component) {
    ErrorReportDialog dialog(parent);
    dialog.setup_ui(error_message, error_details, component);
    dialog.exec();
}

void ErrorReportDialog::setup_ui(const QString& error_message, const QString& error_details, const QString& component) {
    error_message_ = error_message;
    error_details_ = error_details;
    component_ = component;
    
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(15);
    main_layout->setContentsMargins(20, 20, 20, 20);
    
    // Header
    QLabel* header_label = new QLabel("Report an Issue");
    header_label->setStyleSheet("font-size: 18px; font-weight: bold; margin-bottom: 10px;");
    main_layout->addWidget(header_label);
    
    QLabel* desc_label = new QLabel(
        "Help us improve by reporting this issue. You can either:\n"
        "1. Copy the issue template and create a GitHub issue manually\n"
        "2. Click 'Open GitHub' to create an issue with pre-filled information"
    );
    desc_label->setWordWrap(true);
    main_layout->addWidget(desc_label);
    
    // Issue Title
    QGroupBox* title_group = new QGroupBox("Issue Title");
    QVBoxLayout* title_layout = new QVBoxLayout(title_group);
    title_edit_ = new QLineEdit();
    title_edit_->setPlaceholderText("Brief description of the issue");
    title_edit_->setText("Error: " + error_message.left(50));
    title_layout->addWidget(title_edit_);
    main_layout->addWidget(title_group);
    
    // Steps to Reproduce
    QGroupBox* steps_group = new QGroupBox("Steps to Reproduce");
    QVBoxLayout* steps_layout = new QVBoxLayout(steps_group);
    steps_edit_ = new QTextEdit();
    steps_edit_->setPlaceholderText("1. Open the application\n2. Click on...\n3. See error");
    steps_edit_->setMaximumHeight(80);
    steps_layout->addWidget(steps_edit_);
    main_layout->addWidget(steps_group);
    
    // Expected vs Actual
    QHBoxLayout* behavior_layout = new QHBoxLayout();
    
    QGroupBox* expected_group = new QGroupBox("Expected Behavior");
    QVBoxLayout* expected_layout = new QVBoxLayout(expected_group);
    expected_edit_ = new QTextEdit();
    expected_edit_->setPlaceholderText("What should have happened?");
    expected_edit_->setMaximumHeight(80);
    expected_layout->addWidget(expected_edit_);
    behavior_layout->addWidget(expected_group);
    
    QGroupBox* actual_group = new QGroupBox("Actual Behavior");
    QVBoxLayout* actual_layout = new QVBoxLayout(actual_group);
    actual_edit_ = new QTextEdit();
    actual_edit_->setPlaceholderText("What actually happened?");
    actual_edit_->setMaximumHeight(80);
    actual_layout->addWidget(actual_edit_);
    behavior_layout->addWidget(actual_group);
    
    main_layout->addLayout(behavior_layout);
    
    // Include logs checkbox
    include_logs_checkbox_ = new QCheckBox("Include log excerpts in report");
    include_logs_checkbox_->setChecked(true);
    main_layout->addWidget(include_logs_checkbox_);
    
    // Template Preview
    QGroupBox* preview_group = new QGroupBox("Issue Template Preview");
    QVBoxLayout* preview_layout = new QVBoxLayout(preview_group);
    template_preview_ = new QTextEdit();
    template_preview_->setReadOnly(true);
    template_preview_->setMinimumHeight(150);
    preview_layout->addWidget(template_preview_);
    main_layout->addWidget(preview_group);
    
    // Update preview
    auto update_preview = [this]() {
        QString template_text = generate_issue_template(
            title_edit_->text(),
            error_details_,
            component_
        );
        template_preview_->setPlainText(template_text);
    };
    
    connect(title_edit_, &QLineEdit::textChanged, update_preview);
    connect(steps_edit_, &QTextEdit::textChanged, update_preview);
    connect(expected_edit_, &QTextEdit::textChanged, update_preview);
    connect(actual_edit_, &QTextEdit::textChanged, update_preview);
    connect(include_logs_checkbox_, &QCheckBox::toggled, update_preview);
    
    // Initial preview
    update_preview();
    
    // Buttons
    QHBoxLayout* button_layout = new QHBoxLayout();
    button_layout->addStretch();
    
    copy_button_ = new QPushButton("Copy to Clipboard");
    copy_button_->setStyleSheet(
        "QPushButton { "
        "  background-color: #2196F3; "
        "  color: white; "
        "  border: none; "
        "  padding: 10px 20px; "
        "  border-radius: 4px; "
        "} "
        "QPushButton:hover { "
        "  background-color: #1976D2; "
        "}"
    );
    connect(copy_button_, &QPushButton::clicked, this, &ErrorReportDialog::copy_issue_template);
    button_layout->addWidget(copy_button_);
    
    export_button_ = new QPushButton("Export Logs");
    export_button_->setStyleSheet(
        "QPushButton { "
        "  background-color: #FF9800; "
        "  color: white; "
        "  border: none; "
        "  padding: 10px 20px; "
        "  border-radius: 4px; "
        "} "
        "QPushButton:hover { "
        "  background-color: #F57C00; "
        "}"
    );
    connect(export_button_, &QPushButton::clicked, this, &ErrorReportDialog::export_logs);
    button_layout->addWidget(export_button_);
    
    github_button_ = new QPushButton("Open GitHub");
    github_button_->setStyleSheet(
        "QPushButton { "
        "  background-color: #4CAF50; "
        "  color: white; "
        "  border: none; "
        "  padding: 10px 20px; "
        "  border-radius: 4px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { "
        "  background-color: #45a049; "
        "}"
    );
    connect(github_button_, &QPushButton::clicked, this, &ErrorReportDialog::open_github_issue);
    button_layout->addWidget(github_button_);
    
    QPushButton* cancel_button = new QPushButton("Cancel");
    cancel_button->setStyleSheet(
        "QPushButton { "
        "  background-color: #f0f0f0; "
        "  border: 1px solid #ccc; "
        "  padding: 10px 20px; "
        "  border-radius: 4px; "
        "} "
        "QPushButton:hover { "
        "  background-color: #e0e0e0; "
        "}"
    );
    connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
    button_layout->addWidget(cancel_button);
    
    main_layout->addLayout(button_layout);
}

QString ErrorReportDialog::generate_issue_template(const QString& error_message, const QString& error_details, const QString& component) {
    QString template_text;
    
    template_text += "## Bug Report\n\n";
    template_text += "**Error Message:**\n";
    template_text += "```\n" + error_message + "\n```\n\n";
    
    if (!error_details.isEmpty()) {
        template_text += "**Error Details:**\n";
        template_text += "```\n" + error_details + "\n```\n\n";
    }
    
    if (!component.isEmpty()) {
        template_text += "**Component:** " + component + "\n\n";
    }
    
    template_text += "**System Information:**\n";
    template_text += get_system_info() + "\n\n";
    
    template_text += "**Steps to Reproduce:**\n";
    QString steps = steps_edit_->toPlainText();
    if (steps.isEmpty()) {
        template_text += "1. [Describe step 1]\n2. [Describe step 2]\n3. [See error]\n";
    } else {
        template_text += steps + "\n";
    }
    template_text += "\n";
    
    template_text += "**Expected Behavior:**\n";
    QString expected = expected_edit_->toPlainText();
    if (expected.isEmpty()) {
        template_text += "[What should have happened?]\n";
    } else {
        template_text += expected + "\n";
    }
    template_text += "\n";
    
    template_text += "**Actual Behavior:**\n";
    QString actual = actual_edit_->toPlainText();
    if (actual.isEmpty()) {
        template_text += "[What actually happened?]\n";
    } else {
        template_text += actual + "\n";
    }
    template_text += "\n";
    
    if (include_logs_checkbox_->isChecked()) {
        template_text += "**Logs:**\n";
        template_text += "```\n" + get_log_excerpts() + "\n```\n";
    }
    
    return template_text;
}

QString ErrorReportDialog::get_system_info() {
    QString info;
    
    info += "- **OS:** " + QSysInfo::prettyProductName() + "\n";
    info += "- **Kernel:** " + QSysInfo::kernelType() + " " + QSysInfo::kernelVersion() + "\n";
    info += "- **App Version:** " + QString::fromStdString(AppInfo::get_version()) + "\n";
    
    // Get Qt version
    info += "- **Qt Version:** " + QString(QT_VERSION_STR) + "\n";
    
    // Get Conky version (if available)
    QProcess conky_process;
    conky_process.start("conky", {"--version"});
    conky_process.waitForFinished(2000);
    QString conky_output = conky_process.readAllStandardOutput();
    if (!conky_output.isEmpty()) {
        QStringList lines = conky_output.split("\n");
        if (!lines.isEmpty()) {
            info += "- **Conky Version:** " + lines.first().trimmed() + "\n";
        }
    }
    
    return info;
}

QString ErrorReportDialog::get_log_excerpts() {
    QString excerpts;
    
    // Get log file path
    fs::path log_file = Logger::instance().get_log_file_path();
    if (fs::exists(log_file)) {
        QString log_content = QString::fromStdString(Utils::read_file(log_file));
        
        // Get last 50 lines
        QStringList lines = log_content.split("\n");
        int start_line = qMax(0, lines.size() - 50);
        QStringList last_lines = lines.mid(start_line);
        
        excerpts = last_lines.join("\n");
    } else {
        excerpts = "No log file available";
    }
    
    return excerpts;
}

void ErrorReportDialog::open_github_issue() {
    // Default GitHub URL - can be configured in app_config.json
    QString github_url = "https://github.com/beastvamps/Unified-Conky-Control-Center/issues/new";
    
    // Try to get from config if available
    try {
        auto& config = ConfigManager::instance();
        // GitHub URL would be stored in application config if added
        // For now, use default
    } catch (...) {
        // Use default if config not available
    }
    
    // Create issue URL with pre-filled title
    QString issue_url = github_url + "?title=" + QUrl::toPercentEncoding(title_edit_->text());
    
    QDesktopServices::openUrl(QUrl(issue_url));
    
    LOG_INFO("Opened GitHub issue page");
}

void ErrorReportDialog::copy_issue_template() {
    QString template_text = template_preview_->toPlainText();
    QApplication::clipboard()->setText(template_text);
    
    copy_button_->setText("Copied!");
    QTimer::singleShot(2000, [this]() {
        copy_button_->setText("Copy to Clipboard");
    });
    
    LOG_INFO("Issue template copied to clipboard");
}

void ErrorReportDialog::export_logs() {
    QString export_dir = QFileDialog::getExistingDirectory(
        this,
        "Select Export Directory",
        QDir::homePath()
    );
    
    if (export_dir.isEmpty()) {
        return;
    }
    
    // Create export file
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString export_file = export_dir + "/conky_control_center_logs_" + timestamp + ".txt";
    
    QString export_content;
    export_content += "=== Unified Conky Control Center - Log Export ===\n";
    export_content += "Export Date: " + QDateTime::currentDateTime().toString(Qt::ISODate) + "\n\n";
    
    export_content += "=== System Information ===\n";
    export_content += get_system_info() + "\n\n";
    
    export_content += "=== Error Information ===\n";
    export_content += "Message: " + error_message_ + "\n";
    if (!error_details_.isEmpty()) {
        export_content += "Details: " + error_details_ + "\n";
    }
    if (!component_.isEmpty()) {
        export_content += "Component: " + component_ + "\n";
    }
    export_content += "\n";
    
    export_content += "=== Log Excerpts ===\n";
    export_content += get_log_excerpts() + "\n";
    
    // Write to file
    QFile file(export_file);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << export_content;
        file.close();
        
        QMessageBox::information(this, "Export Complete",
            "Logs exported to:\n" + export_file);
        
        LOG_INFO("Logs exported to: " + export_file.toStdString());
    } else {
        QMessageBox::warning(this, "Export Failed",
            "Failed to write log file to:\n" + export_file);
        LOG_ERROR("Failed to export logs to: " + export_file.toStdString());
    }
}

void ErrorReportDialog::accept() {
    QDialog::accept();
}