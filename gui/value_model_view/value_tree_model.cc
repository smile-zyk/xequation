#include "value_tree_model.h"
#include "value_item_builder.h"
#include <QVariant>

namespace xequation
{
namespace gui
{

ValueTreeModel::ValueTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

ValueTreeModel::~ValueTreeModel() = default;

QModelIndex ValueTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column < 0)
        return QModelIndex();

    if (!parent.isValid())
    {
        if (row >= root_items_.size() || column >= columnCount())
            return QModelIndex();
        ValueItem* child = root_items_[row];
        if (!child)
            return QModelIndex();
        return createIndex(row, column, child);
    }

    ValueItem* parentItem = GetValueItemFromIndex(parent);
    if (!parentItem)
        return QModelIndex();

    if (!parentItem->is_loaded())
        return QModelIndex();

    ValueItem* child = parentItem->GetChildAt(row);
    if (!child || column >= columnCount())
        return QModelIndex();

    return createIndex(row, column, child);
}

QModelIndex ValueTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();

    ValueItem* childItem = GetValueItemFromIndex(child);
    if (!childItem)
        return QModelIndex();

    ValueItem* p = childItem->parent();
    if (!p)
        return QModelIndex();

    if (!p->parent())
    {
        int row = root_items_.indexOf(p);
        if (row < 0)
            return QModelIndex();
        return createIndex(row, 0, p);
    }

    ValueItem* grand = p->parent();
    int row = grand ? grand->GetIndexOfChild(p) : -1;
    if (row < 0)
        return QModelIndex();
    return createIndex(row, 0, p);
}

int ValueTreeModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
    {
        return root_items_.size();
    }

    ValueItem* item = GetValueItemFromIndex(parent);
    if (!item)
        return 0;

    if (!item->is_loaded())
    {
        return item->has_children() ? 1 : 0;
    }

    return static_cast<int>(item->ChildCount());
}

int ValueTreeModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 3;
}

QVariant ValueTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    ValueItem* item = GetValueItemFromIndex(index);
    if (!item)
        return QVariant();

    switch (index.column())
    {
    case 0: return item->name();
    case 1: return item->display_value();
    case 2: return item->type();
    default: return QVariant();
    }
}

QVariant ValueTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section)
    {
    case 0: return QStringLiteral("Name");
    case 1: return QStringLiteral("Value");
    case 2: return QStringLiteral("Type");
    default: return QVariant();
    }
}

bool ValueTreeModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return !root_items_.isEmpty();

    ValueItem* item = GetValueItemFromIndex(parent);
    if (!item)
        return false;

    return item->HasChildren();
}

bool ValueTreeModel::canFetchMore(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return false;

    ValueItem* item = GetValueItemFromIndex(parent);
    if (!item)
        return false;

    return item->has_children() && !item->is_loaded();
}

void ValueTreeModel::fetchMore(const QModelIndex& parent)
{
    ValueItem* item = GetValueItemFromIndex(parent);
    if (!item || item->is_loaded())
        return;

    size_t expected_count = item->has_children();
    if (expected_count == 0)
        return;

    beginInsertRows(parent, 0, static_cast<int>(expected_count) - 1);
    item->LoadChildren();
    endInsertRows();
}

void ValueTreeModel::AddRootItem(ValueItem* item)
{
    if (!item)
        return;

    int row = root_items_.size();
    beginInsertRows(QModelIndex(), row, row);
    root_items_.push_back(item);
    endInsertRows();
}

void ValueTreeModel::RemoveRootItem(ValueItem* item)
{
    if (!item)
        return;

    int index = root_items_.indexOf(item);
    if (index >= 0)
    {
        beginRemoveRows(QModelIndex(), index, index);
        root_items_.removeAt(index);
        endRemoveRows();
    }
}

void ValueTreeModel::SetRootItems(const QVector<ValueItem*>& items)
{
    beginResetModel();
    root_items_ = items;
    endResetModel();
}

void ValueTreeModel::Clear()
{
    beginResetModel();
    root_items_.clear();
    endResetModel();
}

ValueItem* ValueTreeModel::GetValueItemFromIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;
    return static_cast<ValueItem*>(index.internalPointer());
}

QModelIndex ValueTreeModel::GetIndexFromValueItem(ValueItem *item) const
{
    if (!item)
        return QModelIndex();

    if (!item->parent())
    {
        int row = root_items_.indexOf(item);
        if (row < 0)
            return QModelIndex();
        return createIndex(row, 0, item);
    }

    ValueItem* parent = item->parent();
    int row = parent->GetIndexOfChild(item);
    if (row < 0)
        return QModelIndex();
    return createIndex(row, 0, item);
}

} // namespace gui
} // namespace xequation