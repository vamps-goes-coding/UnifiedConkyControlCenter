#include "error_dialog.h"
#include "logger.h"

#include <QStyle>
#include <QMessageBox>
#include <QTimer>

ErrorDialog::ErrorDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Error");
    setMinimumWidth(500);
    setMinimumHeight(300);
}

void ErrorDialog::show_info(QWidget* parent, const QString& title, const QString& message) {
    ErrorDialog dialog(parent);
    dialog.setup_ui(ErrorSeverity::Info, title, message);
    dialog.exec();
}

void ErrorDialog::show_warning(QWidget* parent, const QString& title, const QString& message) {
    ErrorDialog dialog(parent);
    dialog.setup_ui(ErrorSeverity::Warning, title, message);
    dialog.exec();
}

void ErrorDialog::show_error(QWidget* parent, const QString& title, const QString& message, const QString& details) {
    ErrorDialog dialog(parent);
    dialog.setup_ui(ErrorSeverity::Error, title, message, details);
    dialog.exec();
}

void ErrorDialog::show_critical(QWidget* parent, const QString& title, const QString& message, const QString& details) {
    ErrorDialog dialog(parent);
    dialog.setup_ui(ErrorSeverity::Critical, title, message, details);
    dialog.exec();
}

void ErrorDialog::show_error_with_suggestions(QWidget* parent, const QString& title, 
                                               const QString& message, 
                                               const QStringList& suggestions,
                                               const QString& details) {
    ErrorDialog dialog(parent);
    dialog.setup_ui(ErrorSeverity::Error, title, message, details, suggestions);
    dialog.exec();
}

void ErrorDialog::setup_ui(ErrorSeverity severity, const QString& title, const QString& message, 
                           const QString& details, const QStringList& suggestions) {
    setWindowTitle(title);
    
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(15);
    main_layout->setContentsMargins(20, 20, 20, 20);
    
    // Header with icon and message
    QHBoxLayout* header_layout = new QHBoxLayout();
    
    // Icon
    icon_label_ = new QLabel();
    icon_label_->setFixedSize(48, 48);
    icon_label_->setAlignment(Qt::AlignCenter);
    
    QString icon_name;
    switch (severity) {
        case ErrorSeverity::Info:
            icon_name = "dialog-information";
            break;
        case ErrorSeverity::Warning:
            icon_name = "dialog-warning";
            break;
        case ErrorSeverity::Error:
        case ErrorSeverity::Critical:
            icon_name = "dialog-error";
            break;
    }
    
    QIcon icon = QIcon::fromTheme(icon_name);
    if (icon.isNull()) {
        // Fallback to standard icons
        if (severity == ErrorSeverity::Info) {
            icon_label_->setText("ℹ️");
        } else if (severity == ErrorSeverity::Warning) {
            icon_label_->setText("⚠️");
        } else {
            icon_label_->setText("❌");
        }
        icon_label_->setStyleSheet("font-size: 32px;");
    } else {
        icon_label_->setPixmap(icon.pixmap(48, 48));
    }
    
    header_layout->addWidget(icon_label_);
    
    // Message
    message_label_ = new QLabel(message);
    message_label_->setWordWrap(true);
    message_label_->setStyleSheet("font-size: 14px; color: #333;");
    header_layout->addWidget(message_label_, 1);
    
    main_layout->addLayout(header_layout);
    
    // Details section (if provided)
    if (!details.isEmpty()) {
        details_ = details;
        
        details_button_ = new QPushButton("Show Details");
        details_button_->setStyleSheet(
            "QPushButton { "
            "  background-color: #f0f0f0; "
            "  border: 1px solid #ccc; "
            "  padding: 8px 16px; "
            "  border-radius: 4px; "
            "} "
            "QPushButton:hover { "
            "  background-color: #e0e0e0; "
            "}"
        );
        connect(details_button_, &QPushButton::clicked, this, &ErrorDialog::show_details);
        
        main_layout->addWidget(details_button_);
        
        details_text_ = new QTextEdit();
        details_text_->setReadOnly(true);
        details_text_->setPlainText(details);
        details_text_->setMaximumHeight(150);
        details_text_->hide();  // Hidden by default
        
        main_layout->addWidget(details_text_);
    }
    
    // Suggestions section (if provided)
    if (!suggestions.isEmpty()) {
        QLabel* suggestions_label = new QLabel("Troubleshooting Suggestions:");
        suggestions_label->setStyleSheet("font-weight: bold; font-size: 13px;");
        main_layout->addWidget(suggestions_label);
        
        suggestions_text_content_ = suggestions.join("\n• ");
        suggestions_text_content_ = "• " + suggestions_text_content_;
        
        suggestions_text_ = new QTextEdit();
        suggestions_text_->setReadOnly(true);
        suggestions_text_->setPlainText(suggestions_text_content_);
        suggestions_text_->setMaximumHeight(120);
        suggestions_text_->setStyleSheet(
            "QTextEdit { "
            "  background-color: #f9f9f9; "
            "  border: 1px solid #ddd; "
            "  padding: 10px; "
            "}"
        );
        
        main_layout->addWidget(suggestions_text_);
    }
    
    // Buttons
    QHBoxLayout* button_layout = new QHBoxLayout();
    button_layout->addStretch();
    
    if (!details.isEmpty() || !suggestions.isEmpty()) {
        copy_button_ = new QPushButton("Copy Details");
        copy_button_->setStyleSheet(
            "QPushButton { "
            "  background-color: #2196F3; "
            "  color: white; "
            "  border: none; "
            "  padding: 8px 16px; "
            "  border-radius: 4px; "
            "} "
            "QPushButton:hover { "
            "  background-color: #1976D2; "
            "}"
        );
        connect(copy_button_, &QPushButton::clicked, this, &ErrorDialog::copy_details);
        button_layout->addWidget(copy_button_);
    }
    
    ok_button_ = new QPushButton("OK");
    ok_button_->setStyleSheet(
        "QPushButton { "
        "  background-color: #4CAF50; "
        "  color: white; "
        "  border: none; "
        "  padding: 8px 24px; "
        "  border-radius: 4px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { "
        "  background-color: #45a049; "
        "}"
    );
    connect(ok_button_, &QPushButton::clicked, this, &QDialog::accept);
    button_layout->addWidget(ok_button_);
    
    main_layout->addLayout(button_layout);
}

void ErrorDialog::show_details() {
    if (details_text_->isVisible()) {
        details_text_->hide();
        details_button_->setText("Show Details");
    } else {
        details_text_->show();
        details_button_->setText("Hide Details");
    }
}

void ErrorDialog::copy_details() {
    QString full_text;
    
    if (!details_.isEmpty()) {
        full_text += "Error Details:\n" + details_ + "\n\n";
    }
    
    if (!suggestions_text_content_.isEmpty()) {
        full_text += "Troubleshooting Suggestions:\n" + suggestions_text_content_;
    }
    
    if (!full_text.isEmpty()) {
        QApplication::clipboard()->setText(full_text);
        
        // Show feedback
        copy_button_->setText("Copied!");
        QTimer::singleShot(2000, [this]() {
            copy_button_->setText("Copy Details");
        });
        
        LOG_INFO("Error details copied to clipboard");
    }
}

void ErrorDialog::copy_suggestions() {
    if (!suggestions_text_content_.isEmpty()) {
        QApplication::clipboard()->setText(suggestions_text_content_);
        LOG_INFO("Troubleshooting suggestions copied to clipboard");
    }
}