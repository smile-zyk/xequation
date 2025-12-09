#include "variable_manager.h"
#include "tsl/ordered_hash.h"

namespace xequation
{
namespace gui
{
Variable::Variable(const QString &name, const QString &value, const QString &type)
    : name_(name), value_(value), type_(type), parent_(nullptr)
{
}

Variable::~Variable()
{
    for (auto *child : child_ordered_set_)
    {
        if (child)
            child->parent_ = nullptr;
    }
    child_ordered_set_.clear();
    parent_ = nullptr;
}

void Variable::AddChild(Variable *child)
{
    manager_->AddVariableChild(this, child);
}

void Variable::AddChildren(const QList<Variable *> &children)
{
    manager_->AddVariableChildren(this, children);
}

void Variable::RemoveChild(Variable *child)
{
    manager_->RemoveVariableChild(this, child);
}

void Variable::RemoveChildren(const QList<Variable *> &children)
{
    manager_->RemoveVariableChildren(this, children);
}

Variable *Variable::GetChildAt(int index) const
{
    if (index < 0 || index >= child_ordered_set_.size())
        return nullptr;
    return *child_ordered_set_.nth(index);
}

void Variable::ClearChildren()
{
    QList<Variable *> children;
    for (auto *child : child_ordered_set_)
    {
        children.append(child);
    }
    manager_->RemoveVariableChildren(this, children);
}

void Variable::SetValue(const QString &value)
{
    manager_->SetVariableValue(this, value);
}

void Variable::SetType(const QString &type)
{
    manager_->SetVariableType(this, type);
}

int Variable::IndexOfChild(Variable *child) const
{
    auto it = child_ordered_set_.find(child);
    if (it == child_ordered_set_.end())
        return -1;
    return std::distance(child_ordered_set_.begin(), it);
}

bool Variable::HasChild(Variable *child) const
{
    return child_ordered_set_.contains(child);
}

VariableManager::VariableManager(QObject *parent) : QObject(parent) {}

VariableManager::~VariableManager()
{
    Clear();
}

Variable *VariableManager::CreateVariable(const QString &name, const QString &value, const QString &type)
{
    Variable *var = new Variable(name, value, type);
    var->manager_ = this;
    variable_set_.insert(var);
    return var;
}

void VariableManager::RemoveVariable(Variable *variable)
{
    if (!variable || !variable_set_.contains(variable))
        return;
    variable_set_.erase(variable);
    delete variable;
}

Variable *VariableManager::GetVariableAt(int index)
{
    if (index < 0 || index >= static_cast<int>(variable_set_.size()))
        return nullptr;
    return *variable_set_.nth(index);
}

void VariableManager::Clear()
{
    for (Variable *var : variable_set_)
    {
        delete var;
    }
    variable_set_.clear();
}

bool VariableManager::IsContain(Variable *variable) const
{
    return variable_set_.contains(variable);
}

void VariableManager::BeginUpdate()
{
    updating_ = true;
}

void VariableManager::EndUpdate()
{
    updating_ = false;
    emit VariablesChanged(updated_variables_);
    updated_variables_.clear();
}

void VariableManager::SetVariableValue(Variable *variable, const QString &value)
{
    variable->value_ = value;
    if (updating_ == false)
    {
        emit VariableChanged(variable);
    }
    else
    {
        updated_variables_.append(variable);
    }
}

void VariableManager::SetVariableType(Variable *variable, const QString &type)
{
    variable->type_ = type;
    if (updating_ == false)
    {
        emit VariableChanged(variable);
    }
    else
    {
        updated_variables_.append(variable);
    }
}

void VariableManager::AddVariableChild(Variable *parent, Variable *child)
{
    if (!parent || !child || parent->HasChild(child))
        return;
    emit VariableBeginInsert(parent, parent->ChildCount(), 1);
    parent->child_ordered_set_.insert(child);
    child->parent_ = parent;
    emit VariableEndInsert();
}

void VariableManager::RemoveVariableChild(Variable *parent, Variable *child)
{
    if (!parent || !child || !parent->HasChild(child))
        return;
    emit VariableBeginRemove(parent, parent->IndexOfChild(child), 1);
    parent->child_ordered_set_.erase(child);
    child->parent_ = nullptr;
    emit VariableEndRemove();
}

void VariableManager::AddVariableChildren(Variable *parent, const QList<Variable *> &children)
{
    int count = 0;

    for (Variable *child : children)
    {
        if (!parent || !child || parent->HasChild(child))
            continue;
        count++;
    }
    emit VariableBeginInsert(parent, parent->ChildCount(), count);
    for (Variable *child : children)
    {
        if (!parent || !child || parent->HasChild(child))
            continue;
        parent->child_ordered_set_.insert(child);
        child->parent_ = parent;
    }
    emit VariableEndInsert();
}

void VariableManager::RemoveVariableChildren(Variable *parent, const QList<Variable *> &children)
{
    std::vector<int> indices;
    for (Variable *child : children)
    {
        if (!parent || !child || !parent->HasChild(child))
            continue;
        int index = parent->IndexOfChild(child);
        if (index >= 0)
            indices.push_back(index);
    }

    std::sort(indices.begin(), indices.end());
    std::vector<std::pair<int, int>> ranges;
    if (indices.empty())
    {
        return;
    }
    int start = indices[0];
    int end = start;
    ;
    for (size_t i = 1; i < indices.size(); i++)
    {
        if (indices[i] == end + 1)
        {
            end = indices[i];
        }
        else
        {
            ranges.emplace_back(std::make_pair(start, end));
            start = indices[i];
            end = start;
        }
    }
    ranges.emplace_back(std::make_pair(start, end));

    std::reverse(ranges.begin(), ranges.end());

    for (const auto &range : ranges)
    {
        emit VariableBeginRemove(parent, range.first, range.second - range.first + 1);
        for (int i = range.second; i >= range.first; i--)
        {
            Variable *child = parent->GetChildAt(i);
            if (child)
            {
                parent->child_ordered_set_.erase(child);
                child->parent_ = nullptr;
            }
        }
        emit VariableEndRemove();
    }
}
} // namespace gui
} // namespace xequation