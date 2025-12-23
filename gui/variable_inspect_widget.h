#pragma once
#include <QWidget>
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
    explicit VariableInspectWidget(const EquationManager* manager, QWidget *parent = nullptr);
    virtual ~VariableInspectWidget() = default;
    
    void SetCurrentEquation(const Equation* equation);
    const Equation* current_equation() const { return current_equation_; }
    void OnCurrentEquationChanged(const Equation* equation);

signals:
    void AddExpressionToWatch(const QString &expression);

private:
    void SetupUI();
    void SetupConnections();

protected:
    void OnEquationRemoving(const Equation* equation);
    void OnEquationUpdated(const Equation* equation, bitmask::bitmask<EquationUpdateFlag> change_type);
    void OnCustomContextMenuRequested(const QPoint& pos);
    void OnCopyVariableValue();
    void OnAddVariableToWatch();
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    ValueTreeView *view_;
    ValueTreeModel *model_;
    const EquationManager* manager_;
    std::map<std::string, std::unique_ptr<ValueItem>> variable_items_cache_;
    const Equation* current_equation_{};
    
    // actions
    QAction* copy_action_{};
    QAction* add_watch_action_{};
};
}
}