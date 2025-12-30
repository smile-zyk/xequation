#pragma once

#include "core/equation.h"
#include "value_model_view/value_item.h"
#include "value_model_view/value_tree_model.h"
#include "value_model_view/value_tree_view.h"

#include <QEvent>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <map>

namespace xequation
{
namespace gui
{
class ExpressionWatchModel : public ValueTreeModel
{
    Q_OBJECT
  public:
    ExpressionWatchModel(QObject *parent = nullptr);
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool IsPlaceHolderIndex(const QModelIndex &index) const;
    void AddWatchItem(ValueItem *item);
    void RemoveWatchItem(ValueItem *item);
    void ReplaceWatchItem(ValueItem *old_item, ValueItem *new_item);
  signals:
    void RequestAddWatchItem(const QString &expression);
    void RequestRemoveWatchItem(ValueItem *item);
    void RequestReplaceWatchItem(ValueItem *old_item, const QString &new_expression);

  private:
    void *placeholder_flag_ = nullptr;
};

class ExpressionWatchWidget : public QWidget
{
  public:
    using ParseExprHandler = std::function<ParseResult(const QString &expression)>;
    using EvalExprHandler = std::function<InterpretResult(const QString &expression)>;
    ExpressionWatchWidget(ParseExprHandler parse_handler, EvalExprHandler eval_handler, QWidget *parent = nullptr);
    ~ExpressionWatchWidget() = default;
    void OnEquationRemoved(const std::string &equation_name);
    void OnEquationUpdated(const Equation *equation, bitmask::bitmask<EquationUpdateFlag> change_type);
    void OnAddExpressionToWatch(const QString &expression);

  protected:
    void SetupUI();
    void SetupConnections();
    static int GetSelectionFlags(const QModelIndexList &indexes, ExpressionWatchModel *model);
    ValueItem *CreateWatchItem(const QString &expression);
    void DeleteWatchItem(ValueItem *item);
    void SetCurrentItemToPlaceholder();
    void OnRequestAddWatchItem(const QString &expression);
    void OnRequestRemoveWatchItem(ValueItem *item);
    void OnRequestReplaceWatchItem(ValueItem *old_item, const QString &new_expression);

    void OnCustomContextMenuRequested(const QPoint &pos);
    void OnSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void OnCopyExpressionValue();
    void OnPasteExpression();
    void OnEditExpression();
    void OnDeleteExpression();
    void OnSelectAllExpressions();
    void OnClearAllExpressions();
    bool eventFilter(QObject *obj, QEvent *event) override;

  private:
    typedef boost::bimaps::bimap<boost::bimaps::multiset_of<ValueItem *>, boost::bimaps::multiset_of<std::string>>
        ExpressionItemEquationNameBimap;
    ExpressionWatchModel *model_;
    ValueTreeView *view_;
    ExpressionItemEquationNameBimap expression_item_equation_name_bimap_;
    std::multimap<std::string, ValueItem::UniquePtr> expression_item_map_;

    // actions
    QAction *copy_action_{};
    QAction *paste_action_{};
    QAction *edit_expression_action_{};
    QAction *delete_watch_action_{};
    QAction *select_all_action_{};
    QAction *clear_all_action_{};

    ParseExprHandler parse_handler_;
    EvalExprHandler eval_handler_;
};
} // namespace gui
} // namespace xequation