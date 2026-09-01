#include "equation.h"

#include <string>

#include "equation_manager.h"
#include "dependency_graph.h"

namespace xequation
{

Equation::Equation(const ParseResultItem &item, const ObjectId &group_id, EquationManager *manager)
    : name_(item.name),
      content_(item.content),
      type_(item.type),
      status_(ResultStatus::kPending),
      group_id_(group_id),
      manager_(manager)
{
}

Equation::Equation(const std::string &name, const ObjectId &group_id, EquationManager *manager)
    : name_(name), type_(ItemType::kUnknown), status_(ResultStatus::kPending), group_id_(group_id), manager_(manager)
{
}

EquationPtr Equation::Create(const ParseResultItem &item, const ObjectId &group_id, EquationManager *manager)
{
    EquationPtr equation = EquationPtr(new Equation(item, group_id, manager));
    return equation;
}

bool Equation::operator==(const Equation &other) const
{
    return name_ == other.name_ && content_ == other.content_ && type_ == other.type_ && status_ == other.status_ &&
           message_ == other.message_ && group_id_ == other.group_id_ && manager_ == other.manager_;
}

bool Equation::operator!=(const Equation &other) const
{
    return !(*this == other);
}

EquationValue Equation::GetValue() const
{
    return manager_->context().Get(name_);
}

tsl::ordered_set<std::string> Equation::GetDependencies() const
{
    // Hide registered-expression graph nodes ("expr_<uuid>") from the
    // user-facing dependency listing: an equation's deps are presented as
    // other equations only.
    tsl::ordered_set<std::string> result;
    for (const std::string &name : manager_->graph().GetNode(name_)->dependencies())
    {
        if (!manager_->IsExpressionNode(name))
        {
            result.insert(name);
        }
    }
    return result;
}

tsl::ordered_set<std::string> Equation::GetDependents() const
{
    // Hide registered-expression graph nodes ("expr_<uuid>") from the
    // user-facing dependent listing: an equation's dependents are presented as
    // other equations only (a watch expression is not an equation).
    tsl::ordered_set<std::string> result;
    for (const std::string &name : manager_->graph().GetNode(name_)->dependents())
    {
        if (!manager_->IsExpressionNode(name))
        {
            result.insert(name);
        }
    }
    return result;
}

} // namespace xequation