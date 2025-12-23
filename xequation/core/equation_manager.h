#pragma once
#include <exception>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/uuid/uuid_io.hpp>

#include "dependency_graph.h"
#include "equation.h"
#include "equation_common.h"
#include "equation_context.h"
#include "equation_group.h"
#include "equation_signals_manager.h"

namespace xequation
{

class EquationException : public std::exception
{
  public:
    enum class ErrorCode
    {
        kEquationGroupNotFound,
        kEquationGroupAlreadyExists,
        kEquationNotFound,
        kEquationAlreayExists,
    };

    const char *what() const noexcept override
    {
        if (message_cache_.empty())
        {
            message_cache_ = GenerateErrorMessage();
        }
        return message_cache_.c_str();
    }

    const std::string &equation_name() const
    {
        return equation_name_;
    }

    const EquationGroupId &group_id() const
    {
        return group_id_;
    }

    ErrorCode error_code() const
    {
        return error_code_;
    }

    static EquationException EquationGroupNotFound(EquationGroupId group_id)
    {
        return EquationException(ErrorCode::kEquationGroupNotFound, group_id);
    }

    static EquationException EquationGroupAlreadyExists(EquationGroupId group_id)
    {
        return EquationException(ErrorCode::kEquationGroupAlreadyExists, group_id);
    }

    static EquationException EquationNotFound(const std::string &equation_name)
    {
        return EquationException(ErrorCode::kEquationNotFound, equation_name);
    }

    static EquationException EquationAlreadyExists(const std::string &equation_name)
    {
        return EquationException(ErrorCode::kEquationAlreayExists, equation_name);
    }

  private:
    std::string GenerateErrorMessage() const
    {
        std::ostringstream oss;

        switch (error_code_)
        {
        case ErrorCode::kEquationGroupNotFound:
            oss << "Equation group not found. Group ID: " << boost::uuids::to_string(group_id_);
            break;

        case ErrorCode::kEquationGroupAlreadyExists:
            oss << "Equation group already exists. Group ID: " << boost::uuids::to_string(group_id_);
            break;

        case ErrorCode::kEquationNotFound:
            oss << "Equation not found. Name: '" << equation_name_ << "'";
            break;

        case ErrorCode::kEquationAlreayExists:
            oss << "Equation already exists. Name: '" << equation_name_ << "'";
            break;

        default:
            oss << "Unknown equation error occurred.";
            break;
        }

        return oss.str();
    }

    EquationException(ErrorCode error_code, const std::string &equation_name)
        : error_code_(error_code), equation_name_(equation_name)
    {
    }

    EquationException(ErrorCode error_code, const EquationGroupId &group_id)
        : error_code_(error_code), group_id_(group_id)
    {
    }

    ErrorCode error_code_;
    std::string equation_name_;
    EquationGroupId group_id_;
    mutable std::string message_cache_;
};

class EquationManager
{
  public:
    EquationManager(
        std::unique_ptr<EquationContext> context, InterpretHandler interpret_handler, ParseHandler parse_handler, const std::string &language = "Unknown") noexcept;

    virtual ~EquationManager() noexcept = default;

    const EquationGroup *GetEquationGroup(const EquationGroupId &group_id) const;

    const Equation *GetEquation(const std::string &equation_name) const;

    std::vector<EquationGroupId> GetEquationGroupIds() const;

    std::vector<std::string> GetEquationNames() const;

    const tsl::ordered_set<std::string> &GetExternalVariableNames() const;

    bool IsEquationGroupExist(const EquationGroupId &group_id) const;

    bool IsEquationExist(const std::string &eqn_name) const;

    bool IsStatementSingleEquation(const std::string &equation_statement) const;

    EquationGroupId AddEquationGroup(const std::string &equation_statement);

    void EditEquationGroup(const EquationGroupId &group_id, const std::string &equation_statement);

    void RemoveEquationGroup(const EquationGroupId &group_id);

    void SetExternalVariable(const std::string &var_name, const Value &value);

    void RemoveExternalVariable(const std::string &var_name);

    ParseResult Parse(const std::string &expression, ParseMode mode) const;

    InterpretResult Eval(const std::string &expression) const;

    void Reset();

    void Update();

    void UpdateEquation(const std::string &equation_name);

    void UpdateSingleEquation(const std::string &equation_name);

    void UpdateEquationGroup(const EquationGroupId &group_id);

    const DependencyGraph &graph()
    {
        return *graph_;
    }

    const EquationContext &context()
    {
        return *context_;
    }

    const EquationSignalsManager &signals_manager() const
    {
        return *signals_manager_;
    }

  private:
    EquationManager(const EquationManager &) = delete;
    EquationManager &operator=(const EquationManager &) = delete;

    EquationManager(EquationManager &&) noexcept = delete;
    EquationManager &operator=(EquationManager &&) noexcept = delete;

    Equation *GetEquationInternal(const std::string &equation_name);
    EquationGroup *GetEquationGroupInternal(const EquationGroupId &group_id);
    void UpdateEquationInternal(const std::string &equation_name);

    void AddNodeToGraph(const std::string &node_name, const std::vector<std::string> &dependencies);
    void RemoveNodeInGraph(const std::string &node_name);

    void AddEquationToGroup(EquationGroup *group, EquationPtr equation);
    void RemoveEquationInGroup(EquationGroup *group, const std::string &equation_name);

    ScopedConnection ConnectGraphDependencyUpdated(std::vector<std::string> &dependency_updated_equation) const;
    ScopedConnection ConnectGraphDependentUpdated(std::vector<std::string> &dependent_updated_equation) const;
    
    void NotifyEquationDependentsUpdated(const std::string &equation_name) const;
    void NotifyEquationDependenciesUpdated(const std::string &equation_name) const;

  private:
    std::unique_ptr<DependencyGraph> graph_;
    std::unique_ptr<EquationContext> context_;
    std::unique_ptr<EquationSignalsManager> signals_manager_;

    EquationGroupPtrOrderedMap equation_group_map_;
    std::unordered_map<std::string, boost::uuids::uuid> equation_name_to_group_id_map_;
    tsl::ordered_set<std::string> external_variable_names_;

    InterpretHandler interpret_handler_ = nullptr;
    ParseHandler parse_handler_ = nullptr;
    std::string language_{};
};
} // namespace xequation