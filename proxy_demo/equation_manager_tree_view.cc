#include "equation_manager_tree_view.h"

#include <QDateTime>
#include <QHeaderView>
#include <QStandardItemModel>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "environment.h"   // rel::Environment (dataset registry)
#include "dataset.h"       // xdataset::Dataset

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
                        const QString &object_id = QString())
{
    auto *item = new QStandardItem(text);
    item->setData(static_cast<int>(kind), EquationManagerTreeView::kRoleKind);
    item->setData(dataset, EquationManagerTreeView::kRoleDataset);
    item->setData(block, EquationManagerTreeView::kRoleBlock);
    item->setData(array, EquationManagerTreeView::kRoleDataArray);
    item->setData(tag, EquationManagerTreeView::kRoleTag);
    item->setData(name, EquationManagerTreeView::kRoleName);
    item->setData(object_id, EquationManagerTreeView::kRoleObjectId);
    item->setEditable(false);
    return item;
}
} // namespace

EquationManagerTreeView::EquationManagerTreeView(EquationManager &manager, QWidget *parent)
    : QTreeView(parent), manager_(manager)
{
    model_ = new QStandardItemModel(this);
    setModel(model_);
    setHeaderHidden(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
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
        [refresh](const Expression *) { refresh(); });
    expr_removing_conn_ = manager_.signals_manager().ConnectScoped<EquationEvent::kExpressionRemoving>(
        [refresh](const Expression *) { refresh(); });
    expr_removed_conn_ = manager_.signals_manager().ConnectScoped<EquationEvent::kExpressionRemoved>(
        [refresh](const std::string &) { refresh(); });

    Refresh();
}

EquationManagerTreeView::~EquationManagerTreeView() = default;

void EquationManagerTreeView::Refresh()
{
    // Preserve the expanded path of the datasets group across refreshes.
    const bool datasets_expanded = model_->invisibleRootItem()->hasChildren()
        ? isExpanded(model_->invisibleRootItem()->child(0)->index())
        : true;

    model_->clear();

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

    expand(datasets_group->index());
    if (!datasets_expanded)
    {
        collapse(datasets_group->index());
    }
}

void EquationManagerTreeView::AddTaggedItems()
{
    // Collect items per tag, equations first then expressions.
    std::map<std::string, std::vector<std::pair<NodeKind, const void *>>> by_tag;

    const std::vector<std::string> eq_names = manager_.GetEquationNames();
    for (const std::string &name : eq_names)
    {
        const Equation *equation = manager_.GetEquation(name);
        if (!equation)
        {
            continue;
        }
        by_tag[equation->tag.empty() ? kEquationTagDefault : equation->tag].push_back(
            {NodeKind::kEquation, equation});
    }

    const std::vector<ObjectId> expr_ids = manager_.GetExpressionIds();
    for (const ObjectId &id : expr_ids)
    {
        const Expression *expression = manager_.GetExpression(id);
        if (!expression)
        {
            continue;
        }
        by_tag[expression->tag.empty() ? kWatchTagDefault : expression->tag].push_back(
            {NodeKind::kExpression, expression});
    }

    for (const auto &entry : by_tag)
    {
        QStandardItem *tag_item = MakeItem(
            NodeKind::kTag, QString::fromStdString(entry.first), QString(), QString(),
            QString(), QString::fromStdString(entry.first));
        model_->appendRow(tag_item);

        for (const auto &kind_item : entry.second)
        {
            if (kind_item.first == NodeKind::kEquation)
            {
                const auto *equation = static_cast<const Equation *>(kind_item.second);
                tag_item->appendRow(MakeItem(
                    NodeKind::kEquation, QString::fromStdString(equation->name),
                    QString(), QString(), QString(), QString::fromStdString(entry.first),
                    QString::fromStdString(equation->name),
                    QString::fromStdString(boost::uuids::to_string(equation->id))));
            }
            else
            {
                const auto *expression = static_cast<const Expression *>(kind_item.second);
                tag_item->appendRow(MakeItem(
                    NodeKind::kExpression, QString::fromStdString(expression->content),
                    QString(), QString(), QString(), QString::fromStdString(entry.first),
                    QString::fromStdString(expression->content),
                    QString::fromStdString(boost::uuids::to_string(expression->id))));
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
    info.object_id = item->data(kRoleObjectId).toString();
    return info;
}

void EquationManagerTreeView::SelectEquation(const QString &equation_name)
{
    // Search the tag leaves for an equation with this name.
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
            if (ItemInfo(leaf).kind == NodeKind::kEquation &&
                leaf->data(kRoleName).toString() == equation_name)
            {
                const QModelIndex idx = leaf->index();
                setCurrentIndex(idx);
                scrollTo(idx);
                return;
            }
        }
    }
}

} // namespace gui
} // namespace xresults
