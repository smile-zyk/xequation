#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include <tsl/ordered_set.h>

#include "dependency_graph.h"
#include "equation_common.h"
#include "equation_signals_manager.h"

namespace rel
{
class Environment;
}

namespace xequation
{
// =========================================================================
// EquationManager - name -> expression dependency manager (wraps REL directly).
//
// It is a process-wide singleton (GetInstance()) and owns the entire engine:
//   - the variable table is rel::Environment (environment() exposes it);
//   - expression evaluation is rel::Eval; dependency extraction is local here;
//   - an equation "binds Eval(content) to a name in the env".
// Each Update recomputes in dependency order; failed names are removed from the
// env and stay dirty so a later Update retries them. Engine initialization
// (REL builtin constants/functions) is idempotent on first construction.
// =========================================================================
class EquationManager
{
  public:
    // Process-wide singleton (C++11 magic-static, thread-safe).
    static EquationManager &GetInstance();

    virtual ~EquationManager() noexcept = default;

    // =========================================================================
    // Environment (variable table)
    // =========================================================================

    // Direct access to the underlying REL environment.
    rel::Environment &environment();
    const rel::Environment &environment() const;

    // Whether the name is bound in the environment.
    bool HasVariable(const std::string &name) const;

    // Read a bound variable; null EquationValue when unbound.
    EquationValue GetVariable(const std::string &name) const;

    // =========================================================================
    // Equation CRUD (one equation per entry; no equation "groups")
    // =========================================================================

    /// Returns the equation with the given name, or nullptr.
    const Equation *GetEquation(const std::string &equation_name) const;

    /// Returns the equation with the given id, or nullptr.
    const Equation *GetEquationById(const ObjectId &id) const;

    /// All equation ids (insertion order).
    std::vector<ObjectId> GetEquationIds() const;

    /// All equation names (insertion order).
    std::vector<std::string> GetEquationNames() const;

    /// True when an equation with this name exists.
    bool IsEquationExist(const std::string &eqn_name) const;

    /// True when an equation with this id exists.
    bool IsEquationExist(const ObjectId &id) const;

    /// Value currently bound to the equation's name in the env
    /// (null EquationValue when the equation has not (successfully)
    /// computed, or its name is not bound).
    EquationValue GetEquationValue(const std::string &equation_name) const;

    /// User-facing dependencies of an equation: other equation names it reads.
    /// Registered-expression graph nodes ("expr_<uuid>") are filtered out.
    std::vector<std::string> GetEquationDependencies(const std::string &equation_name) const;

    /// User-facing dependents of an equation: other equation names that read it.
    /// Registered-expression graph nodes ("expr_<uuid>") are filtered out.
    std::vector<std::string> GetEquationDependents(const std::string &equation_name) const;

    /// Adds a single equation "name = expression".  Returns its id.
    ObjectId AddEquation(const std::string &equation_name, const std::string &expression,
                         const std::string &tag = kEquationTagDefault);

    /// Replaces the content of an existing equation (name unchanged).
    /// Throws when the name is not found.
    ObjectId EditEquation(const std::string &equation_name, const std::string &expression);

    /// Replaces the content of an existing equation (name unchanged).
    /// Throws when the id is not found.
    ObjectId EditEquation(const ObjectId &id, const std::string &expression);

    /// Renames an equation by name (removes the old name, defines the new one).
    /// Throws when the old name is not found or the new name already exists.
    ObjectId RenameEquation(const std::string &old_name, const std::string &new_name);

    /// Renames the equation identified by id (removes the old name, defines
    /// the new one). Throws when the id is not found or the new name exists.
    ObjectId RenameEquation(const ObjectId &id, const std::string &new_name);

    /// Removes an equation by name.  No-op when it does not exist.
    void RemoveEquation(const std::string &equation_name);

    /// Removes an equation by id.  No-op when it does not exist.
    void RemoveEquation(const ObjectId &id);

    // =========================================================================
    // Evaluation / Parse
    // =========================================================================

    /// Parses a single expression: syntax validation + dependency extraction.
    ParseResult Parse(const std::string &expression) const;

    /// Evaluates a single expression against the manager's environment
    /// (pure; the result is NOT bound to any name -- callers bind it with
    /// environment().Define(name, value) or context-free Eval).
    InterpretResult Eval(const std::string &expression) const;

    void Reset();

    void ResetContext();

    void Update();

    void UpdateEquation(const std::string &equation_name);

