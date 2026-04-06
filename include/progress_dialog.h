#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>

class ProgressDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProgressDialog(QWidget* parent = nullptr);
    
    // Show progress dialog with title and message
    static void show_progress(QWidget* parent, const QString& title, const QString& message);
    
    // Update progress
    static void update_progress(int value, const QString& message = "");
    
    // Close progress dialog
    static void close_progress();
    
    // Check if progress dialog is visible
    static bool is_showing();

private slots:
    void cancel_operation();

private:
    void setup_ui(const QString& title, const QString& message);
    
    QLabel* message_label_;
    QProgressBar* progress_bar_;
    QLabel* status_label_;
    QPushButton* cancel_button_;
    QTextEdit* details_text_;
    
    static ProgressDialog* instance_;
    static bool cancelled_;
};