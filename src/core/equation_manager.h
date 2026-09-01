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
        kExpressionNotFound,
        // kEquationConflictsWithBuiltin,
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

    static EquationException ExpressionNotFound(const std::string &expression_id)
    {
        return EquationException(ErrorCode::kExpressionNotFound, expression_id);
    }

    // static EquationException EquationConflictsWithBuiltin(const std::string &equation_name)
    // {
    //     return EquationException(ErrorCode::kEquationConflictsWithBuiltin, equation_name);
    // }

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

        case ErrorCode::kExpressionNotFound:
            oss << "Expression not found. ID: '" << equation_name_ << "'";
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
        std::unique_ptr<EquationContext> context, EvalHandler eval_handler, ExecHandler exec_handler, ParseHandler parse_handler, const EquationEngineInfo &engine_info) noexcept;

    virtual ~EquationManager() noexcept = default;

    const EquationGroup *GetEquationGroup(const EquationGroupId &group_id) const;

    /// The group that owns the given equation name (nullptr if the equation or
    /// its owning group does not exist).  Convenience for group-centric flows:
    /// pair with EquationGroup::FirstEquation() for single-equation groups.
    const EquationGroup *GetEquationGroup(const std::string &equation_name) const;

    const Equation *GetEquation(const std::string &equation_name) const;

    std::vector<EquationGroupId> GetEquationGroupIds() const;

    std::vector<std::string> GetEquationNames() const;

    bool IsEquationGroupExist(const EquationGroupId &group_id) const;

    bool IsEquationExist(const std::string &eqn_name) const;

    EquationGroupId AddEquationGroup(const std::string &equation_statement);

    EquationGroupId AddEquation(const std::string& equation_name, const std::string& equation_content);

    void EditEquationGroup(const EquationGroupId &group_id, const std::string &equation_statement);

    void EditSingleEquation(const EquationGroupId &group_id, const std::string& equation_name, const std::string& equation_content);

    void RemoveEquationGroup(const EquationGroupId &group_id);

    ParseResult Parse(const std::string &expression, ParseMode mode) const;

    InterpretResult Eval(const std::string &expression) const;

    InterpretResult Exec(const std::string &statement) const;

    void Reset();

    void ResetContext();

    void Update();

    void UpdateEquation(const std::string &equation_name);

    void UpdateEquationGroup(const EquationGroupId &group_id);

    // Recomputes a single graph node without propagating to dependents
    // (dispatches equations/expressions by node kind).
    void UpdateNode(const std::string &node_name);

    void UpdateEquationStatus(const std::string &equation_name, ResultStatus status, const std::string& message = "");

    // Computes the update scope for a group: the group's equations + all of their
    // dependents (propagated by TopologicalSort) plus every dirty (invalidated) node
    // (e.g. cross-group equations that lost their dependency after a rename/removal).
    // Returns an empty vector if the group does not exist.
    std::vector<std::string> GetEquationsToUpdate(const EquationGroupId &group_id) const;

    // =========================================================================
    // Registered expressions (observe-only, never written into the context)
    // =========================================================================

    // Registers an expression; returns its id.
    ExpressionId AddExpression(const std::string &expression);

    // Removes a registered expression (and its graph node).
    void RemoveExpression(const ExpressionId &id);

    // Returns the expression object, or nullptr.
    const Expression *GetExpression(const ExpressionId &id) const;

    // Returns true when a registered expression with this id exists.
    bool IsExpressionExist(const ExpressionId &id) const;

    // All registered expression ids (insertion order not guaranteed).
    std::vector<ExpressionId> GetExpressionIds() const;

    // Value of a registered expression (Null when not found / not yet computed).
    EquationValue GetExpressionValue(const ExpressionId &id) const;

    // Computes the expression (does not propagate to dependents).
    void UpdateExpression(const ExpressionId &id);

    // =========================================================================
    // External input symbols (named graph anchors, value owned by the host)
    // =========================================================================

    // Registers an external input symbol as a graph anchor.
    bool AddExternalInput(const std::string &symbol_name);

    // Removes the anchor (dangling edges to it are deactivated but retained).
    void RemoveExternalInput(const std::string &symbol_name);

    // True when the name is a registered external input.
    bool IsExternalInput(const std::string &symbol_name) const;

    // Names of all registered external inputs.
    std::vector<std::string> GetExternalInputNames() const;

    // Marks the external input dirty and invalidates its dependents.  With a
    // value: inject into the context, recompute dependents, then remove again.
    // Without (REL Dataset): only invalidate; recompute on a later Update().
    void UpdateExternalInput(const std::string &symbol_name,
                             const EquationValue &value = EquationValue::Null());

    // Invalidates a batch of inputs (no value injection).  A single merged
    // topological pass recomputes dependents, so shared dependents run once.
    void InvalidateExternalInputs(const std::vector<std::string> &symbol_names);

    bool WriteDependencyGraphToDotFile(const std::string &file_path) const;

    const DependencyGraph &graph()
    {
        return *graph_;
    }

    EquationContext &context()
    {
        return *context_;
    }

    const EquationContext &context() const
    {
        return *context_;
    }

    const EquationSignalsManager &signals_manager() const
    {
        return *signals_manager_;
    }

    const EquationEngineInfo &engine_info() const
    {
        return engine_info_;
    }

  private:
    EquationManager(const EquationManager &) = delete;
    EquationManager &operator=(const EquationManager &) = delete;

    EquationManager(EquationManager &&) noexcept = delete;
    EquationManager &operator=(EquationManager &&) noexcept = delete;

    Equation *GetEquationInternal(const std::string &equation_name);
    EquationGroup *GetEquationGroupInternal(const EquationGroupId &group_id);
    void UpdateEquationInternal(const std::string &equation_name);
    void UpdateExpressionInternal(const ExpressionId &id);

    void AddNodeToGraph(const std::string &node_name, const std::vector<std::string> &dependencies);
    void RemoveNodeInGraph(const std::string &node_name);

    void AddEquationToGroup(EquationGroup *group, EquationPtr equation);
    void RemoveEquationInGroup(EquationGroup *group, const std::string &equation_name);

    // Appends every dirty node in the graph to update_names (may duplicate; the caller
    // deduplicates with TopologicalSort).
    void CollectDirtyNodes(std::vector<std::string> &update_names) const;

    ScopedConnection ConnectGraphDependencyUpdated(std::vector<std::string> &dependency_updated_equation) const;
    ScopedConnection ConnectGraphDependentUpdated(std::vector<std::string> &dependent_updated_equation) const;
    
    void NotifyEquationDependentsUpdated(const std::string &equation_name) const;
    void NotifyEquationDependenciesUpdated(const std::string &equation_name) const;

    std::string GenerateEquationDotNodeLabel(const std::string &equation_name) const;
  private:
    std::unique_ptr<DependencyGraph> graph_;
    std::unique_ptr<EquationContext> context_;
    std::unique_ptr<EquationSignalsManager> signals_manager_;

    EquationGroupPtrOrderedMap equation_group_map_;
    std::unordered_map<std::string, boost::uuids::uuid> equation_name_to_group_id_map_;

    // Registered expressions.  Keyed by name (same slot as equations).
    std::unordered_map<std::string, Expression> expression_map_;
    // Expression id -> internal graph/name slot.
    std::unordered_map<ExpressionId, std::string> expression_id_to_name_map_;

    // External input symbols.  Value is provided externally.
    tsl::ordered_set<std::string> external_input_names_;

    EvalHandler eval_handler_ = nullptr;
    ExecHandler exec_handler_ = nullptr;
    ParseHandler parse_handler_ = nullptr;
    EquationEngineInfo engine_info_{};
};
} // namespace xequation