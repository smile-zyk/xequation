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
    
    editor_ = new xequation::gui::EquationGroupEditor("Python", this);
    editor_->SetStyleMode(xequation::gui::EquationGroupEditor::StyleMode::kDark);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    okButton = new QPushButton("Ok", this);
    cancelButton = new QPushButton("Cancel", this);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addWidget(editor_);
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
    return editor_->toPlainText();
}

void TextEditorDialog::setText(const QString &text)
{
    editor_->setPlainText(text);
}

void TextEditorDialog::setPlaceholderText(const QString &placeholder)
{
    editor_->setPlaceholderText(placeholder);
}

void TextEditorDialog::clearText()
{
    editor_->clear();
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