    // Recomputes a single graph node without propagating to dependents
    // (dispatches equations / expressions by node kind).
    void UpdateNode(const std::string &node_name);

    void UpdateEquationStatus(const std::string &equation_name, ResultStatus status, const std::string& message = "");

    // Computes the update scope for a single equation: the equation + all of
    // its dependents (propagated by TopologicalSort) plus every dirty
    // (invalidated) node.  Returns an empty vector when the equation does not
    // exist.
    std::vector<std::string> GetEquationsToUpdate(const std::string &equation_name) const;

    // =========================================================================
    // Registered expressions (observe-only, never written into the environment)
    // =========================================================================

    // Registers an expression; returns its id.
    ObjectId AddExpression(const std::string &expression,
                           const std::string &tag = kWatchTagDefault);

    // Removes a registered expression (and its graph node).
    void RemoveExpression(const ObjectId &id);

    // Returns the expression object, or nullptr.
    const Expression *GetExpression(const ObjectId &id) const;

    // Returns true when a registered expression with this id exists.
    bool IsExpressionExist(const ObjectId &id) const;

    // Returns true when the given name is the internal graph-node slot of a
    // registered expression (i.e. "expr_<uuid>").  Hosts can use this to hide
    // expression nodes from user-facing dependency / dependent listings.
    bool IsExpressionNode(const std::string &name) const;

    // All registered expression ids (insertion order not guaranteed).
    std::vector<ObjectId> GetExpressionIds() const;

    // Value of a registered expression (nullopt when not found / not yet computed).
    EquationValue GetExpressionValue(const ObjectId &id) const;

    // Computes the expression (does not propagate to dependents).
    void UpdateExpression(const ObjectId &id);

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

    // Marks a batch of external inputs dirty and recomputes their dependents
    // in a single merged topological pass (shared dependents run once; dirty
    // nodes left by earlier failures are also retried).  The inputs' values
    // are owned by the host (env / REL Dataset registry) -- nothing is
    // injected here.  Unknown input names are ignored.  This is the only
    // entry point for updating external inputs.
    void InvalidateExternalInputs(const std::vector<std::string> &symbol_names);

    bool WriteDependencyGraphToDotFile(const std::string &file_path) const;

    const DependencyGraph &graph() const
    {
        return *graph_;
    }

    const EquationSignalsManager &signals_manager() const
    {
        return *signals_manager_;
    }

  private:
    EquationManager();  // Only GetInstance() may construct.
    EquationManager(const EquationManager &) = delete;
    EquationManager &operator=(const EquationManager &) = delete;
    EquationManager(EquationManager &&) noexcept = delete;
    EquationManager &operator=(EquationManager &&) noexcept = delete;

    Equation *GetEquationInternal(const std::string &equation_name);
    Equation *GetEquationInternal(const ObjectId &id);
    void UpdateEquationInternal(const std::string &equation_name);
    void UpdateExpressionInternal(const ObjectId &id);

    void AddNodeToGraph(const std::string &node_name, const std::vector<std::string> &dependencies);
    void RemoveNodeInGraph(const std::string &node_name);

    // Appends every dirty node in the graph to update_names (may duplicate; the caller
    // deduplicates with TopologicalSort).
    void CollectDirtyNodes(std::vector<std::string> &update_names) const;

    ScopedConnection ConnectGraphDependencyUpdated(std::vector<std::string> &dependency_updated_equation) const;
    ScopedConnection ConnectGraphDependentUpdated(std::vector<std::string> &dependent_updated_equation) const;

    void NotifyEquationDependentsUpdated(const std::string &equation_name) const;
    void NotifyEquationDependenciesUpdated(const std::string &equation_name) const;

    std::string GenerateEquationDotNodeLabel(const std::string &equation_name) const;

    std::unique_ptr<DependencyGraph> graph_;
    std::unique_ptr<EquationSignalsManager> signals_manager_;

    // All equations, keyed by name (insertion order).  Each equation owns its
    // own ObjectId; there is no separate group layer any more.
    EquationPtrOrderedMap equation_map_;

    // Registered expressions.  Keyed by name (same slot as equations).
    std::unordered_map<std::string, Expression> expression_map_;
    // Expression id -> internal graph/name slot.
    std::unordered_map<ObjectId, std::string> expression_id_to_name_map_;

    // External input symbols.  Value is provided externally.
    tsl::ordered_set<std::string> external_input_names_;

    std::unique_ptr<rel::Environment> env_;
};

} // namespace xequation
