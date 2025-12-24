#pragma once

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

#include "code_editor/code_editor.h"

class TextEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TextEditorDialog(QWidget *parent = nullptr, const QString &title = "text editor");
    ~TextEditorDialog();

    QString getText() const;
    void setText(const QString &text);
    void setPlaceholderText(const QString &placeholder);
    void clearText();

signals:
    void textSubmitted(const QString &text);

public slots:
    void onOkClicked();
    void reject() override;

private:
    void setupUI();
    void setupConnections();

    xequation::gui::CodeEditor *editor_;
    QPushButton *okButton;
    QPushButton *cancelButton;
};
