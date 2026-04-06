#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QGroupBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QApplication>

class ErrorReportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ErrorReportDialog(QWidget* parent = nullptr);
    
    // Show error report dialog with pre-filled information
    static void show_report_dialog(QWidget* parent, 
                                   const QString& error_message,
                                   const QString& error_details = "",
                                   const QString& component = "");

private slots:
    void open_github_issue();
    void copy_issue_template();
    void export_logs();
    void accept() override;

private:
    void setup_ui(const QString& error_message, const QString& error_details, const QString& component);
    QString generate_issue_template(const QString& error_message, const QString& error_details, const QString& component);
    QString get_system_info();
    QString get_log_excerpts();
    
    QLineEdit* title_edit_;
    QTextEdit* steps_edit_;
    QTextEdit* expected_edit_;
    QTextEdit* actual_edit_;
    QTextEdit* template_preview_;
    QCheckBox* include_logs_checkbox_;
    QPushButton* copy_button_;
    QPushButton* github_button_;
    QPushButton* export_button_;
    
    QString error_message_;
    QString error_details_;
    QString component_;
};