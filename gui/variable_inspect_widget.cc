#include "variable_inspect_widget.h"
#include "value_model_view/value_tree_view.h"
#include "value_model_view/value_item_builder.h"
#include <QVBoxLayout>

namespace xequation
{
namespace gui
{

VariableInspectWidget::VariableInspectWidget(QWidget *parent)
    : QWidget(parent), variable_tree_model_(nullptr), variable_tree_view_(nullptr)
{
    SetupUI();
    SetupConnections();
}

void VariableInspectWidget::SetupUI()
{
    setWindowTitle("Variable Inspector");
    setWindowFlags(
        Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint
    );

    variable_tree_model_ = new ValueTreeModel(this);
    variable_tree_view_ = new ValueTreeView(variable_tree_model_, this);
    variable_tree_view_->SetHeaderSectionResizeRatio(0, 1);
    variable_tree_view_->SetHeaderSectionResizeRatio(1, 3);
    variable_tree_view_->SetHeaderSectionResizeRatio(2, 1);

    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->addWidget(variable_tree_view_);
    setLayout(main_layout);

    setMinimumSize(800, 600);
}

void VariableInspectWidget::SetupConnections() {}

void VariableInspectWidget::OnCurrentEquationChanged(const Equation *equation)
{
    SetCurrentEquation(equation);
}

void VariableInspectWidget::SetCurrentEquation(const Equation *equation)
{
    current_equation_ = equation;
    variable_tree_model_->Clear();
    if (current_equation_)
    {
        QString name = QString::fromStdString(current_equation_->name());
        if (variable_items_cache_.find(current_equation_->name()) == variable_items_cache_.end())
        {
            ValueItem::UniquePtr item = gui::BuilderUtils::CreateValueItem(name, current_equation_->GetValue());
            variable_tree_model_->AddRootItem(item.get());
            variable_items_cache_[current_equation_->name()] = std::move(item);
        }
        else
        {
            ValueItem *item = variable_items_cache_[current_equation_->name()].get();
            variable_tree_model_->AddRootItem(item);
        }
    }
}

void VariableInspectWidget::OnEquationRemoving(const Equation *equation)
{
    if (variable_items_cache_.find(equation->name()) != variable_items_cache_.end())
    {
        variable_items_cache_.erase(equation->name());
    }
    if (equation == current_equation_)
    {
        SetCurrentEquation(nullptr);
    }
}

void VariableInspectWidget::OnEquationUpdated(
    const Equation *equation, bitmask::bitmask<EquationUpdateFlag> change_type
)
{
    if(change_type & EquationUpdateFlag::kValue)
    {
        if (variable_items_cache_.find(equation->name()) != variable_items_cache_.end())
        {
            variable_items_cache_.erase(equation->name());
        }
    }

    if (equation == current_equation_ && change_type & EquationUpdateFlag::kValue)
    {
        SetCurrentEquation(equation);
    }
}
} // namespace gui
} // namespace xequation