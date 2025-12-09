#include "variable_model.h"
#include "variable_manager.h"

namespace xequation
{
namespace gui
{

VariableModel::VariableModel(QObject *parent) : QAbstractItemModel(parent) {}

VariableModel::~VariableModel() {}

void VariableModel::SetRootVariable(const QList<Variable *> &variable_list)
{
    beginResetModel();
    root_variable_set_.clear();
    for (Variable *var : variable_list)
    {
        if (var)
            root_variable_set_.insert(var);
    }
    endResetModel();
}

void VariableModel::AddRootVariable(Variable *variable)
{
    if (!variable)
        return;

    int row = root_variable_set_.size();
    beginInsertRows(QModelIndex(), row, row);
    root_variable_set_.insert(variable);
    endInsertRows();
    ConnectVariableManager(variable);
}

void VariableModel::RemoveRootVariable(Variable *variable)
{
    if (!variable)
        return;
    if(!IsContainRootVariable(variable))
        return;
    int row = IndexOfRootVariable(variable);
    DisconnectVariableManager(variable);
    beginRemoveRows(QModelIndex(), row, row);
    root_variable_set_.erase(variable);
    endRemoveRows();
}

void VariableModel::ClearRootVariables()
{
    if (root_variable_set_.empty())
        return;

    beginResetModel();
    root_variable_set_.clear();
    variable_manager_cache_.clear();
    endResetModel();
}

Variable *VariableModel::GetRootVariableAt(int index) const
{
    if (index < 0 || index >= root_variable_set_.size())
        return nullptr;
    return *root_variable_set_.nth(index);
}

int VariableModel::IndexOfRootVariable(Variable *variable) const
{
    auto it = root_variable_set_.find(variable);
    if (it == root_variable_set_.end())
        return -1;
    return std::distance(root_variable_set_.begin(), it);
}

bool VariableModel::IsContainRootVariable(Variable *variable) const
{
    return root_variable_set_.contains(variable);
}

int VariableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 3; // Name, Value, Type
}

int VariableModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return root_variable_set_.size();

    auto *parent_data = GetVariableFromIndex(parent);
    if (!parent_data)
        return 0;
    return parent_data->ChildCount();
}

QModelIndex VariableModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (!parent.isValid())
    {
        if (row < 0 || row >= root_variable_set_.size())
            return QModelIndex();
        return createIndex(row, column, GetRootVariableAt(row));
    }

    auto *parent_variable = GetVariableFromIndex(parent);
    if (!parent_variable)
        return QModelIndex();

    auto *child = parent_variable->GetChildAt(row);
    if (!child)
        return QModelIndex();

    return createIndex(row, column, child);
}

QModelIndex VariableModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();

    auto *child_variable = GetVariableFromIndex(child);
    if (!child_variable)
        return QModelIndex();

    auto *parent_variable = child_variable->parent();
    if (!parent_variable)
        return QModelIndex();

    int root_index = IndexOfRootVariable(parent_variable);
    if (root_index >= 0)
    {
        return createIndex(root_index, 0, parent_variable);
    }

    auto *grand = parent_variable->parent();
    if (!grand)
        return QModelIndex();

    int row = grand->IndexOfChild(parent_variable);
    if (row < 0)
        return QModelIndex();

    return createIndex(row, 0, parent_variable);
}

QVariant VariableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return QVariant();

    auto *d = GetVariableFromIndex(index);
    if (!d)
        return QVariant();

    switch (index.column())
    {
    case 0:
        return d->name();
    case 1:
        return d->value();
    case 2:
        return d->type();
    default:
        return QVariant();
    }
}

QVariant VariableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0:
            return tr("Name");
        case 1:
            return tr("Value");
        case 2:
            return tr("Type");
        default:
            return QVariant();
        }
    }
    return QVariant();
}

void VariableModel::OnVariableChanged(Variable *variable)
{
    QModelIndex begin_index = GetIndexFromVariable(variable);
    QModelIndex end_index = createIndex(begin_index.row(), columnCount() - 1, variable); // Update the entire row
    if (begin_index.isValid() && end_index.isValid())
    {
        emit dataChanged(begin_index, end_index);
    }
}

