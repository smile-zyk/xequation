#pragma once

#include <memory>
#include <string>

#include <boost/uuid/uuid.hpp>
#include <tsl/ordered_map.h>

#include "value.h"
#include "dependency_graph.h"

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
    enum class Type
    {
        kUnknown,
        kVariable,
        kFunction,
        kClass,
        kImport,
        kImportFrom,
    };

    enum class Status
    {
        kPending,
        kSuccess,
        kSyntaxError,
        kNameError,
        kTypeError,
        kZeroDivisionError,
        kValueError,
        kMemoryError,
        kOverflowError,
        kRecursionError,
        kIndexError,
        kKeyError,
        kAttributeError,
    };

    explicit Equation(const ParseResultItem& item, const boost::uuids::uuid &group_id, EquationManager *manager);
    explicit Equation(const std::string &name, const boost::uuids::uuid &group_id, EquationManager *manager);
    virtual ~Equation() = default;

    static EquationPtr Create(const ParseResultItem& item, const boost::uuids::uuid &group_id, EquationManager *manager);

    void set_content(const std::string &content)
    {
        content_ = content;
    }

    void set_type(Type type)
    {
        type_ = type;
    }

    void set_status(Status status)
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

    Type type() const
    {
        return type_;
    }

    Status status() const
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
    const DependencyGraph::NodeNameSet& GetDependencies() const;
    const DependencyGraph::NodeNameSet& GetDependents() const;

    bool operator==(const Equation &other) const;
    bool operator!=(const Equation &other) const;

    void NotifyValueChanged();

    static Type StringToType(const std::string &type_str);
    static Status StringToStatus(const std::string &status_str);
    static std::string TypeToString(Type type);
    static std::string StatusToString(Status status);

  private:
  private:
    std::string name_;
    std::string content_;
    Type type_;
    Status status_;
    std::string message_;
    EquationGroupId group_id_;
    EquationManager *manager_ = nullptr;
};

std::ostream &operator<<(std::ostream &os, Equation::Type type);
std::ostream &operator<<(std::ostream &os, Equation::Status status);

} // namespace xequation