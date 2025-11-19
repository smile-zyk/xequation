#include "equation_group.h"

#include <boost/uuid/random_generator.hpp>

namespace xequation
{

EquationGroupPtr Create(const EquationManager *manager)
{
    EquationGroupPtr group = EquationGroupPtr(new EquationGroup(manager));
    return group;
}

EquationGroup::EquationGroup(const EquationManager *manager) : manager_(manager)
{
    static boost::uuids::random_generator rgen;
    id_ = rgen();
}

void EquationGroup::AddEquation(std::unique_ptr<Equation> equation)
{
    equation_map_.insert({equation->name(), std::move(equation)});
}

void EquationGroup::RemoveEquation(const std::string &equation_name)
{
    equation_map_.erase(equation_name);
}

const Equation *EquationGroup::GetEquation(const std::string &equation_name) const
{
    if (equation_map_.contains(equation_name))
    {
        return equation_map_.at(equation_name).get();
    }
    return nullptr;
}

Equation *EquationGroup::GetEquation(const std::string &equation_name)
{
    if (equation_map_.contains(equation_name))
    {
        return equation_map_.at(equation_name).get();
    }
    return nullptr;
}

bool EquationGroup::IsEquationExist(const std::string &equation_name) const
{
    return equation_map_.contains(equation_name);
}

std::vector<std::string> EquationGroup::GetEquationNames() const
{
    std::vector<std::string> res;
    for (const auto &entry : equation_map_)
    {
        res.push_back(entry.first);
    }
    return res;
}

} // namespace xequation