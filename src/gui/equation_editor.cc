#include "equation_editor.h"
#include "code_editor/completion_line_edit.h"
#include "core/equation.h"
#include "equation_completion_model.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <qcompleter.h>
#include <qdialog.h>
#include <qlistview.h>

namespace xequation
{
namespace gui
{
ContextSelectionWidget::ContextSelectionWidget(EquationCompletionFilterModel* model, QWidget *parent)
    : QWidget(parent), model_(model)
{
    SetupUI();
    SetupConnections();
}

void ContextSelectionWidget::SetupUI()
{
    QVBoxLayout *main_layout = new QVBoxLayout(this);

    context_combo_box_ = new QComboBox(this);
    auto categories = model_->GetAllCategories();
    for( const auto &category : categories)
    {
        context_combo_box_->addItem(category.name);
    }

    context_filter_edit_ = new QLineEdit(this);
    context_filter_edit_->setPlaceholderText("Filter variables...");

    context_list_view_ = new QListView(this);
    context_list_view_->setModel(model_);

    main_layout->addWidget(context_combo_box_);
    main_layout->addWidget(context_filter_edit_);
    main_layout->addWidget(context_list_view_);

    adjustSize();
    main_layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    OnComboBoxChanged(context_combo_box_->currentText());
}

void ContextSelectionWidget::SetupConnections()
{
    connect(context_combo_box_, &QComboBox::currentTextChanged, this, &ContextSelectionWidget::OnComboBoxChanged);
    connect(context_filter_edit_, &QLineEdit::textChanged, this, &ContextSelectionWidget::OnFilterTextChanged);
    connect(context_list_view_, &QListView::doubleClicked, this, &ContextSelectionWidget::OnListViewDoubleClicked);
}

QString ContextSelectionWidget::GetSelectedVariable() const
{
    QModelIndex current_index = context_list_view_->currentIndex();
    if (current_index.isValid())
    {
        return current_index.data(Qt::DisplayRole).toString();
    }
    return QString();
}

void ContextSelectionWidget::OnComboBoxChanged(const QString &text)
{
    model_->SetCategory(text);
}

void ContextSelectionWidget::OnFilterTextChanged(const QString &text)
{
    model_->SetFilterText(text);
}

void ContextSelectionWidget::OnListViewDoubleClicked(const QModelIndex &index)
{
    if (index.isValid())
    {
        QString variable = index.data().toString();
        emit VariableDoubleClicked(variable);
    }
}

EquationEditor::EquationEditor(EquationCompletionModel* language_model, QWidget *parent)
    : QDialog(parent), group_(nullptr)
{
    context_selection_filter_model_ = new EquationCompletionFilterModel(language_model, this);
    context_selection_filter_model_->SetDisplayOnlyWord(true);
    completion_filter_model_ = new EquationCompletionFilterModel(language_model, this);
    SetupUI();
    SetupConnections();
}

void EquationEditor::SetEquationGroup(const EquationGroup* group)
{
    group_ = group;
    context_selection_filter_model_->SetEquationGroup(group);
    completion_filter_model_->SetEquationGroup(group);
    if (group_ != nullptr)
    {
        const auto &equation_names = group_->GetEquationNames();
        if (equation_names.size() == 1)
        {
            const auto &equation = group_->GetEquation(equation_names[0]);
            equation_name_edit_->setText(QString::fromStdString(equation->name()));
            expression_edit_->setText(QString::fromStdString(equation->content()));
        }
        else 
        {
            equation_name_edit_->clear();
            expression_edit_->clear();
        }
    }
}

void EquationEditor::SetupUI()
{
    equation_name_label_ = new QLabel("Name:", this);
    equation_name_edit_ = new QLineEdit(this);
    expression_label_ = new QLabel("Expression:", this);
    expression_edit_ = new CompletionLineEdit(this);
    insert_button_ = new QPushButton("Insert", this);
    switch_to_group_button_ = new QPushButton("Switch to Group", this);
    ok_button_ = new QPushButton("OK", this);
    cancel_button_ = new QPushButton("Cancel", this);

    context_button_ = new QPushButton("Context>>", this);

    context_selection_widget_ = new ContextSelectionWidget(context_selection_filter_model_, this);
    context_selection_widget_->setVisible(false);
    insert_button_->setVisible(false);
    equation_name_edit_->setPlaceholderText("Enter equation name...");
    expression_edit_->setPlaceholderText("Enter equation expression...");
    expression_edit_->SetCompletionModel(completion_filter_model_);

    QVBoxLayout *main_layout = new QVBoxLayout();
    QGridLayout *equation_layout = new QGridLayout();
    QVBoxLayout *context_layout = new QVBoxLayout();
    QHBoxLayout *context_button_layout = new QHBoxLayout();
    QHBoxLayout *button_layout = new QHBoxLayout();

    equation_layout->addWidget(equation_name_label_, 0, 0);
    equation_layout->addWidget(equation_name_edit_, 0, 1);
    equation_layout->addWidget(expression_label_, 1, 0);
    equation_layout->addWidget(expression_edit_, 1, 1);

    context_button_layout->addStretch();
    context_button_layout->addWidget(context_button_);
    context_layout->addLayout(context_button_layout);
    context_layout->addWidget(context_selection_widget_);

    button_layout->addWidget(switch_to_group_button_);
    button_layout->addStretch();
    button_layout->addWidget(insert_button_);
    button_layout->addWidget(ok_button_);
    button_layout->addWidget(cancel_button_);

    main_layout->addLayout(equation_layout);
    main_layout->addLayout(context_layout);
    main_layout->addLayout(button_layout);
    setLayout(main_layout);

    setWindowTitle("Insert Equation");
    adjustSize();
    setMinimumWidth(500);
    main_layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
}

void EquationEditor::SetupConnections()
{
    connect(context_button_, &QPushButton::clicked, this, &EquationEditor::OnContextButtonClicked);
    connect(insert_button_, &QPushButton::clicked, this, &EquationEditor::OnInsertButtonClicked);
    connect(switch_to_group_button_, &QPushButton::clicked, this, &EquationEditor::OnSwitchToGroupEditorClicked);
    connect(ok_button_, &QPushButton::clicked, this, &EquationEditor::OnOkButtonClicked);
    connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);
    connect(context_selection_widget_, &ContextSelectionWidget::VariableDoubleClicked, this, &EquationEditor::OnVariableDoubleClicked);
}

void EquationEditor::OnContextButtonClicked()
{
    if (context_button_->text().contains(">>"))
    {
        context_selection_widget_->setVisible(true);
        insert_button_->setVisible(true);
        context_button_->setText("Context<<");
    }
    else
    {
        context_selection_widget_->setVisible(false);
        insert_button_->setVisible(false);
        context_button_->setText("Context>>");
    }
}

void EquationEditor::OnInsertButtonClicked()
{
    QString select_variable = context_selection_widget_->GetSelectedVariable();
    if (select_variable.isEmpty() == false)
    {
        expression_edit_->insert(select_variable);
    }
}

void EquationEditor::OnVariableDoubleClicked(const QString& variable)
{
    if (variable.isEmpty() == false)
    {
        expression_edit_->insert(variable);
    }
}

void EquationEditor::OnSwitchToGroupEditorClicked()
{
    QString name = equation_name_edit_->text().trimmed();
    QString expression = expression_edit_->text().trimmed();
    QString initial_text;
    
    if (!name.isEmpty() && !expression.isEmpty())
    {
        initial_text = name + " = " + expression;
    }
    else if (!name.isEmpty())
    {
        initial_text = name + " = ";
    }
    else if (!expression.isEmpty())
    {
        initial_text = expression;
    }
    
    emit SwitchToGroupEditorRequest(initial_text);
    accept();  // Close this dialog
}

void EquationEditor::OnOkButtonClicked()
{
    QString name = equation_name_edit_->text();
    QString expression = expression_edit_->text();

    if (name.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "equation name is empty!", QMessageBox::Ok);
        return;
    }

    if (expression.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "equation expression is empty", QMessageBox::Ok);
        return;
    }

    if (group_ == nullptr)
    {
        emit AddEquationRequest(name, expression);
    }
    else
    {
        emit EditEquationRequest(group_->id(), name, expression);
    }
}
} // namespace gui
} // namespace xequation