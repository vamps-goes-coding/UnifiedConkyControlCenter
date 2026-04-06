#include "in_app_editor.h"
#include "conky_syntax_highlighter.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QApplication>
#include <QClipboard>
#include <QTextCursor>
#include <QTextBlock>
#include <QInputDialog>
#include <QPushButton>

InAppEditor::InAppEditor(QWidget* parent)
    : QDialog(parent)
    , isUntitled_(true)
    , findDialog_(nullptr)
{
    setupUI();
    setupToolbar();
    setupConnections();
    updateTitle();
    updateStatusBar();
}

void InAppEditor::setupUI() {
    setWindowTitle("In-App Editor");
    setMinimumSize(800, 600);
    resize(1000, 700);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Text editor
    textEdit_ = new QTextEdit();
    textEdit_->setFont(QFont("Monospace", 10));
    textEdit_->setTabStopDistance(40);  // 4 spaces
    textEdit_->setLineWrapMode(QTextEdit::NoWrap);
    mainLayout->addWidget(textEdit_);
    
    // Syntax highlighter
    highlighter_ = new ConkySyntaxHighlighter(textEdit_->document());
    
    // Status bar
    QFrame* statusBar = new QFrame();
    statusBar->setFrameStyle(QFrame::StyledPanel);
    statusBar->setStyleSheet("QFrame { background-color: #f0f0f0; padding: 2px; }");
    
    QHBoxLayout* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(5, 2, 5, 2);
    
    statusLabel_ = new QLabel("Ready");
    lineColLabel_ = new QLabel("Line: 1, Col: 1");
    modifiedLabel_ = new QLabel("");
    
    statusLayout->addWidget(statusLabel_);
    statusLayout->addStretch();
    statusLayout->addWidget(modifiedLabel_);
    statusLayout->addWidget(lineColLabel_);
    
    mainLayout->addWidget(statusBar);
}

void InAppEditor::setupToolbar() {
    QToolBar* toolbar = new QToolBar();
    toolbar->setMovable(false);
    toolbar->setStyleSheet(
        "QToolBar { background-color: #f8f8f8; border-bottom: 1px solid #ddd; padding: 4px; }"
        "QToolButton { padding: 4px 8px; margin: 2px; border-radius: 3px; }"
        "QToolButton:hover { background-color: #e0e0e0; }"
    );
    
    // File operations
    newAction_ = toolbar->addAction("New");
    newAction_->setShortcut(QKeySequence::New);
    newAction_->setToolTip("Create new file (Ctrl+N)");
    
    openAction_ = toolbar->addAction("Open");
    openAction_->setShortcut(QKeySequence::Open);
    openAction_->setToolTip("Open file (Ctrl+O)");
    
    toolbar->addSeparator();
    
    saveAction_ = toolbar->addAction("Save");
    saveAction_->setShortcut(QKeySequence::Save);
    saveAction_->setToolTip("Save file (Ctrl+S)");
    
    saveAsAction_ = toolbar->addAction("Save As");
    saveAsAction_->setShortcut(QKeySequence::SaveAs);
    saveAsAction_->setToolTip("Save file as (Ctrl+Shift+S)");
    
    toolbar->addSeparator();
    
    // Edit operations
    undoAction_ = toolbar->addAction("Undo");
    undoAction_->setShortcut(QKeySequence::Undo);
    undoAction_->setToolTip("Undo (Ctrl+Z)");
    undoAction_->setEnabled(false);
    
    redoAction_ = toolbar->addAction("Redo");
    redoAction_->setShortcut(QKeySequence::Redo);
    redoAction_->setToolTip("Redo (Ctrl+Y)");
    redoAction_->setEnabled(false);
    
    toolbar->addSeparator();
    
    cutAction_ = toolbar->addAction("Cut");
    cutAction_->setShortcut(QKeySequence::Cut);
    cutAction_->setToolTip("Cut (Ctrl+X)");
    
    copyAction_ = toolbar->addAction("Copy");
    copyAction_->setShortcut(QKeySequence::Copy);
    copyAction_->setToolTip("Copy (Ctrl+C)");
    
    pasteAction_ = toolbar->addAction("Paste");
    pasteAction_->setShortcut(QKeySequence::Paste);
    pasteAction_->setToolTip("Paste (Ctrl+V)");
    
    toolbar->addSeparator();
    
    selectAllAction_ = toolbar->addAction("Select All");
    selectAllAction_->setShortcut(QKeySequence::SelectAll);
    selectAllAction_->setToolTip("Select all (Ctrl+A)");
    
    toolbar->addSeparator();
    
    // Search operations
    findAction_ = toolbar->addAction("Find");
    findAction_->setShortcut(QKeySequence::Find);
    findAction_->setToolTip("Find (Ctrl+F)");
    
    replaceAction_ = toolbar->addAction("Replace");
    replaceAction_->setShortcut(QKeySequence::Replace);
    replaceAction_->setToolTip("Replace (Ctrl+H)");
    
    toolbar->addSeparator();
    
    goToLineAction_ = toolbar->addAction("Go to Line");
    goToLineAction_->setShortcut(QKeySequence("Ctrl+G"));
    goToLineAction_->setToolTip("Go to line (Ctrl+G)");
    
    // Insert toolbar at top of layout
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(layout());
    if (mainLayout) {
        mainLayout->insertWidget(0, toolbar);
    }
}

