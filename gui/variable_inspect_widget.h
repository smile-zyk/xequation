#pragma once
#include <QWidget>
#include <memory>
#include <qchar.h>
#include <string>

#include "core/equation.h"
#include "core/equation_signals_manager.h"
#include "value_model_view/value_item.h"
#include "value_model_view/value_tree_model.h"
#include "value_model_view/value_tree_view.h"

namespace xequation
{
namespace gui {
class VariableInspectWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VariableInspectWidget(QWidget *parent = nullptr);
    virtual ~VariableInspectWidget() = default;
    
    void SetCurrentEquation(const Equation* equation);
    const Equation* current_equation() const { return current_equation_; }

    void OnEquationRemoving(const Equation* equation);
    void OnEquationUpdated(const Equation* equation, bitmask::bitmask<EquationUpdateFlag> change_type);
    void OnCurrentEquationChanged(const Equation* equation);

private:
    void SetupUI();
    void SetupConnections();

private:
    ValueTreeView *variable_tree_view_;
    ValueTreeModel *variable_tree_model_;
    std::map<std::string, std::unique_ptr<ValueItem>> variable_items_cache_;
    const Equation* current_equation_{nullptr};
};
}
}