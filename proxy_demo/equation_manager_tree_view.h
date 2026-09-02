#pragma once

#include <QTreeView>

#include "core/equation_manager.h"

class QStandardItemModel;
class QStandardItem;

namespace xresults
{
namespace gui
{

// =========================================================================
// EquationManagerTreeView -- live tree over an EquationManager + the REL
// dataset registry.
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

class EquationManagerTreeView : public QTreeView
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
        QString object_id;    // uuid string (kEquation / kExpression)
    };

    static constexpr int kRoleKind = Qt::UserRole + 1;
    static constexpr int kRoleDataset = Qt::UserRole + 2;
    static constexpr int kRoleBlock = Qt::UserRole + 3;
    static constexpr int kRoleDataArray = Qt::UserRole + 4;
    static constexpr int kRoleTag = Qt::UserRole + 5;
    static constexpr int kRoleName = Qt::UserRole + 6;
    static constexpr int kRoleObjectId = Qt::UserRole + 7;

    explicit EquationManagerTreeView(xequation::EquationManager &manager,
                                     QWidget *parent = nullptr);
    ~EquationManagerTreeView() override;

    /// Rebuild the tree from the manager + dataset registry.
    void Refresh();

    /// Payload of the current (selected) item.
    SelectionInfo CurrentSelection() const;

    /// Payload of an arbitrary item.
    static SelectionInfo ItemInfo(const QStandardItem *item);

    /// Highlight + reveal the item for an equation name (used after rename
    /// etc.); -1 when the equation is not listed.
    void SelectEquation(const QString &equation_name);

  private:
    QStandardItem *AddGroupChild(QStandardItem *parent, NodeKind kind,
                                 const QString &text);
    void AddTaggedItems();

  private:
    xequation::EquationManager &manager_;
    QStandardItemModel *model_ = nullptr;

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
