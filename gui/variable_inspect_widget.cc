#include "variable_inspect_widget.h"
#include "value_model_view/value_item.h"
#include "value_model_view/value_item_builder.h"
#include <QVBoxLayout>
namespace xequation
{
namespace gui
{

VariableInspectWidget::VariableInspectWidget(QWidget *parent)
    : QWidget(parent), model_(nullptr), view_(nullptr)
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

    view_ = new ValueTreeView(this);
    model_ = new ValueTreeModel(view_);

    view_->SetValueModel(model_);
    view_->SetHeaderSectionResizeRatio(0, 1);
    view_->SetHeaderSectionResizeRatio(1, 3);
    view_->SetHeaderSectionResizeRatio(2, 1);

    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->addWidget(view_);
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
    if (current_equation_ == equation)
    {
        return;
    }
    current_equation_ = equation;
    model_->Clear();
    if (current_equation_)
    {
        QString name = QString::fromStdString(current_equation_->name());
        if (variable_items_cache_.find(current_equation_->name()) == variable_items_cache_.end())
        {
            Value value = current_equation_->GetValue();
            ValueItem::UniquePtr item;
            if (value.IsNull())
            {
                item = ValueItem::Create(name, QString::fromStdString(current_equation_->message()), "error");
            }
            else
            {
                item = gui::BuilderUtils::CreateValueItem(name, current_equation_->GetValue());
            }
            model_->AddRootItem(item.get());
            variable_items_cache_[current_equation_->name()] = std::move(item);
        }
        else
        {
            ValueItem *item = variable_items_cache_[current_equation_->name()].get();
            model_->AddRootItem(item);
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
    if (change_type & EquationUpdateFlag::kValue)
    {
        if (variable_items_cache_.find(equation->name()) != variable_items_cache_.end())
        {
            variable_items_cache_.erase(equation->name());
        }
    }

    if (equation == current_equation_ && change_type & EquationUpdateFlag::kValue)
    {
        current_equation_ = nullptr;
        SetCurrentEquation(equation);
    }
}
} // namespace gui
} // namespace xequation