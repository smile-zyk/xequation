#include "variable_model.h"
#include <cstddef>

namespace xequation
{
namespace gui
{

// ============================================================================
// Variable Implementation
// ============================================================================

Variable::Variable(const QString &name, const QString &value, const QString &type)
    : name_(name), value_(value), type_(type), parent_(nullptr)
{
}

Variable::~Variable()
{
    ClearChildren();
}

std::unique_ptr<Variable> Variable::Create(const QString &name, const QString &value, const QString &type)
{
    return std::unique_ptr<Variable>(new Variable(name, value, type));
}

void Variable::AddChild(std::unique_ptr<Variable> child)
{
    if (!child)
        return;

    Variable *child_ptr = child.get();
    if (children_.contains(child_ptr))
        return;
    child->parent_ = this;
    children_.emplace(child_ptr, std::move(child));
}

void Variable::RemoveChild(Variable *child)
{
    if (!child)
        return;

    auto it = children_.find(child);
    if (it == children_.end())
        return;

    children_.erase(it);
}

void Variable::ClearChildren()
{
    children_.clear();
}

size_t Variable::ChildCount() const
{
    return children_.size();
}

QList<Variable*> Variable::GetChildren() const
{
    QList<Variable*> result;
    result.reserve(children_.size());
    for (const auto &pair : children_)
    {
        result.append(pair.first);
    }
    return result;
}

Variable *Variable::GetChildAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(children_.size()))
        return nullptr;

    auto it = children_.nth(index);
    return it != children_.end() ? it->first : nullptr;
}

int Variable::IndexOfChild(Variable *child) const
{
    if (!child)
        return -1;

    auto it = children_.find(child);
    if (it == children_.end())
        return -1;

    return std::distance(children_.begin(), it);
}

bool Variable::HasChild(Variable *child) const
{
    return child && children_.contains(child);
}

// ============================================================================
// VariableModel Implementation
// ============================================================================

VariableModel::VariableModel(QObject *parent) 
    : QAbstractItemModel(parent)
{
}

VariableModel::~VariableModel()
{
    ClearRootVariables();
}

void VariableModel::AddRootVariable(Variable::UniquePtr variable)
{
    if (!variable || root_variable_map_.contains(variable.get()))
        return;

    int row = root_variable_map_.size();
    beginInsertRows(QModelIndex(), row, row);
    root_variable_map_.emplace(variable.get(), std::move(variable));
    endInsertRows();
}

void VariableModel::RemoveRootVariable(Variable *variable)
{
    if (!variable || !root_variable_map_.contains(variable))
        return;

    int row = IndexOfRootVariable(variable);
    if (row < 0)
        return;

    beginRemoveRows(QModelIndex(), row, row);
    root_variable_map_.erase(variable);
    endRemoveRows();
}

void VariableModel::ClearRootVariables()
{
    if (root_variable_map_.empty())
        return;

    beginResetModel();
    root_variable_map_.clear();
    endResetModel();
}

QList<Variable*> VariableModel::GetRootVariables() const
{
    QList<Variable*> result;
    result.reserve(root_variable_map_.size());
    for (const auto &pair : root_variable_map_)
    {
        result.append(pair.first);
    }
    return result;
}

Variable *VariableModel::GetRootVariableAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(root_variable_map_.size()))
        return nullptr;
    
    auto it = root_variable_map_.nth(index);
    return it != root_variable_map_.end() ? it->first : nullptr;
}

int VariableModel::IndexOfRootVariable(Variable *variable) const
{
    if (!variable)
        return -1;

    auto it = root_variable_map_.find(variable);
    if (it == root_variable_map_.end())
        return -1;
    
    return std::distance(root_variable_map_.begin(), it);
}

bool VariableModel::IsContainRootVariable(Variable *variable) const
{
    return variable && root_variable_map_.contains(variable);
}

int VariableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 3; // Name, Value, Type
}

int VariableModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return root_variable_map_.size();

    auto *parent_variable = GetVariableFromIndex(parent);
    if (!parent_variable)
        return 0;
    
    return parent_variable->ChildCount();
}

QModelIndex VariableModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (!parent.isValid())
    {
        auto *variable = GetRootVariableAt(row);
        return variable ? createIndex(row, column, variable) : QModelIndex();
    }

    auto *parent_variable = GetVariableFromIndex(parent);
    if (!parent_variable)
        return QModelIndex();

    auto *child = parent_variable->GetChildAt(row);
    return child ? createIndex(row, column, child) : QModelIndex();
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

    auto *grand_parent = parent_variable->parent();
    if (!grand_parent)
        return QModelIndex();

    int row = grand_parent->IndexOfChild(parent_variable);
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

    auto *variable = GetVariableFromIndex(index);
    if (!variable)
        return QVariant();

    switch (index.column())
    {
    case 0:
        return variable->name();
    case 1:
        return variable->value();
    case 2:
        return variable->type();
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

Variable *VariableModel::GetVariableFromIndex(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<Variable *>(index.internalPointer()) : nullptr;
}

QModelIndex VariableModel::GetIndexFromVariable(Variable *variable) const
{
    if (!variable)
        return QModelIndex();

    int root_idx = IndexOfRootVariable(variable);
    if (root_idx >= 0)
        return createIndex(root_idx, 0, variable);

    auto *parent_variable = variable->parent();
    if (!parent_variable)
        return QModelIndex();

    int row = parent_variable->IndexOfChild(variable);
    if (row < 0)
        return QModelIndex();

    return createIndex(row, 0, variable);
}

} // namespace gui
} // namespace xequation