void VariableModel::OnVariablesChanged(const QList<Variable *> &variables)
{
    if (variables.isEmpty())
        return;

    QMap<QModelIndex, QPair<int, int>> parent_row_ranges;

    foreach (Variable *variable, variables)
    {
        QModelIndex index = GetIndexFromVariable(variable);
        if (!index.isValid())
            continue;

        QModelIndex parent = index.parent();
        int row = index.row();

        if (!parent_row_ranges.contains(parent))
        {
            parent_row_ranges[parent] = qMakePair(row, row);
        }
        else
        {
            QPair<int, int> &range = parent_row_ranges[parent];
            if (row < range.first)
                range.first = row;
            if (row > range.second)
                range.second = row;
        }
    }

    QMap<QModelIndex, QPair<int, int>>::const_iterator it;
    for (it = parent_row_ranges.constBegin(); it != parent_row_ranges.constEnd(); ++it)
    {
        QModelIndex parent = it.key();
        int first_row = it.value().first;
        int last_row = it.value().second;

        QModelIndex top_left = index(first_row, 0, parent);
        QModelIndex bottom_right = index(last_row, columnCount(parent) - 1, parent);
        emit dataChanged(top_left, bottom_right);
    }
}

void VariableModel::ConnectVariableManager(Variable *variable)
{
    if (variable_manager_cache_[variable->manager()].isEmpty())
    {
        connect(variable->manager(), &VariableManager::VariableBeginInsert,
                this, &VariableModel::OnVariableBeginInsert);
        connect(variable->manager(), &VariableManager::VariableEndInsert,
                this, &VariableModel::OnVariableEndInsert);
        connect(variable->manager(), &VariableManager::VariableBeginRemove,
                this, &VariableModel::OnVariableBeginRemove);
        connect(variable->manager(), &VariableManager::VariableEndRemove,
                this, &VariableModel::OnVariableEndRemove);
        connect(variable->manager(), &VariableManager::VariableChanged,
                this, &VariableModel::OnVariableChanged);
        connect(variable->manager(), &VariableManager::VariablesChanged,
                this, &VariableModel::OnVariablesChanged);
    }
    variable_manager_cache_[variable->manager()].append(variable);
}

void VariableModel::DisconnectVariableManager(Variable *variable)
{
    if (!variable_manager_cache_.contains(variable->manager()))
        return;

    variable_manager_cache_[variable->manager()].removeOne(variable);
    if (variable_manager_cache_[variable->manager()].isEmpty())
    {
        disconnect(variable->manager(), &VariableManager::VariableBeginInsert,
                   this, &VariableModel::OnVariableBeginInsert);
        disconnect(variable->manager(), &VariableManager::VariableEndInsert,
                   this, &VariableModel::OnVariableEndInsert);
        disconnect(variable->manager(), &VariableManager::VariableBeginRemove,  
                   this, &VariableModel::OnVariableBeginRemove);
        disconnect(variable->manager(), &VariableManager::VariableEndRemove,
                   this, &VariableModel::OnVariableEndRemove);
        disconnect(variable->manager(), &VariableManager::VariableChanged,
                   this, &VariableModel::OnVariableChanged);
        disconnect(variable->manager(), &VariableManager::VariablesChanged,
                   this, &VariableModel::OnVariablesChanged);
        variable_manager_cache_.remove(variable->manager());
    }
}

void VariableModel::OnVariableBeginInsert(Variable* parent, int index, int count)
{
    QModelIndex parent_index;
    if (parent)
        parent_index = GetIndexFromVariable(parent);

    beginInsertRows(parent_index, index, index + count - 1);
}

void VariableModel::OnVariableEndInsert()
{
    endInsertRows();
}

void VariableModel::OnVariableBeginRemove(Variable* parent, int index, int count)
{
    QModelIndex parent_index;
    if (parent)
        parent_index = GetIndexFromVariable(parent);

    beginRemoveRows(parent_index, index, index + count - 1);
}

void VariableModel::OnVariableEndRemove()
{
    endRemoveRows();
}

Variable *VariableModel::GetVariableFromIndex(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<Variable *>(index.internalPointer()) : nullptr;
}

QModelIndex VariableModel::GetIndexFromVariable(Variable *variable) const
{
    if (!variable)
        return QModelIndex();

    int idx = IndexOfRootVariable(variable);
    if (idx >= 0)
        return createIndex(idx, 0, variable);

    auto *parent_variable = variable->parent();
    if (!parent_variable)
        return QModelIndex();

    int row = parent_variable->IndexOfChild(variable);
    if (row < 0)
        return QModelIndex();

    QModelIndex parent_index = GetIndexFromVariable(parent_variable);
    if (!parent_index.isValid())
        return QModelIndex();

    return createIndex(row, 0, variable);
}

} // namespace gui
} // namespace xequation