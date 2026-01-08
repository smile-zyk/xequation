#pragma once

#include "core/equation.h"
#include "value_model/value_item.h"
#include "value_model/value_tree_model.h"
#include "value_model/value_tree_view.h"
#include "code_editor/completion_line_edit_delegate.h"
#include "equation_completion_model.h"

#include <QEvent>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <quuid.h>

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
    void RemoveWatchItem(const QUuid& id);
    void ReplaceWatchItem(const QUuid& id, ValueItem *new_item);
  signals:
    void AddWatchItemRequested(const QString &expression);
    void ReplaceWatchItemRequested(const QUuid& id, const QString &new_expression);

  private:
    void *placeholder_flag_ = nullptr;
};

class ExpressionWatchWidget : public QWidget
{
    Q_OBJECT
  public:
    ExpressionWatchWidget(EquationCompletionModel* completion_model, QWidget *parent = nullptr);
    ~ExpressionWatchWidget() = default;
    void OnEquationRemoved(const std::string &equation_name);
    void OnEquationUpdated(const Equation *equation, bitmask::bitmask<EquationUpdateFlag> change_type);
    void OnAddExpressionToWatch(const QString &expression);
    void OnEvalResultSubmitted(const QUuid& id, const InterpretResult& result);
    
  signals:
    void ParseResultRequested(const QString& expression, ParseResult &result);
    void EvalResultAsyncRequested(const QUuid& id, const QString& expression);

  protected:
    void SetupUI();
    void SetupConnections();
    static int GetSelectionFlags(const QModelIndexList &indexes, ExpressionWatchModel *model);
    ValueItem *CreateWatchItem(const QString &expression);
    void DeleteWatchItem(const QUuid& id);
    void SetCurrentItemToPlaceholder();
    void OnRequestAddWatchItem(const QString &expression);
    void OnRequestReplaceWatchItem(const QUuid& id, const QString &new_expression);

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
    typedef boost::bimaps::bimap<boost::bimaps::multiset_of<QUuid>, boost::bimaps::multiset_of<std::string>>
        ExpressionItemEquationNameBimap;
    ExpressionWatchModel *model_;
    ValueTreeView *view_;
    EquationCompletionModel* completion_model_;
    CompletionLineEditDelegate *completer_delegate_;
    ExpressionItemEquationNameBimap expression_item_equation_name_bimap_;
    std::map<QUuid, ValueItem::UniquePtr> expression_item_map_;

    // actions
    QAction *copy_action_{};
    QAction *paste_action_{};
    QAction *edit_expression_action_{};
    QAction *delete_watch_action_{};
    QAction *select_all_action_{};
    QAction *clear_all_action_{};
};
} // namespace gui
} // namespace xequation