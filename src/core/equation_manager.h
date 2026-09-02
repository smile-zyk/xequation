#pragma once
#include <exception>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/uuid/uuid_io.hpp>
#include <tsl/ordered_map.h>
#include <tsl/ordered_set.h>

#include "dependency_graph.h"
#include "equation.h"
#include "equation_common.h"
#include "equation_signals_manager.h"

namespace rel
{
class Environment;
}

namespace xequation
{
using EquationPtr = std::unique_ptr<Equation>;
using EquationPtrOrderedMap = tsl::ordered_map<std::string, EquationPtr>;

class EquationException : public std::exception
{
  public:
    enum class ErrorCode
    {
        kEquationNotFound,
        kEquationAlreayExists,
        kExpressionNotFound,
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

    const ObjectId &id() const
    {
        return id_;
    }

    ErrorCode error_code() const
    {
        return error_code_;
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

  private:
    std::string GenerateErrorMessage() const
    {
        std::ostringstream oss;

        switch (error_code_)
        {
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

    EquationException(ErrorCode error_code, const ObjectId &id)
        : error_code_(error_code), id_(id)
    {
    }

    ErrorCode error_code_;
    std::string equation_name_;
    ObjectId id_;
    mutable std::string message_cache_;
};

// =========================================================================
//  EquationManager —— 名字 -> 表达式的依赖管理器（坍缩后直持 REL）
//
//  不再有 EquationContext / EquationEngine 抽象层；EquationManager 自身是
//  进程级单例（GetInstance()）：
//    - 变量表就是 rel::Environment（env() 可直接访问 / 注入 Dataset）；
//    - 表达式求值 = rel::Eval；依赖提取内聚在本文件（语法校验 + 依赖收集）；
//    - 一个 equation 表示 "把 Eval(content) 的结果绑定到 env 中的 name"。
//  每次 Update/UpdateEquation 时按依赖图拓扑序重算，失败的名字从环境移除
//  并保持节点 dirty，后续 Update 会重试。
//
//  引擎初始化（REL 内置常量 / 函数注册）在首次构造时完成（幂等）。
// =========================================================================
class EquationManager
{
  public:
    /// 进程级唯一 EquationManager（C++11 magic-static 懒构造，线程安全）。
    static EquationManager &GetInstance();

    virtual ~EquationManager() noexcept = default;

    // =========================================================================
    // 环境（变量表）
    // =========================================================================

    /// 直接访问底层 REL 环境（宿主读值 / 注入外部数据时用）。
    rel::Environment &env();
    const rel::Environment &env() const;

    /// 环境中是否已绑定该名字。
    bool HasVariable(const std::string &name) const;

    /// 读一个已绑定变量的值；未绑定返回 nullopt。
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
    ObjectId AddEquation(const std::string &equation_name, const std::string &expression);

    /// Replaces the content of an existing equation (name unchanged).
    ObjectId EditEquation(const std::string &equation_name, const std::string &expression);

    /// Renames an equation (removes the old name, defines the new one).
    /// Throws when the old name is not found or the new name already exists.
    ObjectId RenameEquation(const std::string &old_name, const std::string &new_name);

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
    /// env().Define(name, value) or context-free Eval).
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
    ObjectId AddExpression(const std::string &expression);

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
    EquationManager();  // 单例：仅 GetInstance() 可构造
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

    /// 变量表（直接使用 rel::Environment）。
    std::unique_ptr<rel::Environment> env_;
};

} // namespace xequation
