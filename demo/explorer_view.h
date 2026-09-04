#pragma once

#include <QTreeView>

#include <map>
#include <tuple>
#include <vector>

#include "core/equation_manager.h"

class QStandardItemModel;
class QStandardItem;
class QMenu;
class QAction;

namespace xresults
{
namespace gui
{

// =========================================================================
// ExplorerView -- live explorer over the REL dataset registry + an
// EquationManager (equations/expressions by tag).
//
// Root children (in order):
//   1. "Datasets" node
//        -> each Dataset (default suffixed "(default)")
//             -> each Block (full final path as a flat node text)
//                  -> each DataArray
//   2. One node per distinct Equation/Expression tag
//        -> the Equations/Expressions carrying that tag
//
// Roles carry the payload so a host can route clicks (dataset name, block
// path, data-array name, tag, equation name, expression id).
// =========================================================================

class ExplorerView : public QTreeView
{
    Q_OBJECT

  public:
    enum class NodeKind
    {
        kGroupDatasets,   // "Datasets" root node
        kDataset,         // one Dataset
        kBlock,           // one Block (full path)
        kDataArray,       // one DataArray within a Block
        kTag,             // one tag node (equations/expressions below)
        kEquation,        // one Equation
        kExpression,      // one Expression
    };

    struct SelectionInfo
    {
        NodeKind kind = NodeKind::kGroupDatasets;
        QString dataset;      // dataset name (kDataset / kBlock / kDataArray)
        QString block_path;   // full block path (kBlock / kDataArray)
        QString data_array;   // data-array name (kDataArray)
        QString tag;          // tag string (kTag / kEquation / kExpression)
        QString name;         // equation name / expression display text
        xequation::ObjectId object_id = xequation::NilObjectId();  // kEquation/kExpression; kDataArray after first access
    };

    static constexpr int kRoleKind = Qt::UserRole + 1;
    static constexpr int kRoleDataset = Qt::UserRole + 2;
    static constexpr int kRoleBlock = Qt::UserRole + 3;
    static constexpr int kRoleDataArray = Qt::UserRole + 4;
    static constexpr int kRoleTag = Qt::UserRole + 5;
    static constexpr int kRoleName = Qt::UserRole + 6;
    static constexpr int kRoleObjectId = Qt::UserRole + 7;

    explicit ExplorerView(xequation::EquationManager &manager,
                          QWidget *parent = nullptr);
    ~ExplorerView() override;

    /// Rebuild the tree from the manager + dataset registry.
    void Refresh();

    /// Payload of the current (selected) item.
    SelectionInfo CurrentSelection() const;

    /// Payloads of every selected item, in tree order (top to bottom).  With
    /// multi-select enabled this is the full set a host should act on.
    std::vector<SelectionInfo> SelectedInfos() const;

    /// Payload of an arbitrary item.
    static SelectionInfo ItemInfo(const QStandardItem *item);

    /// Replace the entire tree selection with exactly the given equation
    /// leaves (multi-select friendly; tag groups of selected leaves are
    /// expanded).  Used when the equation LIST is the last-clicked panel: the
    /// tree then mirrors only the list's equation selection, so any stale
    /// dataset / data-array selection is dropped.
    void SetEquationSelection(const std::vector<QString> &equation_names);

    /// Returns (creating + registering on first use, then caching on the node)
    /// the hidden "DataArray access" expression id for a DataArray.  The
    /// expression content is the REL access path (`ds.block…array`); its tag is
    /// hidden so the tree never lists it under a tag group.  Later clicks on
    /// the same array reuse the cached id.  Nil when registration fails.
    xequation::ObjectId GetDataArrayExpression(const QString &dataset,
                                               const QString &block_path,
                                               const QString &data_array);

  private:
    QStandardItem *AddGroupChild(QStandardItem *parent, NodeKind kind,
                                 const QString &text);
    void AddTaggedItems();
    /// Find the DataArray node for (dataset, block path, data array), if shown.
    QStandardItem *FindDataArrayItem(const QString &dataset,
                                     const QString &block_path,
                                     const QString &data_array) const;
    /// Drop cached DataArray-access expressions whose (dataset, block, array)
    /// no longer exists in the registry (env reload / removal).  Called from
    /// Refresh(); the expression is removed from the manager.
    void CleanupDataArrayExpressions();

    /// Show the shared context menu at the given viewport position.  One
    /// menu (built once) is reused for every tree item; the Delete action is
    /// enabled only for deletable Equation / Expression leaves.
    void OnShowContextMenu(const QPoint &pos);

  private:
    xequation::EquationManager &manager_;
    QStandardItemModel *model_ = nullptr;

    /// (dataset, block path, data array) -> lazily-created hidden access
    /// expression id.  Survives tree rebuilds; CleanupDataArrayExpressions()
    /// removes entries whose array is gone (env reload / dataset removal).
    std::map<std::tuple<QString, QString, QString>, xequation::ObjectId>
        data_array_exprs_;
    bool refreshing_ = false;  // nested-Refresh guard (prune triggers expr-removed)

    /// The item the menu was opened for (the right-clicked item).  The Delete
    /// action acts on this precise item, not the (possibly multi-)selection.
    SelectionInfo context_target_;

    // Shared context menu: built once, shown for every tree item.  The Delete
    // action's enabled state is updated per item before showing.
    QMenu *context_menu_ = nullptr;
    QAction *delete_action_ = nullptr;

    // Scoped manager signal subscriptions (auto-disconnect on destruction).
    xequation::ScopedConnection eq_added_conn_;
    xequation::ScopedConnection eq_removing_conn_;
    xequation::ScopedConnection eq_removed_conn_;
    xequation::ScopedConnection eq_updated_conn_;
    xequation::ScopedConnection expr_added_conn_;
    xequation::ScopedConnection expr_removing_conn_;
    xequation::ScopedConnection expr_removed_conn_;
};

} // namespace gui
} // namespace xresults
