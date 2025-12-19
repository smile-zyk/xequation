#pragma once

#include "core/equation.h"
#include "core/equation_common.h"
#include "core/equation_signals_manager.h"
#include "value_model_view/value_item.h"
#include "value_model_view/value_tree_model.h"
#include "value_model_view/value_tree_view.h"
#include <functional>
#include <qchar.h>

namespace xequation
{
namespace gui
{
class ExpressionWatchModel : public ValueTreeModel
{
    Q_OBJECT
public:
    ExpressionWatchModel(QObject* parent = nullptr);
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool IsPlaceHolderIndex(const QModelIndex& index) const;
    void OnExpressionValueItemAdded(ValueItem* item);
    void OnExpressionValueItemReplaced(ValueItem* old_item, ValueItem* new_item);
signals:
    void RequestAddWatchExpression(const QString& expression);
    void RequestReplaceWatchExpression(const QString& old_expression, const QString& new_expression);
private:
    void* placeholder_flag_ = nullptr;
};

class ExpressionWatchWidget : public QWidget
{
public:
    using EvalExprHandler = std::function<InterpretResult(const std::string&)>;
    using ParseExprHandler = std::function<ParseResult(const std::string&)>;
    ExpressionWatchWidget(EvalExprHandler eval_handler, ParseExprHandler parse_handler, QWidget* parent = nullptr);
    ~ExpressionWatchWidget() = default;

    void OnEquationAdded(const Equation* equation);
    void OnEquationRemoving(const Equation* equation);
    void OnEquationUpdated(const Equation* equation, bitmask::bitmask<EquationUpdateFlag> change_type);
    void OnCurrentEquationChanged(const Equation* equation);

protected:
    void SetupUI();
    void SetupConnections();
    void OnRequestAddWatchExpression(const QString& expression);
    void OnRequestReplaceWatchExpression(const QString& old_expression, const QString& new_expression);

private:
    ExpressionWatchModel* model_;
    ValueTreeView* view_;
    std::set<ValueItem::UniquePtr> watch_items_set_;
    EvalExprHandler eval_handler_ = nullptr;
    ParseExprHandler parse_handler_ = nullptr;
};
} // namespace gui
} // namespace xequation