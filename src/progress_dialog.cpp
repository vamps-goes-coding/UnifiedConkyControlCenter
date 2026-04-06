#include "progress_dialog.h"
#include "logger.h"

#include <QApplication>

ProgressDialog* ProgressDialog::instance_ = nullptr;
bool ProgressDialog::cancelled_ = false;

ProgressDialog::ProgressDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Progress");
    setMinimumWidth(400);
    setMinimumHeight(200);
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
}

void ProgressDialog::show_progress(QWidget* parent, const QString& title, const QString& message) {
    if (instance_) {
        instance_->close();
        delete instance_;
    }
    
    instance_ = new ProgressDialog(parent);
    instance_->setup_ui(title, message);
    instance_->show();
    instance_->raise();
    instance_->activateWindow();
    
    cancelled_ = false;
    LOG_INFO("Progress dialog shown: " + message.toStdString());
}

void ProgressDialog::update_progress(int value, const QString& message) {
    if (!instance_ || !instance_->isVisible()) {
        return;
    }
    
    instance_->progress_bar_->setValue(value);
    
    if (!message.isEmpty()) {
        instance_->status_label_->setText(message);
    }
    
    QApplication::processEvents();
}

void ProgressDialog::close_progress() {
    if (instance_) {
        instance_->close();
        delete instance_;
        instance_ = nullptr;
    }
}

bool ProgressDialog::is_showing() {
    return instance_ && instance_->isVisible();
}

void ProgressDialog::setup_ui(const QString& title, const QString& message) {
    setWindowTitle(title);
    
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(15);
    main_layout->setContentsMargins(20, 20, 20, 20);
    
    // Message
    message_label_ = new QLabel(message);
    message_label_->setWordWrap(true);
    message_label_->setStyleSheet("font-size: 14px;");
    main_layout->addWidget(message_label_);
    
    // Progress bar
    progress_bar_ = new QProgressBar();
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    progress_bar_->setTextVisible(true);
    progress_bar_->setStyleSheet(
        "QProgressBar { "
        "  border: 2px solid #ddd; "
        "  border-radius: 5px; "
        "  text-align: center; "
        "  height: 25px; "
        "} "
        "QProgressBar::chunk { "
        "  background-color: #4CAF50; "
        "  border-radius: 3px; "
        "}"
    );
    main_layout->addWidget(progress_bar_);
    
    // Status label
    status_label_ = new QLabel("Initializing...");
    status_label_->setStyleSheet("color: #666; font-size: 12px;");
    main_layout->addWidget(status_label_);
    
    // Details text (hidden by default)
    details_text_ = new QTextEdit();
    details_text_->setReadOnly(true);
    details_text_->setMaximumHeight(100);
    details_text_->hide();
    main_layout->addWidget(details_text_);
    
    // Cancel button
    QHBoxLayout* button_layout = new QHBoxLayout();
    button_layout->addStretch();
    
    cancel_button_ = new QPushButton("Cancel");
    cancel_button_->setStyleSheet(
        "QPushButton { "
        "  background-color: #f44336; "
        "  color: white; "
        "  border: none; "
        "  padding: 8px 20px; "
        "  border-radius: 4px; "
        "} "
        "QPushButton:hover { "
        "  background-color: #da190b; "
        "}"
    );
    connect(cancel_button_, &QPushButton::clicked, this, &ProgressDialog::cancel_operation);
    button_layout->addWidget(cancel_button_);
    
    main_layout->addLayout(button_layout);
}

void ProgressDialog::cancel_operation() {
    cancelled_ = true;
    LOG_INFO("Operation cancelled by user");
    close();
}