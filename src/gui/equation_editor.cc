#include "equation_editor.h"
#include "code_editor/completion_line_edit.h"
#include "code_editor/completion_model.h"
#include "core/equation.h"
#include "equation_completion_model.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QVBoxLayout>
#include <qcompleter.h>
#include <qdialog.h>
#include <qlistview.h>
#include <qnamespace.h>

namespace xequation
{
namespace gui
{
ContextSelectionWidget::ContextSelectionWidget(EquationCompletionModel* model, QWidget *parent)
    : QWidget(parent), model_(model), proxy_model_(new ContextFilterProxyModel(model, this))
{
    SetupUI();
    SetupConnections();
}

void ContextSelectionWidget::SetupUI()
{
    QVBoxLayout *main_layout = new QVBoxLayout(this);

    context_combo_box_ = new QComboBox(this);
    auto categories = proxy_model_->GetAllCategories();
    for(const auto &category : categories)
    {
        context_combo_box_->addItem(category);
    }

    context_filter_edit_ = new QLineEdit(this);
    context_filter_edit_->setPlaceholderText("Filter variables...");

    context_list_view_ = new QListView(this);
    context_list_view_->setModel(proxy_model_);

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
        return current_index.data(Qt::EditRole).toString();
    }
    return QString();
}

void ContextSelectionWidget::OnComboBoxChanged(const QString &text)
{
    proxy_model_->SetFilterCategory(text);
    proxy_model_->invalidate();
}

void ContextSelectionWidget::OnFilterTextChanged(const QString &text)
{
    proxy_model_->SetFilterText(text);
    proxy_model_->invalidate();
}

void ContextSelectionWidget::OnListViewDoubleClicked(const QModelIndex &index)
{
    if (index.isValid())
    {
        QString variable = index.data(Qt::EditRole).toString();
        emit VariableDoubleClicked(variable);
    }
}

void ContextSelectionWidget::ResetFilters()
{
    // Clear filter text
    if (context_filter_edit_)
    {
        context_filter_edit_->blockSignals(true);
        context_filter_edit_->clear();
        context_filter_edit_->blockSignals(false);
    }
    // Reset combo box to first category
    if (context_combo_box_)
    {
        context_combo_box_->blockSignals(true);
        context_combo_box_->setCurrentIndex(0);
        context_combo_box_->blockSignals(false);
        OnComboBoxChanged(context_combo_box_->currentText());
    }
}

void ContextSelectionWidget::RefreshCategories()
{
    // Reload categories from model
    if (context_combo_box_)
    {
        context_combo_box_->blockSignals(true);
        context_combo_box_->clear();
        
        if (proxy_model_)
        {
            auto categories = proxy_model_->GetAllCategories();
            for (const auto &category : categories)
            {
                context_combo_box_->addItem(category);
            }
        }
        
        context_combo_box_->setCurrentIndex(0);
        context_combo_box_->blockSignals(false);
        if (context_combo_box_->count() > 0)
        {
            OnComboBoxChanged(context_combo_box_->currentText());
        }
    }
}

EquationEditor::EquationEditor(EquationCompletionModel* language_model, QWidget *parent)
    : QDialog(parent), group_(nullptr), completion_model_(language_model)
{
    SetupUI();
    SetupConnections();
}

void EquationEditor::SetEquationGroup(const EquationGroup* group)
{
    group_ = group;
    
    // Refresh categories when group changes
    if (context_selection_widget_)
    {
        context_selection_widget_->RefreshCategories();
    }
    
    if (group_ != nullptr)
    {
        setWindowTitle("Edit Equation");
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
    else
    {
        setWindowTitle("Insert Equation");
    }
}

void EquationEditor::SetupUI()
{
    equation_name_label_ = new QLabel("Name:", this);
    equation_name_edit_ = new QLineEdit(this);
    expression_label_ = new QLabel("Expression:", this);
    expression_edit_ = new CompletionLineEdit(this);
    insert_button_ = new QPushButton("Insert", this);
    switch_to_group_button_ = new QPushButton("Use Editor", this);
    ok_button_ = new QPushButton("OK", this);
    cancel_button_ = new QPushButton("Cancel", this);

    context_button_ = new QPushButton("Context>>", this);

    context_selection_widget_ = new ContextSelectionWidget(completion_model_, this);
    context_selection_widget_->setVisible(false);
    insert_button_->setVisible(false);
    equation_name_edit_->setPlaceholderText("Enter equation name...");
    expression_edit_->setPlaceholderText("Enter equation expression...");
    expression_edit_->SetCompletionModel(completion_model_);

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
    
    emit UseCodeEditorRequest(initial_text);
    reject();
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

void EquationEditor::ClearText()
{
    if (equation_name_edit_) equation_name_edit_->clear();
    if (expression_edit_) expression_edit_->clear();
}

void EquationEditor::ResetState()
{
    // Reset all state except text fields
    group_ = nullptr;
    
    // Reset context selection widget to initial state (hide and clear filters)
    if (context_selection_widget_)
    {
        context_selection_widget_->setVisible(false);
        context_selection_widget_->ResetFilters();
    }
    
    if (insert_button_) insert_button_->setVisible(false);
    if (context_button_) context_button_->setText("Context>>");
}

void EquationEditor::done(int result)
{
    ResetState();
    ClearText();
    QDialog::done(result);
}

void EquationEditor::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    // Refresh categories when window is about to show
    if (context_selection_widget_)
    {
        context_selection_widget_->RefreshCategories();
    }
}

void EquationEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        event->ignore();  // Ignore Escape key to prevent closing dialog
        return;
    }
    QDialog::keyPressEvent(event);
}

