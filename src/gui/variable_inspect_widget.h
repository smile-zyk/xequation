#pragma once
#include <QWidget>

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
    const Equation* current_equation() const { return current_equation_; }
    void OnCurrentEquationChanged(const Equation* equation);
    void OnEquationRemoving(const Equation* equation);
    void OnEquationUpdated(const Equation* equation, bitmask::bitmask<EquationUpdateFlag> change_type);
signals:
    void AddExpressionToWatch(const QString &expression);

private:
    void SetupUI();
    void SetupConnections();

protected:
    void OnCustomContextMenuRequested(const QPoint& pos);
    void OnCopyVariableValue();
    void OnAddVariableToWatch();
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    ValueTreeView *view_;
    ValueTreeModel *model_;
    const Equation* current_equation_{};
    ValueItem::UniquePtr current_variable_item_;
    // actions
    QAction* copy_action_{};
    QAction* add_watch_action_{};
};
}
}