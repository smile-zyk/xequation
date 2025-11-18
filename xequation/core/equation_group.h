#pragma once

#include "equation.h"

namespace xequation
{
class EquationGroup;
using EquationGroupPtr = std::unique_ptr<EquationGroup>;
using EquationGroupPtrOrderedMap = tsl::ordered_map<boost::uuids::uuid, EquationGroupPtr>;

class EquationGroup
{
  public:
    EquationGroup(const EquationManager *manager);

    static EquationGroupPtr Create(const EquationManager* manager);

    void AddEquation(std::unique_ptr<Equation> equation);

    void RemoveEquation(const std::string &equation_name);

    const Equation *GetEquation(const std::string &equation_name) const;

    bool IsEquationExist(const std::string& equation_name) const;

    std::vector<std::string> GetEquationNames() const;

    void set_statement(const std::string& statement)
    {
        statement_ = statement;
    }

    const std::string& statement() const
    {
        return statement_;
    }

    const EquationManager *manaegr() const
    {
        return manager_;
    }

    const EquationPtrOrderedMap &equation_map() const
    {
        return equation_map_;
    }

    const EquationGroupId &id() const
    {
        return id_;
    }

  private:
    EquationPtrOrderedMap equation_map_;
    EquationGroupId id_;
    std::string statement_;
    const EquationManager *manager_;
};
} // namespace xequation
