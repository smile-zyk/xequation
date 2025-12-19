#pragma once

#include "core/equation_common.h"
#include "value_model_view/value_item.h"
#include "value_model_view/value_tree_model.h"
#include "value_model_view/value_tree_view.h"

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
    void RequestAddWatchExpression(const QString& variable_name);
    void RequestReplaceWatchExpression(const QString& old_variable_name, const QString& new_variable_name);
private:
    void* placeholder_flag_ = nullptr;
};

class ExpressionWatchWidget : public QWidget
{
public:
    ExpressionWatchWidget(QWidget* parent = nullptr);
    ~ExpressionWatchWidget() = default;
protected:
    void SetupUI();
    void SetupConnections();
    void OnRequestAddWatchExpression(const QString& variable_name);
    void OnRequestReplaceWatchExpression(const QString& old_variable_name, const QString& new_variable_name);
private:
    ExpressionWatchModel* model_;
    ValueTreeView* view_;
    std::map<QString, ValueItem::UniquePtr> watch_items_map_;
};
} // namespace gui
} // namespace xequation