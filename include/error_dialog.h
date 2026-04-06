#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QIcon>
#include <QClipboard>
#include <QApplication>
#include <QScrollArea>

enum class ErrorSeverity {
    Info,
    Warning,
    Error,
    Critical
};

class ErrorDialog : public QDialog {
    Q_OBJECT

public:
    explicit ErrorDialog(QWidget* parent = nullptr);
    
    // Show different types of dialogs
    static void show_info(QWidget* parent, const QString& title, const QString& message);
    static void show_warning(QWidget* parent, const QString& title, const QString& message);
    static void show_error(QWidget* parent, const QString& title, const QString& message, const QString& details = "");
    static void show_critical(QWidget* parent, const QString& title, const QString& message, const QString& details = "");
    
    // Show error with troubleshooting suggestions
    static void show_error_with_suggestions(QWidget* parent, const QString& title, 
                                           const QString& message, 
                                           const QStringList& suggestions,
                                           const QString& details = "");

private slots:
    void copy_details();
    void copy_suggestions();
    void show_details();

private:
    void setup_ui(ErrorSeverity severity, const QString& title, const QString& message, 
                  const QString& details = "", const QStringList& suggestions = QStringList());
    
    QLabel* icon_label_;
    QLabel* message_label_;
    QTextEdit* details_text_;
    QTextEdit* suggestions_text_;
    QPushButton* details_button_;
    QPushButton* copy_button_;
    QPushButton* ok_button_;
    
    QString details_;
    QString suggestions_text_content_;
};