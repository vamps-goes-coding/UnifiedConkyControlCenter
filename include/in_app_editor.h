#pragma once

#include <QDialog>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QLabel>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QFont>
#include <QTextStream>
#include <QFileInfo>
#include <QLineEdit>
#include <QCheckBox>
#include <QDir>

class ConkySyntaxHighlighter;

class InAppEditor : public QDialog {
    Q_OBJECT

public:
    explicit InAppEditor(QWidget* parent = nullptr);
    
    bool openFile(const QString& filePath);
    bool saveFile();
    bool saveFileAs();
    
    QString getCurrentFilePath() const { return currentFilePath_; }
    bool isModified() const;

private slots:
    void newFile();
    void openFile();
    void save();
    void saveAs();
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();
    void find();
    void findNext();
    void replace();
    void goToLine();
    void updateStatusBar();
    void documentModified();

private:
    void setupUI();
    void setupToolbar();
    void setupConnections();
    void updateTitle();
    bool maybeSave();
    void loadFile(const QString& filePath);
    void saveToFile(const QString& filePath);
    
    QTextEdit* textEdit_;
    ConkySyntaxHighlighter* highlighter_;
    QLabel* statusLabel_;
    QLabel* lineColLabel_;
    QLabel* modifiedLabel_;
    
    QString currentFilePath_;
    bool isUntitled_;
    
    // Find/Replace
    QDialog* findDialog_;
    QLineEdit* findText_;
    QLineEdit* replaceText_;
    QCheckBox* caseSensitive_;
    QCheckBox* wholeWords_;
    
    // Actions
    QAction* newAction_;
    QAction* openAction_;
    QAction* saveAction_;
    QAction* saveAsAction_;
    QAction* undoAction_;
    QAction* redoAction_;
    QAction* cutAction_;
    QAction* copyAction_;
    QAction* pasteAction_;
    QAction* selectAllAction_;
    QAction* findAction_;
    QAction* replaceAction_;
    QAction* goToLineAction_;
};