void InAppEditor::setupConnections() {
    // File operations
    connect(newAction_, &QAction::triggered, this, &InAppEditor::newFile);
    connect(openAction_, &QAction::triggered, this, static_cast<void (InAppEditor::*)()>(&InAppEditor::openFile));
    connect(saveAction_, &QAction::triggered, this, &InAppEditor::save);
    connect(saveAsAction_, &QAction::triggered, this, &InAppEditor::saveAs);
    
    // Edit operations
    connect(undoAction_, &QAction::triggered, this, &InAppEditor::undo);
    connect(redoAction_, &QAction::triggered, this, &InAppEditor::redo);
    connect(cutAction_, &QAction::triggered, this, &InAppEditor::cut);
    connect(copyAction_, &QAction::triggered, this, &InAppEditor::copy);
    connect(pasteAction_, &QAction::triggered, this, &InAppEditor::paste);
    connect(selectAllAction_, &QAction::triggered, this, &InAppEditor::selectAll);
    
    // Search operations
    connect(findAction_, &QAction::triggered, this, &InAppEditor::find);
    connect(replaceAction_, &QAction::triggered, this, &InAppEditor::replace);
    connect(goToLineAction_, &QAction::triggered, this, &InAppEditor::goToLine);
    
    // Text edit signals
    connect(textEdit_, &QTextEdit::textChanged, this, &InAppEditor::documentModified);
    connect(textEdit_, &QTextEdit::cursorPositionChanged, this, &InAppEditor::updateStatusBar);
    connect(textEdit_, &QTextEdit::undoAvailable, undoAction_, &QAction::setEnabled);
    connect(textEdit_, &QTextEdit::redoAvailable, redoAction_, &QAction::setEnabled);
}

bool InAppEditor::openFile(const QString& filePath) {
    if (!maybeSave()) {
        return false;
    }
    
    loadFile(filePath);
    return true;
}

bool InAppEditor::saveFile() {
    if (isUntitled_) {
        return saveFileAs();
    } else {
        saveToFile(currentFilePath_);
        return true;
    }
}

bool InAppEditor::saveFileAs() {
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Save As",
        currentFilePath_.isEmpty() ? QDir::homePath() : currentFilePath_,
        "Conky Config Files (*.conf);;All Files (*)"
    );
    
    if (filePath.isEmpty()) {
        return false;
    }
    
    saveToFile(filePath);
    return true;
}

void InAppEditor::newFile() {
    if (!maybeSave()) {
        return;
    }
    
    textEdit_->clear();
    currentFilePath_.clear();
    isUntitled_ = true;
    updateTitle();
    updateStatusBar();
}

void InAppEditor::openFile() {
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Open File",
        QDir::homePath(),
        "Conky Config Files (*.conf);;All Files (*)"
    );
    
    if (!filePath.isEmpty()) {
        loadFile(filePath);
    }
}

void InAppEditor::save() {
    saveFile();
}

void InAppEditor::saveAs() {
    saveFileAs();
}

void InAppEditor::undo() {
    textEdit_->undo();
}

void InAppEditor::redo() {
    textEdit_->redo();
}

void InAppEditor::cut() {
    textEdit_->cut();
}

void InAppEditor::copy() {
    textEdit_->copy();
}

void InAppEditor::paste() {
    textEdit_->paste();
}

void InAppEditor::selectAll() {
    textEdit_->selectAll();
}

void InAppEditor::find() {
    if (!findDialog_) {
        findDialog_ = new QDialog(this);
        findDialog_->setWindowTitle("Find");
        findDialog_->setMinimumWidth(400);
        
        QVBoxLayout* layout = new QVBoxLayout(findDialog_);
        
        QHBoxLayout* findLayout = new QHBoxLayout();
        findLayout->addWidget(new QLabel("Find:"));
        findText_ = new QLineEdit();
        findLayout->addWidget(findText_);
        layout->addLayout(findLayout);
        
        QHBoxLayout* optionsLayout = new QHBoxLayout();
        caseSensitive_ = new QCheckBox("Case sensitive");
        wholeWords_ = new QCheckBox("Whole words");
        optionsLayout->addWidget(caseSensitive_);
        optionsLayout->addWidget(wholeWords_);
        optionsLayout->addStretch();
        layout->addLayout(optionsLayout);
        
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
        QPushButton* findButton = buttonBox->addButton("Find", QDialogButtonBox::AcceptRole);
        connect(findButton, &QPushButton::clicked, this, &InAppEditor::findNext);
        connect(buttonBox, &QDialogButtonBox::rejected, findDialog_, &QDialog::close);
        layout->addWidget(buttonBox);
    }
    
    findDialog_->show();
    findDialog_->raise();
    findDialog_->activateWindow();
    findText_->setFocus();
    findText_->selectAll();
}

