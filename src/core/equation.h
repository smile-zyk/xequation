#pragma once

#include <memory>
#include <string>

#include <boost/uuid/uuid.hpp>
#include <tsl/ordered_map.h>
#include <tsl/ordered_set.h>

#include "equation_common.h"

namespace xequation
{
class Equation;
class EquationManager;
class ParseResultItem;

using EquationGroupId = boost::uuids::uuid;
using EquationPtr = std::unique_ptr<Equation>;
using EquationPtrOrderedMap = tsl::ordered_map<std::string, EquationPtr>;

class Equation
{
  public:
    explicit Equation(const ParseResultItem& item, const boost::uuids::uuid &group_id, EquationManager *manager);
    explicit Equation(const std::string &name, const boost::uuids::uuid &group_id, EquationManager *manager);
    virtual ~Equation() = default;

    static EquationPtr Create(const ParseResultItem& item, const boost::uuids::uuid &group_id, EquationManager *manager);

    void set_content(const std::string &content)
    {
        content_ = content;
    }

    void set_type(ItemType type)
    {
        type_ = type;
    }

    void set_status(ResultStatus status)
    {
        status_ = status;
    }

    void set_message(const std::string &message)
    {
        message_ = message;
    }

    const std::string &name() const
    {
        return name_;
    }

    const std::string &content() const
    {
        return content_;
    }

    ItemType type() const
    {
        return type_;
    }

    ResultStatus status() const
    {
        return status_;
    }

    const std::string &message() const
    {
        return message_;
    }

    const EquationManager *manager() const
    {
        return manager_;
    }

    const EquationGroupId &group_id() const
    {
        return group_id_;
    }

    Value GetValue() const;
    const tsl::ordered_set<std::string> & GetDependencies() const;
    const tsl::ordered_set<std::string> & GetDependents() const;

    bool operator==(const Equation &other) const;
    bool operator!=(const Equation &other) const;

  private:
    std::string name_;
    std::string content_;
    ItemType type_;
    ResultStatus status_;
    std::string message_;
    EquationGroupId group_id_;
    EquationManager *manager_ = nullptr;
};

} // namespace xequation