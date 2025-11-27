// TextEditorDialog.cpp
#include "text_editor_dialog.h"

// TextEditorDialog.cpp
#include <QMessageBox>

TextEditorDialog::TextEditorDialog(QWidget *parent, const QString &title)
    : QDialog(parent)
{
    setWindowTitle(title);
    setupUI();
    setupConnections();
}

TextEditorDialog::~TextEditorDialog()
{
}

void TextEditorDialog::setupUI()
{
    setMinimumSize(600, 400);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    textEdit = new QTextEdit(this);
    textEdit->setAcceptRichText(false);
    textEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    okButton = new QPushButton("Ok", this);
    cancelButton = new QPushButton("Cancel", this);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addWidget(textEdit);
    mainLayout->addLayout(buttonLayout);
    
    okButton->setDefault(true);
}

void TextEditorDialog::setupConnections()
{
    connect(okButton, &QPushButton::clicked, this, &TextEditorDialog::onOkClicked);
    connect(cancelButton, &QPushButton::clicked, this, &TextEditorDialog::reject);
}

QString TextEditorDialog::getText() const
{
    return textEdit->toPlainText();
}

void TextEditorDialog::setText(const QString &text)
{
    textEdit->setPlainText(text);
}

void TextEditorDialog::setPlaceholderText(const QString &placeholder)
{
    textEdit->setPlaceholderText(placeholder);
}

void TextEditorDialog::clearText()
{
    textEdit->clear();
}

void TextEditorDialog::onOkClicked()
{
    QString text = getText();
    
    emit textSubmitted(text);
}

void TextEditorDialog::reject()
{
    clearText();
    QDialog::reject();
}