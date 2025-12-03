#include "variable_model.h"
#include "variable_manager.h"

namespace xequation
{
namespace gui
{

VariableModel::VariableModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

VariableModel::~VariableModel()
{
}

void VariableModel::SetRootVariable(const QList<Variable *> &variable_list)
{
    beginResetModel();
    root_variable_list_ = variable_list;
    endResetModel();
}

void VariableModel::AddRootVariable(Variable *variable)
{
    if (!variable) return;
    int row = root_variable_list_.size();
    beginInsertRows(QModelIndex(), row, row);
    root_variable_list_.append(variable);
    VariableInserted(variable);
    endInsertRows();
}

void VariableModel::RemoveRootVariable(Variable *variable)
{
    if (!variable) return;
    int row = root_variable_list_.indexOf(variable);
    if (row < 0) return;

    beginRemoveRows(QModelIndex(), row, row);
    root_variable_list_.removeAt(row);
    endRemoveRows();
}

void VariableModel::ClearRootVariables()
{
    if (root_variable_list_.isEmpty()) return;
    
    beginResetModel();
    root_variable_list_.clear();
    endResetModel();
}

Variable *VariableModel::GetRootVariableAt(int index) const
{
    if (index < 0 || index >= root_variable_list_.size()) return nullptr;
    return root_variable_list_[index];
}

int VariableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 3; // Name, Value, Type
}

int VariableModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return root_variable_list_.size();

    auto *parent_data = GetVariableFromIndex(parent);
    if (!parent_data) return 0;
    return parent_data->ChildCount();
}

QModelIndex VariableModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return QModelIndex();

    if (!parent.isValid())
    {
        if (row < 0 || row >= root_variable_list_.size()) return QModelIndex();
        return createIndex(row, column, root_variable_list_[row]);
    }

    auto *parent_data = GetVariableFromIndex(parent);
    if (!parent_data) return QModelIndex();

    auto *child = parent_data->GetChildAt(row);
    if (!child) return QModelIndex();

    return createIndex(row, column, child);
}

QModelIndex VariableModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) return QModelIndex();

    auto *child_data = GetVariableFromIndex(child);
    if (!child_data) return QModelIndex();

    auto *parent_data = child_data->parent();
    if (!parent_data) return QModelIndex();

    for (int i = 0; i < root_variable_list_.size(); ++i)
    {
        if (root_variable_list_[i] == parent_data)
            return createIndex(i, 0, parent_data);
    }

    auto *grand = parent_data->parent();
    if (!grand) return QModelIndex();

    int row = RowOfChildInParent(grand, parent_data);
    if (row < 0) return QModelIndex();

    return createIndex(row, 0, parent_data);
}

QVariant VariableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();
    if (role != Qt::DisplayRole && role != Qt::EditRole) return QVariant();

    auto *d = GetVariableFromIndex(index);
    if (!d) return QVariant();

    switch (index.column())
    {
    case 0: return d->name();
    case 1: return d->value();
    case 2: return d->type();
    default: return QVariant();
    }
}

QVariant VariableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0: return tr("Name");
        case 1: return tr("Value");
        case 2: return tr("Type");
        default: return QVariant();
        }
    }
    return QVariant();
}

void VariableModel::OnVariableChanged(Variable* variable)
{
    QModelIndex index = GetIndexFromVariable(variable);
    if (index.isValid())
    {
        emit dataChanged(index, index);
    }
}

void VariableModel::OnVariablesChanged(const QList<Variable*>& variables)
{
    // emit one time
    if (variables.isEmpty()) return;
    QModelIndex first_index = GetIndexFromVariable(variables.first());
    QModelIndex last_index = GetIndexFromVariable(variables.last());
    if (first_index.isValid() && last_index.isValid())
    {
        emit dataChanged(first_index, last_index);
    }
}

void VariableModel::OnVariableChildInserted(Variable* parent, Variable* child)
{

}

void VariableModel::OnVariableChildRemoved(Variable* parent, Variable* child)
{

}

void VariableModel::OnVariableChildrenInserted(Variable* parent, const QList<Variable*>& children)
{

}

void VariableModel::OnVariableChildrenRemoved(Variable* parent, const QList<Variable*>& children)
{

}

void VariableModel::VariableInserted(Variable* variable, Variable* parent)
{
    if(variable_manager_cache_[variable->manager()].isEmpty())
    {
        connect(variable->manager(), &VariableManager::VariableChanged,
                this, &VariableModel::OnVariableChanged);
        connect(variable->manager(), &VariableManager::VariablesChanged,
                this, &VariableModel::OnVariablesChanged);
        connect(variable->manager(), &VariableManager::VariableChildInserted,
                this, &VariableModel::OnVariableChildInserted);
        connect(variable->manager(), &VariableManager::VariableChildRemoved,
                this, &VariableModel::OnVariableChildRemoved);
        connect(variable->manager(), &VariableManager::VariableChildrenInserted,
                this, &VariableModel::OnVariableChildrenInserted);
        connect(variable->manager(), &VariableManager::VariableChildrenRemoved,
                this, &VariableModel::OnVariableChildrenRemoved);
    }
    variable_manager_cache_[variable->manager()].append(variable);
}

void VariableModel::VariableRemoved(Variable* variable, Variable* parent)
{
    if(!variable_manager_cache_.contains(variable->manager()))
        return;

    variable_manager_cache_[variable->manager()].removeOne(variable);
    if(variable_manager_cache_[variable->manager()].isEmpty())
    {
        disconnect(variable->manager(), &VariableManager::VariableChanged,
                this, &VariableModel::OnVariableChanged);
        disconnect(variable->manager(), &VariableManager::VariablesChanged,
                this, &VariableModel::OnVariablesChanged);
        disconnect(variable->manager(), &VariableManager::VariableChildInserted,
                this, &VariableModel::OnVariableChildInserted);
        disconnect(variable->manager(), &VariableManager::VariableChildRemoved,
                this, &VariableModel::OnVariableChildRemoved);
        disconnect(variable->manager(), &VariableManager::VariableChildrenInserted,
                this, &VariableModel::OnVariableChildrenInserted);
        disconnect(variable->manager(), &VariableManager::VariableChildrenRemoved,
                this, &VariableModel::OnVariableChildrenRemoved);
    }
}

Variable *VariableModel::GetVariableFromIndex(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<Variable *>(index.internalPointer()) : nullptr;
}

QModelIndex VariableModel::GetIndexFromVariable(Variable *data) const
{
    if (!data) return QModelIndex();

    int idx = root_variable_list_.indexOf(data);
    if (idx >= 0)
        return createIndex(idx, 0, data);

    auto *parent_data = data->parent();
    if (!parent_data) return QModelIndex();

    int row = RowOfChildInParent(parent_data, data);
    if (row < 0) return QModelIndex();

    return createIndex(row, 0, data);
}

int VariableModel::RowOfChildInParent(Variable *parent, Variable *child) const
{
    if (!parent || !child) return -1;
    const auto children = parent->children();
    return children.indexOf(child);
}

} // namespace gui
} // namespace xequation