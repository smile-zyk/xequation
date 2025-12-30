#include "equation_group_editor.h"
#include "core/equation_group.h"
#include "equation_completion_model.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QListView>
#include <QFile>
#include <QTextStream>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>

namespace xequation
{
namespace gui
{

EquationGroupEditor::EquationGroupEditor(EquationCompletionModel* model, QWidget *parent, const QString &title)
    : QDialog(parent), language_name_(model->language_name())
{
    equation_completion_filter_model_ = new EquationCompletionFilterModel(model, this);

    setWindowTitle(title);
    setMinimumSize(800, 600);

    SetupUI();
    SetupConnections();
}

EquationGroupEditor::~EquationGroupEditor()
{
}

void EquationGroupEditor::SetEquationGroup(const EquationGroup* group)
{
    if(!group)
    {
        return;
    }
    equation_completion_filter_model_->SetEquationGroup(group);
    SetText(QString::fromStdString(group->statement()));
}

QString EquationGroupEditor::GetText() const
{
    return editor_ ? editor_->toPlainText() : QString();
}

void EquationGroupEditor::SetText(const QString &text)
{
    if (editor_) editor_->setPlainText(text);
}

void EquationGroupEditor::ClearText()
{
    if (editor_) editor_->clear();
}

void EquationGroupEditor::SetupUI()
{
    // Toolbar with actions
    tool_bar_ = new QToolBar(this);

    open_action_ = new QAction(QIcon(":/icons/file-open.png"), "Open", this);
    save_action_ = new QAction(QIcon(":/icons/file-save.png"), "Save", this);
    reset_action_ = new QAction(QIcon(":/icons/editor-eraser.png"), "Reset", this);
    undo_action_ = new QAction(QIcon(":/icons/undo.png"), "Undo", this);
    redo_action_ = new QAction(QIcon(":/icons/redo.png"), "Redo", this);
    switch_mode_action_ = new QAction(QIcon(":/icons/mode-light.png"), "Switch Mode", this);
    // Track style state in action data: false = light, true = dark
    switch_mode_action_->setData(false);

    tool_bar_->addAction(open_action_);
    tool_bar_->addAction(save_action_);
    tool_bar_->addSeparator();
    tool_bar_->addAction(undo_action_);
    tool_bar_->addAction(redo_action_);
    tool_bar_->addSeparator();
    tool_bar_->addAction(reset_action_);
    tool_bar_->addSeparator();
    tool_bar_->addAction(switch_mode_action_);

    // Central editor
    editor_ = new xequation::gui::CodeEditor(language_name_, this);
    editor_highlighter_ = CodeHighlighter::Create(language_name_, editor_->document());
    editor_completer_ = new CodeCompleter(this);

    equation_completion_filter_model_->sort(0);
    editor_completer_->setModel(equation_completion_filter_model_);
    editor_highlighter_->SetModel(equation_completion_filter_model_);
    editor_->setCompleter(editor_completer_);
    editor_->setHighlighter(editor_highlighter_);

    // Bottom-right buttons
    ok_button_ = new QPushButton("Ok", this);
    cancel_button_ = new QPushButton("Cancel", this);

    // Layout composition
    auto *main_layout = new QVBoxLayout(this);
    auto *button_layout = new QHBoxLayout();

    button_layout->addStretch();
    button_layout->addWidget(ok_button_);
    button_layout->addWidget(cancel_button_);

    main_layout->addWidget(tool_bar_);
    main_layout->addWidget(editor_, 1);
    main_layout->addLayout(button_layout);
}

void EquationGroupEditor::SetupConnections()
{
    // Toolbar actions
    connect(open_action_, &QAction::triggered, this, &EquationGroupEditor::OnOpen);
    connect(save_action_, &QAction::triggered, this, &EquationGroupEditor::OnSave);
    connect(reset_action_, &QAction::triggered, this, &EquationGroupEditor::OnReset);
    connect(undo_action_, &QAction::triggered, this, &EquationGroupEditor::OnUndo);
    connect(redo_action_, &QAction::triggered, this, &EquationGroupEditor::OnRedo);
    connect(switch_mode_action_, &QAction::triggered, this, &EquationGroupEditor::OnSwitchMode);

    // Dialog buttons
    connect(ok_button_, &QPushButton::clicked, this, &EquationGroupEditor::OnOkClicked);
    connect(cancel_button_, &QPushButton::clicked, this, &EquationGroupEditor::OnCancelClicked);
}

void EquationGroupEditor::OnOpen()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open File", QString(), "Text Files (*.txt);;Python Files (*.py);;All Files (*.*)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Open Failed", "Cannot open file: " + file.errorString());
        return;
    }
    QTextStream in(&file);
    SetText(in.readAll());
}

void EquationGroupEditor::OnSave()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save File", QString(), "Text Files (*.txt);;Python Files (*.py);;All Files (*.*)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Save Failed", "Cannot save file: " + file.errorString());
        return;
    }
    QTextStream out(&file);
    out << GetText();
}

void EquationGroupEditor::OnReset()
{
    ClearText();
}

void EquationGroupEditor::OnUndo()
{
    if (editor_) editor_->undo();
}

void EquationGroupEditor::OnRedo()
{
    if (editor_) editor_->redo();
}

void EquationGroupEditor::OnSwitchMode()
{
    // Toggle between light and dark
    bool isDark = switch_mode_action_->data().toBool();
    if (isDark)
    {
        editor_->SetStyleMode(CodeEditor::StyleMode::kLight);
        switch_mode_action_->setIcon(QIcon(":/icons/mode-light.png"));
        switch_mode_action_->setData(false);
        switch_mode_action_->setText("Switch to Dark");
    }
    else
    {
        editor_->SetStyleMode(CodeEditor::StyleMode::kDark);
        switch_mode_action_->setIcon(QIcon(":/icons/mode-light-filled.png"));
        switch_mode_action_->setData(true);
        switch_mode_action_->setText("Switch to Light");
    }
}

void EquationGroupEditor::OnOkClicked()
{
    emit TextSubmitted(GetText());
}

void EquationGroupEditor::OnCancelClicked()
{
    reject();
}

} // namespace gui
} // namespace xequation
