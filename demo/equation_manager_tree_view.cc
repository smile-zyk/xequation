#include "equation_manager_tree_view.h"

#include <QContextMenuEvent>
#include <QDateTime>
#include <QHeaderView>
#include <QMenu>
#include <QMetaType>
#include <QStandardItemModel>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "environment.h"   // rel::Environment (dataset registry)
#include "dataset.h"       // xdataset::Dataset
#include "tree_view_tag.h"  // UI-layer tag definitions

// Allow QStandardItem roles (QVariant) to carry the raw ObjectId so the tree
// does not round-trip through a uuid string.
Q_DECLARE_METATYPE(xequation::ObjectId)

namespace xresults
{
namespace gui
{

using namespace xequation;

namespace
{
QStandardItem *MakeItem(EquationManagerTreeView::NodeKind kind, const QString &text,
                        const QString &dataset = QString(),
                        const QString &block = QString(),
                        const QString &array = QString(),
                        const QString &tag = QString(),
                        const QString &name = QString(),
                        const xequation::ObjectId &object_id = xequation::ObjectId())
{
    auto *item = new QStandardItem(text);
    item->setData(static_cast<int>(kind), EquationManagerTreeView::kRoleKind);
    item->setData(dataset, EquationManagerTreeView::kRoleDataset);
    item->setData(block, EquationManagerTreeView::kRoleBlock);
    item->setData(array, EquationManagerTreeView::kRoleDataArray);
    item->setData(tag, EquationManagerTreeView::kRoleTag);
    item->setData(name, EquationManagerTreeView::kRoleName);
    item->setData(QVariant::fromValue(object_id), EquationManagerTreeView::kRoleObjectId);
    item->setEditable(false);
    return item;
}

/// True when a tree node may be deleted by the user.  Only an Equation or
/// Expression leaf under a tag group is deletable; Datasets (and its Block /
/// DataArray nodes) and the Tag group nodes themselves are structural &
/// read-only and are never offered for deletion.
bool IsDeletableNodeKind(EquationManagerTreeView::NodeKind kind)
{
    return kind == EquationManagerTreeView::NodeKind::kEquation ||
           kind == EquationManagerTreeView::NodeKind::kExpression;
}
} // namespace

EquationManagerTreeView::EquationManagerTreeView(EquationManager &manager, QWidget *parent)
    : QTreeView(parent), manager_(manager)
{
    model_ = new QStandardItemModel(this);
    setModel(model_);
    setHeaderHidden(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setUniformRowHeights(true);
    setAnimated(true);

    // Live refresh: equation/expression structure changes rebuild the tree.
    auto refresh = [this]() { Refresh(); };
    eq_added_conn_ = manager_.signals_manager().ConnectScoped<EquationEvent::kEquationAdded>(
        [refresh](const Equation *) { refresh(); });
    eq_removing_conn_ = manager_.signals_manager().ConnectScoped<EquationEvent::kEquationRemoving>(
        [refresh](const Equation *) { refresh(); });
    eq_removed_conn_ = manager_.signals_manager().ConnectScoped<EquationEvent::kEquationRemoved>(
        [refresh](const std::string &) { refresh(); });
    // Renames emit kEquationUpdated(kName); values change often, so only
    // refresh when the update carries kName.
    eq_updated_conn_ = manager_.signals_manager().ConnectScoped<EquationEvent::kEquationUpdated>(
        [refresh](const Equation *, bitmask::bitmask<EquationUpdateFlag> flags) {
            if (flags & EquationUpdateFlag::kName)
            {
                refresh();
            }
        });
    expr_added_conn_ = manager_.signals_manager().ConnectScoped<EquationEvent::kExpressionAdded>(
        [this, refresh](const Expression *expr) {
            if (expr && IsDataArrayAccessTag(expr->tag))
            {
                return;  // invisible: no tag group / no tree change
            }
            refresh();
        });
    expr_removing_conn_ = manager_.signals_manager().ConnectScoped<EquationEvent::kExpressionRemoving>(
        [this](const Expression *expr) {
            if (!expr || !IsDataArrayAccessTag(expr->tag))
            {
                return;
            }
            // Remove the cache entry if this was one of ours; the DataArray
            // node stays visible and re-creates the expression lazily.
            for (auto it = data_array_exprs_.begin(); it != data_array_exprs_.end(); ++it)
            {
                if (it->second == expr->id)
                {
                    data_array_exprs_.erase(it);
                    break;
                }
            }
        });
    expr_removed_conn_ = manager_.signals_manager().ConnectScoped<EquationEvent::kExpressionRemoved>(
        [refresh](const std::string &) { refresh(); });

    // Right-click on an Equation / Expression leaf: delete it.  Dataset /
    // Block / DataArray and Tag group nodes are structural & read-only -- they
    // are never offered for deletion.
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
                const QModelIndex index = indexAt(pos);
                const QStandardItem *item = index.isValid()
                    ? model_->itemFromIndex(index)
                    : nullptr;
                if (!item)
                {
                    return;
                }
                const SelectionInfo info = ItemInfo(item);
                if (!IsDeletableNodeKind(info.kind))
                {
                    return;
                }
                const ObjectId id = info.object_id;
                if (id.is_nil())
                {
                    return;
                }
                QMenu menu(this);
                QAction *remove = nullptr;
                if (info.kind == NodeKind::kEquation)
                {
                    remove = menu.addAction(tr("Delete Equation"));
                }
                else
                {
                    remove = menu.addAction(tr("Delete Expression"));
                }
                if (menu.exec(viewport()->mapToGlobal(pos)) == remove)
                {
                    if (info.kind == NodeKind::kEquation)
                    {
                        manager_.RemoveEquation(id);
                    }
                    else
                    {
                        manager_.RemoveExpression(id);
                    }
                }
            });