void InAppEditor::findNext() {
    if (!findDialog_ || findText_->text().isEmpty()) {
        return;
    }
    
    QString searchText = findText_->text();
    QTextDocument::FindFlags flags;
    
    if (caseSensitive_->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    if (wholeWords_->isChecked()) {
        flags |= QTextDocument::FindWholeWords;
    }
    
    if (!textEdit_->find(searchText, flags)) {
        // Wrap around
        QTextCursor cursor = textEdit_->textCursor();
        cursor.movePosition(QTextCursor::Start);
        textEdit_->setTextCursor(cursor);
        
        if (!textEdit_->find(searchText, flags)) {
            QMessageBox::information(this, "Find", "Text not found.");
        }
    }
}

void InAppEditor::replace() {
    // Simple replace dialog - could be expanded
    bool ok;
    QString findText = QInputDialog::getText(this, "Replace", "Find:", QLineEdit::Normal, "", &ok);
    if (!ok || findText.isEmpty()) {
        return;
    }
    
    QString replaceText = QInputDialog::getText(this, "Replace", "Replace with:", QLineEdit::Normal, "", &ok);
    if (!ok) {
        return;
    }
    
    // Replace all occurrences
    QTextDocument* doc = textEdit_->document();
    QTextCursor cursor(doc);
    
    while (!cursor.isNull() && !cursor.atEnd()) {
        cursor = doc->find(findText, cursor);
        if (!cursor.isNull()) {
            cursor.insertText(replaceText);
        }
    }
}

void InAppEditor::goToLine() {
    bool ok;
    int line = QInputDialog::getInt(this, "Go to Line", "Line number:", 1, 1, textEdit_->document()->blockCount(), 1, &ok);
    
    if (ok) {
        QTextBlock block = textEdit_->document()->findBlockByNumber(line - 1);
        if (block.isValid()) {
            QTextCursor cursor(block);
            textEdit_->setTextCursor(cursor);
            textEdit_->ensureCursorVisible();
        }
    }
}

void InAppEditor::updateStatusBar() {
    QTextCursor cursor = textEdit_->textCursor();
    int line = cursor.blockNumber() + 1;
    int col = cursor.columnNumber() + 1;
    
    lineColLabel_->setText(QString("Line: %1, Col: %2").arg(line).arg(col));
    
    if (textEdit_->document()->isModified()) {
        modifiedLabel_->setText("Modified");
        modifiedLabel_->setStyleSheet("color: #d32f2f; font-weight: bold;");
    } else {
        modifiedLabel_->setText("");
    }
}

void InAppEditor::documentModified() {
    updateTitle();
    updateStatusBar();
}

void InAppEditor::updateTitle() {
    QString title = isUntitled_ ? "Untitled" : QFileInfo(currentFilePath_).fileName();
    if (textEdit_->document()->isModified()) {
        title += " *";
    }
    title += " - In-App Editor";
    setWindowTitle(title);
}

bool InAppEditor::maybeSave() {
    if (!textEdit_->document()->isModified()) {
        return true;
    }
    
    QMessageBox::StandardButton ret = QMessageBox::warning(
        this,
        "In-App Editor",
        "The document has been modified.\nDo you want to save your changes?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
    );
    
    if (ret == QMessageBox::Save) {
        return saveFile();
    } else if (ret == QMessageBox::Cancel) {
        return false;
    }
    
    return true;
}

void InAppEditor::loadFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, "In-App Editor",
            QString("Cannot read file %1:\n%2.").arg(filePath, file.errorString()));
        return;
    }
    
    QTextStream in(&file);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    textEdit_->setPlainText(in.readAll());
    QApplication::restoreOverrideCursor();
    
    currentFilePath_ = filePath;
    isUntitled_ = false;
    
    updateTitle();
    updateStatusBar();
}

void InAppEditor::saveToFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, "In-App Editor",
            QString("Cannot write file %1:\n%2.").arg(filePath, file.errorString()));
        return;
    }
    
    QTextStream out(&file);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    out << textEdit_->toPlainText();
    QApplication::restoreOverrideCursor();
    
    currentFilePath_ = filePath;
    isUntitled_ = false;
    textEdit_->document()->setModified(false);
    
    updateTitle();
    updateStatusBar();
    
    statusLabel_->setText("File saved");
}

bool InAppEditor::isModified() const {
    return textEdit_->document()->isModified();
}