// ContextFilterProxyModel implementation
ContextSelectionWidget::ContextFilterProxyModel::ContextFilterProxyModel(EquationCompletionModel *model, QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(model);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

QVector<QString> ContextSelectionWidget::ContextFilterProxyModel::GetAllCategories() const
{
    QVector<QString> categories;
    if (sourceModel() == nullptr)
        return categories;

    auto source_model = dynamic_cast<EquationCompletionModel *>(sourceModel());
    if (source_model == nullptr)
        return categories;

    int row_count = source_model->rowCount();
    for (int row = 0; row < row_count; ++row)
    {
        QModelIndex index = source_model->index(row, 0);
        QString category = index.data(CompletionModel::kCategoryRole).toString();
        if (!category.isEmpty() && !categories.contains(category))
        {
            categories.push_back(category);
        }
    }
    return categories;
}

void ContextSelectionWidget::ContextFilterProxyModel::SetFilterCategory(const QString &category)
{
    filter_category_ = category;
    invalidateFilter();
}

void ContextSelectionWidget::ContextFilterProxyModel::SetFilterText(const QString &text)
{
    filter_text_ = text;
    invalidateFilter();
}

QVariant ContextSelectionWidget::ContextFilterProxyModel::data(const QModelIndex &index, int role) const
{
    if(role == Qt::DisplayRole)
    {
        QModelIndex source_index = mapToSource(index);
        return sourceModel()->data(source_index, CompletionModel::kWordRole);
    }

    return QSortFilterProxyModel::data(index, role);
}

bool ContextSelectionWidget::ContextFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (sourceModel() == nullptr)
        return false;

    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    
    // Filter by category
    if (!filter_category_.isEmpty())
    {
        QString category = index.data(CompletionModel::kCategoryRole).toString();
        if (category != filter_category_)
            return false;
    }

    // Filter by text
    if (!filter_text_.isEmpty())
    {
        QString text = index.data(CompletionModel::kWordRole).toString();
        if (!text.contains(filter_text_, Qt::CaseInsensitive))
            return false;
    }

    return true;
}

} // namespace gui
} // namespace xequation