    Refresh();
}

EquationManagerTreeView::~EquationManagerTreeView() = default;

void EquationManagerTreeView::Refresh()
{
    if (refreshing_)
    {
        return;  // prune → expr-removed → Refresh() re-entry guard
    }
    refreshing_ = true;

    // Preserve the expanded path of the datasets group across refreshes.
    const bool datasets_expanded = model_->invisibleRootItem()->hasChildren()
        ? isExpanded(model_->invisibleRootItem()->child(0)->index())
        : true;

    // Rebuilding the model clears the selection; that is a programmatic reset
    // (manager change / env reload), NOT a user deselection -- suppress the
    // selectionChanged noise so hosts do not treat it as user input.
    QSignalBlocker blocker(selectionModel());
    model_->clear();
    blocker.unblock();

    // ---- 1. Datasets subtree ----------------------------------------------
    QStandardItem *datasets_group = MakeItem(NodeKind::kGroupDatasets, tr("Datasets"));
    model_->appendRow(datasets_group);

    std::vector<std::string> dataset_names = rel::Environment::DatasetNames();
    std::sort(dataset_names.begin(), dataset_names.end());

    std::string default_name;
    if (rel::Environment::DefaultDataset())
    {
        default_name = rel::Environment::DefaultDataset()->name();
    }

    for (const std::string &ds_name : dataset_names)
    {
        xdataset::Dataset *ds = rel::Environment::FindDataset(ds_name);
        if (!ds)
        {
            continue;
        }
        QString text = QString::fromStdString(ds_name);
        if (ds_name == default_name)
        {
            text += tr(" (default)");
        }
        QStandardItem *dataset_item =
            MakeItem(NodeKind::kDataset, text, QString::fromStdString(ds_name));
        datasets_group->appendRow(dataset_item);

        // Blocks: the *final* block path is listed flat (no per-level nodes).
        std::vector<std::string> block_paths = ds->GetAllBlockPaths();
        std::sort(block_paths.begin(), block_paths.end());
        for (const std::string &path : block_paths)
        {
            QStandardItem *block_item = MakeItem(
                NodeKind::kBlock, QString::fromStdString(path),
                QString::fromStdString(ds_name), QString::fromStdString(path));
            dataset_item->appendRow(block_item);

            std::vector<std::string> arrays = ds->GetDataArrayNames(path);
            for (const std::string &array : arrays)
            {
                block_item->appendRow(MakeItem(
                    NodeKind::kDataArray, QString::fromStdString(array),
                    QString::fromStdString(ds_name), QString::fromStdString(path),
                    QString::fromStdString(array)));
            }
        }
    }

    // ---- 2. Equations / Expressions by Tag ---------------------------------
    AddTaggedItems();

    // ---- 3. DataArray access-expression roles ------------------------------
    // Re-stamp cached access-expression ids onto the freshly built nodes;
    // drop (unregister) entries whose array no longer exists in the env.
    CleanupDataArrayExpressions();

    expand(datasets_group->index());
    if (!datasets_expanded)
    {
        collapse(datasets_group->index());
    }
    refreshing_ = false;
}

void EquationManagerTreeView::AddTaggedItems()
{
    // Collect per tag, keeping the two types separate (Equation / Expression
    // have no common base) and equations before expressions inside each group.
    std::map<std::string, std::vector<const Equation *>> eq_by_tag;
    std::map<std::string, std::vector<const Expression *>> ex_by_tag;

    const std::vector<std::string> eq_names = manager_.GetEquationNames();
    for (const std::string &name : eq_names)
    {
        const Equation *equation = manager_.GetEquation(name);
        if (!equation)
        {
            continue;
        }
        eq_by_tag[equation->tag.empty() ? kEquationTagDefault : equation->tag].push_back(
            equation);
    }

    const std::vector<ObjectId> expr_ids = manager_.GetExpressionIds();
    for (const ObjectId &id : expr_ids)
    {
        const Expression *expression = manager_.GetExpression(id);
        if (!expression || IsDataArrayAccessTag(expression->tag))
        {
            continue;  // DataArray access expressions are never shown
        }
        ex_by_tag[expression->tag.empty() ? kWatchTagDefault : expression->tag].push_back(
            expression);
    }

    // Union of tag keys, sorted (std::map iterates in key order).
    std::set<std::string> tags;
    for (const auto &entry : eq_by_tag)
    {
        tags.insert(entry.first);
    }
    for (const auto &entry : ex_by_tag)
    {
        tags.insert(entry.first);
    }

    for (const std::string &tag : tags)
    {
        QStandardItem *tag_item = MakeItem(
            NodeKind::kTag, QString::fromStdString(tag), QString(), QString(),
            QString(), QString::fromStdString(tag));
        model_->appendRow(tag_item);

        const auto eq_it = eq_by_tag.find(tag);
        if (eq_it != eq_by_tag.end())
        {
            for (const Equation *equation : eq_it->second)
            {
                tag_item->appendRow(MakeItem(
                    NodeKind::kEquation, QString::fromStdString(equation->name),
                    QString(), QString(), QString(), QString::fromStdString(tag),
                    QString::fromStdString(equation->name),
                    equation->id));
            }
        }

        const auto ex_it = ex_by_tag.find(tag);
        if (ex_it != ex_by_tag.end())
        {
            for (const Expression *expression : ex_it->second)
            {
                tag_item->appendRow(MakeItem(
                    NodeKind::kExpression, QString::fromStdString(expression->content),
                    QString(), QString(), QString(), QString::fromStdString(tag),
                    QString::fromStdString(expression->content),
                    expression->id));
            }
        }
    }
}

QStandardItem *EquationManagerTreeView::AddGroupChild(QStandardItem *parent,
                                                      NodeKind kind,
                                                      const QString &text)
{
    auto *item = MakeItem(kind, text);
    if (parent)
    {
        parent->appendRow(item);
    }
    else
    {
        model_->appendRow(item);
    }
    return item;
}

EquationManagerTreeView::SelectionInfo EquationManagerTreeView::CurrentSelection() const
{
    const QModelIndex index = currentIndex();
    if (!index.isValid())
    {
        return {};
    }
    return ItemInfo(model_->itemFromIndex(index));
}

std::vector<EquationManagerTreeView::SelectionInfo> EquationManagerTreeView::SelectedInfos() const
{
    std::vector<SelectionInfo> infos;

    // Depth-first, left-to-right over the (shallow) tree so the result is in
    // a stable visual order regardless of how Qt reports the selection.
    struct Frame
    {
        const QStandardItem *item;
        int child;
    };
    std::vector<Frame> stack;
    stack.push_back({model_->invisibleRootItem(), 0});
    while (!stack.empty())
    {
        Frame &frame = stack.back();
        if (frame.child >= frame.item->rowCount())
        {
            stack.pop_back();
            continue;
        }
        const QStandardItem *child = frame.item->child(frame.child);
        ++frame.child;
        if (!child)
        {
            continue;
        }
        // Rows are selected as whole rows; column 0 carries the data.
        if (selectionModel()->isSelected(child->index()))
        {
            infos.push_back(ItemInfo(child));
        }
        stack.push_back({child, 0});
    }
    return infos;
}

EquationManagerTreeView::SelectionInfo EquationManagerTreeView::ItemInfo(const QStandardItem *item)
{
    SelectionInfo info;
    if (!item)
    {
        return info;
    }
    info.kind = static_cast<NodeKind>(item->data(kRoleKind).toInt());
    info.dataset = item->data(kRoleDataset).toString();
    info.block_path = item->data(kRoleBlock).toString();
    info.data_array = item->data(kRoleDataArray).toString();
    info.tag = item->data(kRoleTag).toString();
    info.name = item->data(kRoleName).toString();
    info.object_id = item->data(kRoleObjectId).value<xequation::ObjectId>();
    return info;
}

void EquationManagerTreeView::SetEquationSelection(
    const std::vector<QString> &equation_names)
{    // Desired equation leaves (search the tag groups).
    QItemSelection eq_selection;
    QModelIndex first_equation;
    for (int row = 0; row < model_->rowCount(); ++row)
    {
        QStandardItem *item = model_->item(row);
        if (!item)
        {
            continue;
        }
        for (int child_row = 0; child_row < item->rowCount(); ++child_row)
        {
            QStandardItem *leaf = item->child(child_row);
            if (!leaf)
            {
                continue;
            }
            if (ItemInfo(leaf).kind != NodeKind::kEquation)
            {
                continue;
            }
            const QString leaf_name = leaf->data(kRoleName).toString();
            const bool wanted = std::find(equation_names.begin(), equation_names.end(),
                                          leaf_name) != equation_names.end();
            if (!wanted)
            {
                continue;
            }
            // A programmatic selection inside a collapsed group is invisible.
            if (QStandardItem *tag_item = leaf->parent())
            {
                expand(tag_item->index());
            }
            const QModelIndex idx = leaf->index();
            eq_selection.select(idx, idx);
            if (!first_equation.isValid())
            {
                first_equation = idx;
            }
        }
    }

    // Replace the whole selection with the equation leaves.
    selectionModel()->select(eq_selection, QItemSelectionModel::ClearAndSelect);

    if (first_equation.isValid())
    {
        setCurrentIndex(first_equation);
        scrollTo(first_equation);
    }
    else
    {
        setCurrentIndex(QModelIndex());
    }
}

// =========================================================================
// DataArray hidden "access" expressions
// =========================================================================

namespace
{
/// REL path text that evaluates to the given dataset array, e.g.
/// `LNA.a.b.Id.i` (dataset + block path with '/'->'.' + array).  Registration
/// (parse) succeeds whenever the reference is well-formed; evaluation happens
/// against the environment that owns the datasets.
QString MakeDataArrayAccessPath(const QString &dataset, const QString &block_path,
                                const QString &data_array)
{
    QString dots = block_path;
    dots.replace(QLatin1Char('/'), QLatin1Char('.'));
    return dataset + QLatin1Char('.') + dots + QLatin1Char('.') + data_array;
}

/// True when the environment registry still has the (dataset, block, array).
bool HasArrayInEnvironment(const QString &dataset, const QString &block_path,
                           const QString &data_array)
{
    xdataset::Dataset *ds = rel::Environment::FindDataset(dataset.toStdString());
    if (!ds)
    {
        return false;
    }
    if (!ds->IsLeaf(block_path.toStdString()))
    {
        return false;
    }
    const std::vector<std::string> arrays = ds->GetDataArrayNames(block_path.toStdString());
    return std::find(arrays.begin(), arrays.end(), data_array.toStdString()) != arrays.end();
}
} // namespace

ObjectId EquationManagerTreeView::GetDataArrayExpression(
    const QString &dataset, const QString &block_path, const QString &data_array)
{
    const auto key = std::make_tuple(dataset, block_path, data_array);

    // Reuse the cached expression while it is still registered.
    const auto it = data_array_exprs_.find(key);
    if (it != data_array_exprs_.end() && manager_.IsExpressionExist(it->second))
    {
        // Keep the node role in sync (Refresh() re-stamps, but the item may
        // be fresh from a rebuild).
        return it->second;
    }

    // (Re-)create the hidden access expression.  Content = dataset path.
    const QString content = MakeDataArrayAccessPath(dataset, block_path, data_array);
    if (content.isEmpty())
    {
        return NilObjectId();
    }
    const std::string tag = kDataArrayAccessTag;

    ObjectId id = NilObjectId();
    try
    {
        id = manager_.AddExpression(content.toStdString(), tag);
    }
    catch (const std::exception &)
    {
        id = NilObjectId();
    }
    if (id.is_nil())
    {
        return NilObjectId();
    }

    data_array_exprs_[key] = id;
    // Stamp the freshly built / existing DataArray node so SelectionInfo
    // carries the id on the next click.
    QStandardItem *array_item = FindDataArrayItem(dataset, block_path, data_array);
    if (array_item)
    {
        array_item->setData(QVariant::fromValue(id), kRoleObjectId);
    }
    return id;
}

QStandardItem *EquationManagerTreeView::FindDataArrayItem(
    const QString &dataset, const QString &block_path,
    const QString &data_array) const
{
    // Datasets group is the first root child; dataset -> block -> array.
    QStandardItem *datasets_group = model_->item(0);
    if (!datasets_group)
    {
        return nullptr;
    }
    for (int d = 0; d < datasets_group->rowCount(); ++d)
    {
        QStandardItem *ds_item = datasets_group->child(d);
        if (!ds_item || ds_item->data(kRoleDataset).toString() != dataset)
        {
            continue;
        }
        for (int b = 0; b < ds_item->rowCount(); ++b)
        {
            QStandardItem *block_item = ds_item->child(b);
            if (!block_item || block_item->data(kRoleBlock).toString() != block_path)
            {
                continue;
            }
            for (int a = 0; a < block_item->rowCount(); ++a)
            {
                QStandardItem *array_item = block_item->child(a);
                if (array_item &&
                    array_item->data(kRoleDataArray).toString() == data_array)
                {
                    return array_item;
                }
            }
        }
    }
    return nullptr;
}

void EquationManagerTreeView::CleanupDataArrayExpressions()
{
    std::vector<std::tuple<QString, QString, QString>> gone;
    for (const auto &entry : data_array_exprs_)
    {
        const auto &key = entry.first;
        if (!HasArrayInEnvironment(std::get<0>(key), std::get<1>(key), std::get<2>(key)))
        {
            gone.push_back(key);
        }
    }
    for (const auto &key : gone)
    {
        const auto it = data_array_exprs_.find(key);
        if (it == data_array_exprs_.end())
        {
            continue;
        }
        const ObjectId id = it->second;
        data_array_exprs_.erase(it);
        if (manager_.IsExpressionExist(id))
        {
            manager_.RemoveExpression(id);
        }
    }

    // Re-stamp surviving ids onto the freshly rebuilt DataArray nodes so
    // SelectionInfo carries them without an extra click round-trip.
    for (const auto &entry : data_array_exprs_)
    {
        const auto &key = entry.first;
        QStandardItem *array_item =
            FindDataArrayItem(std::get<0>(key), std::get<1>(key), std::get<2>(key));
        if (array_item)
        {
            array_item->setData(QVariant::fromValue(entry.second), kRoleObjectId);
        }
    }
}

} // namespace gui
} // namespace xresults
