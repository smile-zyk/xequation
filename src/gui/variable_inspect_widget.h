#pragma once
#include <QWidget>
#include <unordered_map>
#include <vector>

#include "core/equation.h"
#include "value_model/value_item.h"
#include "value_model/value_tree_model.h"
#include "value_model/value_tree_view.h"

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
    void SetCurrentEquations(const std::vector<const Equation*>& equations);
    const std::vector<const Equation*>& current_equations() const { return current_equations_; }
    void OnCurrentEquationChanged(const Equation* equation);
    void OnEquationRemoving(const Equation* equation);
    void OnEquationUpdated(const Equation* equation, bitmask::bitmask<EquationUpdateFlag> change_type);
signals:
    void AddExpressionToWatch(const QString &expression);

private:
    void SetupUI();
    void SetupConnections();
    void RefreshModel();
    ValueItem::UniquePtr BuildRootItemFromEquation(const Equation* equation) const;

protected:
    void OnCustomContextMenuRequested(const QPoint& pos);
    void OnCopyVariableValue();
    void OnAddVariableToWatch();
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    ValueTreeView *view_;
    ValueTreeModel *model_;
    std::vector<const Equation*> current_equations_{};
    std::unordered_map<const Equation*, ValueItem::UniquePtr> equation_variable_items_;
    // actions
    QAction* copy_action_{};
    QAction* add_watch_action_{};
};
}
}