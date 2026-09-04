#include "equation_manager.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "environment.h"
#include "expr.h"
#include "rel.h"

namespace xequation
{
namespace
{
InterpretResult MapRelError(const std::exception &e)
{
    InterpretResult result;
    result.status = ResultStatus::kError;
    result.message = e.what();
    result.value = EquationValue();
    return result;
}

// Dependency collector: walks rel::Expr and gathers all "read" reference paths.
//   - ReferenceExpr collects paths: a single segment pushes the name; a
//     multi-segment path pushes its first segment (may be an equation name or
//     dataset reference) plus the full dotted path (DataArray / block path).
//   - a CallExpr whose callee is a single-segment registered function is a
//     call (callee is not a dependency, only args are); otherwise it's a
//     matrix index and the callee is a dependency;
//   - single-segment builtin constants are not dependencies;
//   - leaf nodes (number/bool/string/null range) have no dependencies.
class RelDependencyVisitor : public rel::ExprVisitor
{
  public:
    explicit RelDependencyVisitor(std::vector<std::string> &out) : out_(out) {}

    void visit_number(const rel::NumberExpr &) override {}
    void visit_boolean(const rel::BooleanExpr &) override {}
    void visit_string(const rel::StringExpr &) override {}
    void visit_null_range(const rel::NullRangeExpr &) override {}

    void visit_reference(const rel::ReferenceExpr &expr) override
    {
        CollectReference(expr);
    }

    void visit_unary(const rel::UnaryExpr &expr) override
    {
        if (expr.operand)
            expr.operand->accept(*this);
    }

    void visit_binary(const rel::BinaryExpr &expr) override
    {
        if (expr.left)
            expr.left->accept(*this);
        if (expr.right)
            expr.right->accept(*this);
    }

    void visit_logical(const rel::LogicalExpr &expr) override
    {
        if (expr.left)
            expr.left->accept(*this);
        if (expr.right)
            expr.right->accept(*this);
    }

    void visit_conditional(const rel::ConditionalExpr &expr) override
    {
        if (expr.condition)
            expr.condition->accept(*this);
        if (expr.then_branch)
            expr.then_branch->accept(*this);
        if (expr.else_branch)
            expr.else_branch->accept(*this);
    }

    void visit_if(const rel::IfExpr &expr) override
    {
        for (const auto &branch : expr.branches)
        {
            if (branch.condition)
                branch.condition->accept(*this);
            if (branch.value)
                branch.value->accept(*this);
        }
        if (expr.else_value)
            expr.else_value->accept(*this);
    }

    void visit_call(const rel::CallExpr &expr) override
    {
        // Call vs matrix index: a single-segment registered identifier is a
        // call (callee is not a dependency); otherwise it's an index and the
        // callee is a dependency.
        const rel::ReferenceExpr *ref =
            dynamic_cast<const rel::ReferenceExpr *>(expr.callee.get());
        const bool is_function_call =
            ref && ref->segments.size() == 1 &&
            rel::Environment::HasFunction(ref->segments[0].name);

        if (!is_function_call && expr.callee)
        {
            expr.callee->accept(*this);
        }
        for (const auto &arg : expr.args)
        {
            if (arg)
                arg->accept(*this);
        }
    }

    void visit_index(const rel::IndexExpr &expr) override
    {
        if (expr.object)
            expr.object->accept(*this);
        for (const auto &idx : expr.indices)
        {
            if (idx)
                idx->accept(*this);
        }
    }

    void visit_grouping(const rel::GroupingExpr &expr) override
    {
        if (expr.inner)
            expr.inner->accept(*this);
    }

    void visit_sweep(const rel::SweepExpr &expr) override
    {
        for (const auto &item : expr.items)
        {
            if (item)
                item->accept(*this);
        }
    }

    void visit_matrix(const rel::MatrixExpr &expr) override
    {
        for (const auto &item : expr.items)
        {
            if (item)
                item->accept(*this);
        }
    }

    void visit_range(const rel::RangeExpr &expr) override
    {
        if (expr.start)
            expr.start->accept(*this);
        if (expr.step)
            expr.step->accept(*this);
        if (expr.stop)
            expr.stop->accept(*this);
    }

  private:
    void CollectReference(const rel::ReferenceExpr &expr)
    {
        if (expr.segments.empty())
            return;

        // Single segment: registered function / builtin constant is not a dep.
        if (expr.segments.size() == 1)
        {
            const std::string &name = expr.segments[0].name;
            if (rel::Environment::HasFunction(name))
                return;
            if (rel::Environment::FindConstant(name) != nullptr)
                return;
            out_.push_back(name);
            return;
        }

        out_.push_back(expr.segments[0].name);

        std::string full_path;
        for (std::size_t i = 0; i < expr.segments.size(); ++i)
        {
            if (i > 0)
            {
                full_path += (expr.segments[i].sep == rel::RefSeparator::DDot) ? ".." : ".";
            }
            full_path += expr.segments[i].name;
        }
        out_.push_back(full_path);
    }

    std::vector<std::string> &out_;
};

// Deduplicate preserving order.
std::vector<std::string> Dedupe(const std::vector<std::string> &deps)
{
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (const auto &d : deps)
    {
        if (seen.insert(d).second)
        {
            result.push_back(d);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Parse a single REL expression: syntax check + dependency extraction.
// ---------------------------------------------------------------------------
ParseResult ParseRelExpression(const std::string &code)
{
    ParseResult result;
    result.status = ResultStatus::kSuccess;

    try
    {
        rel::ExprPtr expr = rel::Parse(code);
        std::vector<std::string> deps;
        if (expr)
        {
            RelDependencyVisitor visitor(deps);
            expr->accept(visitor);
        }
        result.symbols = Dedupe(deps);
    }
    catch (const std::exception &e)
    {
        result.status = ResultStatus::kError;
        result.message = e.what();
    }

    return result;
}
} // namespace

EquationManager::EquationManager()
    : graph_(new DependencyGraph()),
      signals_manager_(new EquationSignalsManager()),
      env_(new rel::Environment())
{
    // Idempotent: register REL builtin constants/functions once (safe to re-run).
    rel::Environment::InitBuiltinConstants();
    rel::Environment::InitBuiltinFunctions();
}

// =========================================================================
// Environment
// =========================================================================

rel::Environment &EquationManager::environment()
{
    return *env_;
}

const rel::Environment &EquationManager::environment() const
{
    return *env_;
}

bool EquationManager::HasVariable(const std::string &name) const
{
    const std::vector<std::string> names = env_->VariableNames();
    return std::find(names.begin(), names.end(), name) != names.end();
}

EquationValue EquationManager::GetVariable(const std::string &name) const
{
    if (!HasVariable(name))
    {
        return EquationValue();
    }
    return EquationValue(env_->Get(name));
}

EquationValue EquationManager::GetEquationValue(const std::string &equation_name) const
{
    // An equation's current value is whatever its name is bound to in the env.
    return GetVariable(equation_name);
}

std::string EquationManager::GraphNodeNameForObjectId(const ObjectId &object_id) const
{
    // Registered expression -> its internal "expr_<uuid>" graph slot.
    const auto expr_it = expression_id_to_name_map_.find(object_id);
    if (expr_it != expression_id_to_name_map_.end())
    {
        return expr_it->second;
    }
    // Equation -> its name (the graph node is keyed by the equation name).
    for (const auto &entry : equation_map_)
    {
        if (entry.second->id == object_id)
        {
            return entry.first;
        }
    }
    return std::string();
}

std::vector<std::string> EquationManager::GetDependencies(const ObjectId &object_id) const
{
    // User-facing dependencies of an object (Equation or registered
    // Expression): the names it reads (active graph edges).  Internal
    // registered-expression graph nodes ("expr_<uuid>") are filtered out.
    std::vector<std::string> result;
    const std::string node_name = GraphNodeNameForObjectId(object_id);
    if (node_name.empty())
    {
        return result;
    }
    const DependencyGraph::Node *node = graph_->GetNode(node_name);
    if (!node)
    {
        return result;
    }
    for (const std::string &name : node->dependencies())
    {
        if (!IsExpressionNode(name))
        {
            result.push_back(name);
        }
    }
    return result;
}

std::vector<std::string> EquationManager::GetDependents(const ObjectId &object_id) const
{
    // User-facing dependents of an object (Equation or registered Expression):
    // other names that read it (active graph edges).  Internal
    // registered-expression graph nodes ("expr_<uuid>") are filtered out.
    std::vector<std::string> result;
    const std::string node_name = GraphNodeNameForObjectId(object_id);
    if (node_name.empty())
    {
        return result;
    }
    const DependencyGraph::Node *node = graph_->GetNode(node_name);
    if (!node)
    {
        return result;
    }
    for (const std::string &name : node->dependents())
    {
        if (!IsExpressionNode(name))
        {
            result.push_back(name);
        }
    }
    return result;
}

// =========================================================================
// Equation CRUD (no equation groups; every equation stands on its own)
// =========================================================================

bool EquationManager::IsEquationExist(const std::string &equation_name) const
{
    return equation_map_.contains(equation_name);
}

bool EquationManager::IsEquationExist(const ObjectId &id) const
{
    return GetEquationById(id) != nullptr;
}

const Equation *EquationManager::GetEquation(const std::string &equation_name) const
{
    const auto it = equation_map_.find(equation_name);
    if (it == equation_map_.end())
    {
        return nullptr;
    }
    return it->second.get();
}

const Equation *EquationManager::GetEquationById(const ObjectId &id) const
{
    for (const auto &entry : equation_map_)
    {
        if (entry.second->id == id)
        {
            return entry.second.get();
        }
    }
    return nullptr;
}

Equation *EquationManager::GetEquationInternal(const std::string &equation_name)
{
    const auto it = equation_map_.find(equation_name);
    if (it == equation_map_.end())
    {
        return nullptr;
    }
    return it->second.get();
}

Equation *EquationManager::GetEquationInternal(const ObjectId &id)
{
    for (const auto &entry : equation_map_)
    {
        if (entry.second->id == id)
        {
            return entry.second.get();
        }
    }
    return nullptr;
}

std::vector<ObjectId> EquationManager::GetEquationIds() const
{
    std::vector<ObjectId> result;
    result.reserve(equation_map_.size());
    for (const auto &entry : equation_map_)
    {
        result.push_back(entry.second->id);
    }
    return result;
}

std::vector<std::string> EquationManager::GetEquationNames() const
{
    std::vector<std::string> result;
    result.reserve(equation_map_.size());
    for (const auto &entry : equation_map_)
    {
        result.push_back(entry.first);
    }
    return result;
}

ObjectId EquationManager::AddEquation(const std::string &equation_name, const std::string &expression,
                                      const std::string &tag)
{
    if (IsEquationExist(equation_name))
    {
        throw EquationException::EquationAlreadyExists(equation_name);
    }

    static const std::regex name_regex("^[A-Za-z_][A-Za-z0-9_]*$");
    if (!std::regex_match(equation_name, name_regex))
    {
        throw ParseException("Invalid equation name: " + equation_name);
    }

    // An equation is "name -> expression": parse the expression (which is the
    // content) for syntax + dependencies.  The name binding is performed by
    // Update/UpdateEquation (Eval then environment().Define).
    ParseResult res = Parse(expression);
    if (res.status != ResultStatus::kSuccess)
    {
        throw ParseException(res.message.empty() ? "Failed to parse expression: " + expression
                                                 : res.message);
    }

    std::vector<std::string> dependency_updated_equation;
    ScopedConnection dependency_connection = ConnectGraphDependencyUpdated(dependency_updated_equation);

    std::vector<std::string> dependent_updated_equation;
    ScopedConnection dependent_connection = ConnectGraphDependentUpdated(dependent_updated_equation);

    AddNodeToGraph(equation_name, res.symbols);
    graph_->InvalidateNode(equation_name);

    const ObjectId id = boost::uuids::random_generator()();
    EquationPtr equation(new Equation());
    equation->id = id;
    equation->name = equation_name;
    equation->content = expression;
    equation->status = ResultStatus::kPending;
    equation->parse_symbols = res.symbols;
    equation->tag = tag;   // opaque; empty stays empty (UI chooses defaults)

    Equation *equation_ptr = equation.get();
    equation_map_.insert({equation_name, std::move(equation)});
    signals_manager_->Emit<EquationEvent::kEquationAdded>(equation_ptr);

    for (const auto &equation_name_it : dependency_updated_equation)
    {
        NotifyEquationDependenciesUpdated(equation_name_it);
    }

    for (const auto &equation_name_it : dependent_updated_equation)
    {
        NotifyEquationDependentsUpdated(equation_name_it);
    }

    return id;
}

ObjectId EquationManager::EditEquation(const std::string &equation_name, const std::string &expression)
{
    Equation *equation = GetEquationInternal(equation_name);
    if (!equation)
    {
        throw EquationException::EquationNotFound(equation_name);
    }
    return EditEquation(equation->id, expression);
}

ObjectId EquationManager::EditEquation(const ObjectId &id, const std::string &expression)
{
    Equation *equation = GetEquationInternal(id);
    if (!equation)
    {
        throw EquationException::EquationNotFound(id);
    }
    const std::string equation_name = equation->name;
    if (equation->content == expression)
    {
        return equation->id;
    }

    // The name is already a valid identifier (it was validated when the
    // equation was added/renamed), so no re-validation is needed here.
    ParseResult res = Parse(expression);
    if (res.status != ResultStatus::kSuccess)
    {
        throw ParseException(res.message.empty() ? "Failed to parse expression: " + expression
                                                 : res.message);
    }

    std::vector<std::string> dependency_updated_equation;
    ScopedConnection dependency_connection = ConnectGraphDependencyUpdated(dependency_updated_equation);

    std::vector<std::string> dependent_updated_equation;
    ScopedConnection dependent_connection = ConnectGraphDependentUpdated(dependent_updated_equation);

    // Update the graph: drop the old dependency edges, add the new ones.
    // Dependents that lost their dependency on this equation must be re-dirtied
    // (the edge is deactivated but the dirty flag is not touched by RemoveNode).
    RemoveNodeInGraph(equation_name);
    auto orphan_edge_range = graph_->GetEdgesByTo(equation_name);
    for (auto it = orphan_edge_range.first; it != orphan_edge_range.second; it++)
    {
        graph_->InvalidateNode(it->from());
    }
    AddNodeToGraph(equation_name, res.symbols);
    graph_->InvalidateNode(equation_name);

    equation = GetEquationInternal(equation_name);
    equation->content = expression;
    equation->status = ResultStatus::kPending;
    equation->message.clear();
    equation->parse_symbols = res.symbols;
    env_->Remove(equation_name);
    signals_manager_->Emit<EquationEvent::kEquationUpdated>(
        equation, EquationUpdateFlag::kContent | EquationUpdateFlag::kStatus | EquationUpdateFlag::kMessage
    );

    for (const auto &eqn_name : dependency_updated_equation)
    {
        NotifyEquationDependenciesUpdated(eqn_name);
    }

    for (const auto &eqn_name : dependent_updated_equation)
    {
        NotifyEquationDependentsUpdated(eqn_name);
    }

    return equation->id;
}

ObjectId EquationManager::RenameEquation(const std::string &old_name, const std::string &new_name)
{
    Equation *equation = GetEquationInternal(old_name);
    if (!equation)
    {
        throw EquationException::EquationNotFound(old_name);
    }
    return RenameEquation(equation->id, new_name);
}

ObjectId EquationManager::RenameEquation(const ObjectId &id, const std::string &new_name)
{
    Equation *equation = GetEquationInternal(id);
    if (!equation)
    {
        throw EquationException::EquationNotFound(id);
    }
    const std::string old_name = equation->name;  // copy: name is reassigned below
    if (IsEquationExist(new_name))
    {
        throw EquationException::EquationAlreadyExists(new_name);
    }

    static const std::regex name_regex("^[A-Za-z_][A-Za-z0-9_]*$");
    if (!std::regex_match(new_name, name_regex))
    {
        throw ParseException("Invalid equation name: " + new_name);
    }

    // The content (expression) is unchanged by a rename: re-parse it only to
    // rebuild the graph edges under the new node name.
    ParseResult res = Parse(equation->content);
    if (res.status != ResultStatus::kSuccess)
    {
        throw ParseException(res.message.empty() ? "Failed to parse expression: " + equation->content
                                                 : res.message);
    }

    std::vector<std::string> dependency_updated_equation;
    ScopedConnection dependency_connection = ConnectGraphDependencyUpdated(dependency_updated_equation);

    std::vector<std::string> dependent_updated_equation;
    ScopedConnection dependent_connection = ConnectGraphDependentUpdated(dependent_updated_equation);

    RemoveNodeInGraph(old_name);
    auto orphan_edge_range = graph_->GetEdgesByTo(old_name);
    for (auto it = orphan_edge_range.first; it != orphan_edge_range.second; it++)
    {
        graph_->InvalidateNode(it->from());
    }
    AddNodeToGraph(new_name, res.symbols);
    graph_->InvalidateNode(new_name);

    // Move the equation to its new name (same id: a rename, not a recreate).
    Equation *moved = GetEquationInternal(old_name);
    EquationPtr holder = std::move(equation_map_[old_name]);
    equation_map_.erase(old_name);
    moved->name = new_name;
    moved->status = ResultStatus::kPending;
    moved->parse_symbols = res.symbols;
    env_->Remove(old_name);
    equation_map_.insert({new_name, std::move(holder)});

    signals_manager_->Emit<EquationEvent::kEquationUpdated>(
        moved, EquationUpdateFlag::kName | EquationUpdateFlag::kStatus
    );

    for (const auto &eqn_name : dependency_updated_equation)
    {
        NotifyEquationDependenciesUpdated(eqn_name);
    }

    for (const auto &eqn_name : dependent_updated_equation)
    {
        NotifyEquationDependentsUpdated(eqn_name);
    }

    return id;
}

void EquationManager::RemoveEquation(const std::string &equation_name)
{
    if (!IsEquationExist(equation_name))
    {
        return;
    }
    Equation *equation = GetEquationInternal(equation_name);

    std::vector<std::string> dependency_updated_equation;
    ScopedConnection dependency_connection = ConnectGraphDependencyUpdated(dependency_updated_equation);

    std::vector<std::string> dependent_updated_equation;
    ScopedConnection dependent_connection = ConnectGraphDependentUpdated(dependent_updated_equation);

    RemoveNodeInGraph(equation_name);
    // Dependents that lost their dependency on the removed equation must be
    // re-dirtied so a later Update recomputes them (otherwise they keep stale
    // values/status, e.g. a NameError that is never surfaced).
    auto orphan_edge_range = graph_->GetEdgesByTo(equation_name);
    for (auto it = orphan_edge_range.first; it != orphan_edge_range.second; it++)
    {
        graph_->InvalidateNode(it->from());
    }
    graph_->InvalidateNode(equation_name);
    signals_manager_->Emit<EquationEvent::kEquationRemoving>(equation);
    equation_map_.erase(equation_name);
    env_->Remove(equation_name);
    signals_manager_->Emit<EquationEvent::kEquationRemoved>(equation_name);

    for (const auto &eqn_name : dependency_updated_equation)
    {
        NotifyEquationDependenciesUpdated(eqn_name);
    }

    for (const auto &eqn_name : dependent_updated_equation)
    {
        NotifyEquationDependentsUpdated(eqn_name);
    }
}

void EquationManager::RemoveEquation(const ObjectId &id)
{
    for (const auto &entry : equation_map_)
    {
        if (entry.second->id == id)
        {
            RemoveEquation(entry.first);
            return;
        }
    }
}

ParseResult EquationManager::Parse(const std::string &expression) const
{
    return ParseRelExpression(expression);
}

InterpretResult EquationManager::Eval(const std::string &expression) const
{
    try
    {
        rel::Value value = rel::Eval(expression, env_.get());
        InterpretResult result;
        result.status = ResultStatus::kSuccess;
        result.value = EquationValue(value);
        return result;
    }
    catch (const std::exception &e)
    {
        return MapRelError(e);
    }
}

void EquationManager::Reset()
{
    ClearState();
    signals_manager_->DisconnectAllEvent();
}

void EquationManager::ClearState()
{
    graph_->Reset();

    for (const auto &equation_entry : equation_map_)
    {
        signals_manager_->Emit<EquationEvent::kEquationRemoving>(equation_entry.second.get());
    }
    equation_map_.clear();
    for (const auto &expr_entry : expression_map_)
    {
        signals_manager_->Emit<EquationEvent::kExpressionRemoving>(&expr_entry.second);
    }
    expression_map_.clear();
    expression_id_to_name_map_.clear();
    external_input_names_.clear();
    env_->Clear();
}

void EquationManager::ResetContext()
{
    env_->Clear();
    // The environment has been cleared, so every value is gone. Re-dirty all nodes so that a
    // subsequent Update()/UpdateEquation() recalculates everything.
    // (Previously this relied on "dirty is never cleared"; now that updates clear dirty on
    // success, we must re-dirty explicitly.)
    graph_->Traversal([&](const std::string &equation_name) {
        graph_->SetNodeDirty(equation_name, true);
    });
}

void EquationManager::UpdateEquationInternal(const std::string &equation_name)
{
    if (!IsEquationExist(equation_name))
    {
        throw EquationException::EquationNotFound(equation_name);
    }

    Equation *equation = GetEquationInternal(equation_name);

    // set status and message to calculating before calculation
    equation->status = ResultStatus::kCalculating;
    equation->message = "Calculating...";

    signals_manager_->Emit<EquationEvent::kEquationUpdated>(
        equation, EquationUpdateFlag::kStatus | EquationUpdateFlag::kMessage
    );

    // Binding model: an equation is "name -> expression".  Eval the expression
    // (pure; never writes), then bind the result to the name in the env.
    InterpretResult result = Eval(equation->content);
    equation->status = result.status;
    equation->message = result.message;
    if (equation->status != ResultStatus::kSuccess)
    {
        env_->Remove(equation_name);
    }
    else
    {
        env_->Define(equation_name, result.value.Value());
        // Clear the dirty flag on success; on failure (e.g. NameError) keep it dirty so the
        // next Update/UpdateEquation retries until it succeeds.
        graph_->SetNodeDirty(equation_name, false);
    }
    signals_manager_->Emit<EquationEvent::kEquationUpdated>(
        equation, EquationUpdateFlag::kStatus | EquationUpdateFlag::kMessage | EquationUpdateFlag::kValue
    );
}

void EquationManager::AddNodeToGraph(const std::string &node_name, const std::vector<std::string> &dependencies)
{
    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    graph_->AddNode(node_name);
    const DependencyGraph::EdgeContainer::RangeByFrom edges = graph_->GetEdgesByFrom(node_name);
    std::vector<DependencyGraph::Edge> edges_to_remove;
    for (auto it = edges.first; it != edges.second; it++)
    {
        edges_to_remove.push_back(*it);
    }
    graph_->RemoveEdges(edges_to_remove);
    for (const std::string &dep : dependencies)
    {
        graph_->AddEdge({node_name, dep});
    }
    guard.commit();
}

void EquationManager::RemoveNodeInGraph(const std::string &node_name)
{
    graph_->RemoveNode(node_name);
    auto edges = graph_->GetEdgesByFrom(node_name);
    std::vector<DependencyGraph::Edge> edges_to_remove;
    for (auto it = edges.first; it != edges.second; it++)
    {
        edges_to_remove.push_back(*it);
    }
    graph_->RemoveEdges(edges_to_remove);
}

void EquationManager::Update()
{
    graph_->Traversal([&](const std::string &node_name) { UpdateNode(node_name); });
}

void EquationManager::UpdateEquation(const std::string &equation_name)
{
    if (!IsEquationExist(equation_name))
    {
        throw EquationException::EquationNotFound(equation_name);
    }

    // Update scope = the target equation + all of its dependents (propagated by
    // TopologicalSort) plus every dirty (invalidated) node. Renaming/removing a variable
    // dirties equations that lost their dependency (e.g. in "a=1;b=a", renaming "a" to
    // "c" dirties "b"). There is no live edge from "c" to "b" anymore, so reachability
    // from "c" alone would miss it; it must be recomputed explicitly or it keeps stale
    // values/status.
    std::vector<std::string> update_names = graph_->TopologicalSort(equation_name);
    CollectDirtyNodes(update_names);

    auto topo_order = graph_->TopologicalSort(update_names);

    for (const auto &node_name : topo_order)
    {
        UpdateNode(node_name);
    }
}

std::vector<std::string> EquationManager::GetEquationsToUpdate(const std::string &equation_name) const
{
    if (!IsEquationExist(equation_name))
    {
        return {};
    }

    std::vector<std::string> update_names = graph_->TopologicalSort(equation_name);
    CollectDirtyNodes(update_names);
    return graph_->TopologicalSort(update_names);
}

// =========================================================================
// Registered expressions (observe-only)
// =========================================================================

ObjectId EquationManager::AddExpression(const std::string &expression, const std::string &tag)
{
    static boost::uuids::random_generator rgen;

    ParseResult parse_result = Parse(expression);

    Expression expr;
    expr.id = rgen();
    const std::string name = "expr_" + boost::uuids::to_string(expr.id);
    expr.content = expression;
    expr.tag = tag;   // opaque; empty stays empty (UI chooses defaults)
    // Register even on syntax errors: status/message are recorded on the
    // expression, and Update recomputes (and keeps it dirty) on failure.
    expr.result.status = parse_result.status;
    expr.result.message = parse_result.message;
    expr.parse_symbols = parse_result.symbols;

    // The expression node + its dependency edges (dependencies may not have graph
    // nodes yet -- e.g. an equation defined by a later AddEquation; edges stay
    // inactive until both endpoints exist).
    AddNodeToGraph(name, expr.parse_symbols);
    // Dirty the expression so it is computed on the next Update/UpdateExpression.
    graph_->InvalidateNode(name);

    const ObjectId id = expr.id;
    expression_map_.insert({name, std::move(expr)});
    expression_id_to_name_map_.insert({id, name});
    const Expression *added = &expression_map_.at(name);
    signals_manager_->Emit<EquationEvent::kExpressionAdded>(added);
    return id;
}

void EquationManager::RemoveExpression(const ObjectId &id)
{
    const auto it = expression_id_to_name_map_.find(id);
    if (it == expression_id_to_name_map_.end())
    {
        return;
    }
    const std::string &name = it->second;
    RemoveNodeInGraph(name);
    const Expression *removing = &expression_map_.at(name);
    signals_manager_->Emit<EquationEvent::kExpressionRemoving>(removing);
    expression_map_.erase(name);
    expression_id_to_name_map_.erase(it);
    signals_manager_->Emit<EquationEvent::kExpressionRemoved>(boost::uuids::to_string(id));
}

const Expression *EquationManager::GetExpression(const ObjectId &id) const
{
    const auto it = expression_id_to_name_map_.find(id);
    if (it == expression_id_to_name_map_.end())
    {
        return nullptr;
    }
    const auto expr_it = expression_map_.find(it->second);
    if (expr_it == expression_map_.end())
    {
        return nullptr;
    }
    return &expr_it->second;
}

bool EquationManager::IsExpressionExist(const ObjectId &id) const
{
    return expression_id_to_name_map_.count(id) != 0;
}

bool EquationManager::IsExpressionNode(const std::string &name) const
{
    // The expression_map_ is keyed by the internal graph-node slot
    // ("expr_<uuid>"), which is what shows up in dependency / dependent sets.
    return expression_map_.count(name) != 0;
}

std::vector<ObjectId> EquationManager::GetExpressionIds() const
{
    std::vector<ObjectId> ids;
    ids.reserve(expression_map_.size());
    for (const auto &entry : expression_map_)
    {
        ids.push_back(entry.second.id);
    }
    return ids;
}

EquationValue EquationManager::GetExpressionValue(const ObjectId &id) const
{
    const Expression *expr = GetExpression(id);
    if (expr == nullptr)
    {
        return EquationValue();
    }
    return expr->result.value;
}

void EquationManager::UpdateExpression(const ObjectId &id)
{
    if (!IsExpressionExist(id))
    {
        throw EquationException::ExpressionNotFound(boost::uuids::to_string(id));
    }
    UpdateExpressionInternal(id);
}

void EquationManager::UpdateExpressionInternal(const ObjectId &id)
{
    if (!IsExpressionExist(id))
    {
        return;
    }
    const std::string &name = expression_id_to_name_map_.at(id);
    Expression *expr = &expression_map_.at(name);

    // set status and message to calculating before calculation
    expr->result.status = ResultStatus::kCalculating;
    expr->result.message = "Calculating...";
    signals_manager_->Emit<EquationEvent::kExpressionUpdated>(
        expr, ExpressionUpdateFlag::kStatus | ExpressionUpdateFlag::kMessage
    );

    InterpretResult eval_result = Eval(expr->content);
    expr->result = eval_result;
    // Keep the node dirty on failure so a later Update() retries it (mirrors
    // UpdateEquationInternal); clear it on success.
    graph_->SetNodeDirty(name, eval_result.status != ResultStatus::kSuccess);
    signals_manager_->Emit<EquationEvent::kExpressionUpdated>(
        expr, ExpressionUpdateFlag::kStatus | ExpressionUpdateFlag::kMessage | ExpressionUpdateFlag::kValue
    );
}

// =========================================================================
// External input symbols
// =========================================================================

bool EquationManager::AddExternalInput(const std::string &symbol_name)
{
    if (symbol_name.empty())
    {
        return false;
    }
    // Name conflicts are not allowed: equations, expressions and external inputs
    // share the graph name space.
    if (IsEquationExist(symbol_name) || expression_map_.count(symbol_name) != 0 ||
        external_input_names_.contains(symbol_name))
    {
        return false;
    }
    graph_->AddNode(symbol_name);
    external_input_names_.insert(symbol_name);
    return true;
}

void EquationManager::RemoveExternalInput(const std::string &symbol_name)
{
    if (external_input_names_.erase(symbol_name) != 0)
    {
        RemoveNodeInGraph(symbol_name);
    }
}

bool EquationManager::IsExternalInput(const std::string &symbol_name) const
{
    return external_input_names_.contains(symbol_name);
}

std::vector<std::string> EquationManager::GetExternalInputNames() const
{
    std::vector<std::string> names;
    names.reserve(external_input_names_.size());
    for (const auto &name : external_input_names_)
    {
        names.push_back(name);
    }
    return names;
}

void EquationManager::InvalidateExternalInputs(const std::vector<std::string> &symbol_names)
{
    // ① invalidate every input first: dirty flags propagate to all graph dependents.
    for (const auto &name : symbol_names)
    {
        if (graph_->IsNodeExist(name))
        {
            graph_->InvalidateNode(name);
        }
    }
    // ② merge the update scopes: TopologicalSort(vector) builds one relevant set,
    // so a dependent of several inputs appears exactly once.
    std::vector<std::string> update_names;
    for (const auto &name : symbol_names)
    {
        auto scope = graph_->TopologicalSort(name);
        update_names.insert(update_names.end(), scope.begin(), scope.end());
    }
    // ③ collect dirty nodes (renames/removals may have left unreachable dirty
    // nodes that TopologicalSort from the inputs cannot reach).
    CollectDirtyNodes(update_names);

    auto topo_order = graph_->TopologicalSort(update_names);
    for (const auto &node_name : topo_order)
    {
        UpdateNode(node_name);
    }
}

void EquationManager::UpdateNode(const std::string &node_name)
{
    if (equation_map_.count(node_name) != 0)
    {
        UpdateEquationInternal(node_name);
        return;
    }
    const auto expr_it = expression_map_.find(node_name);
    if (expr_it != expression_map_.end())
    {
        UpdateExpressionInternal(expr_it->second.id);
        return;
    }
    // External inputs and unknown/unresolved names have nothing to compute.
}

void EquationManager::CollectDirtyNodes(std::vector<std::string> &update_names) const
{
    graph_->Traversal([&](const std::string &node_name) {
        const DependencyGraph::Node *node = graph_->GetNode(node_name);
        if (node != nullptr && node->dirty_flag())
        {
            update_names.push_back(node_name);
        }
    });
}

void EquationManager::UpdateEquationStatus(const std::string &equation_name, ResultStatus status, const std::string& message)
{
    if (!IsEquationExist(equation_name))
    {
        throw EquationException::EquationNotFound(equation_name);
    }

    Equation *equation = GetEquationInternal(equation_name);
    equation->status = status;
    equation->message = message;
    env_->Remove(equation_name);
    // On failure (e.g. KeyBoardInterrupt) the value was removed from the context; re-dirty
    // the node so a later Update recomputes it (otherwise, under the "clear on success"
    // semantics, the node may already be clean and the value would be lost forever).
    graph_->SetNodeDirty(equation_name, true);

    signals_manager_->Emit<EquationEvent::kEquationUpdated>(
        equation, EquationUpdateFlag::kStatus | EquationUpdateFlag::kMessage | EquationUpdateFlag::kValue
    );
}

ScopedConnection EquationManager::ConnectGraphDependencyUpdated(std::vector<std::string> &dependency_updated_equation
) const
{
    return graph_->ConnectNodeDependencyChangedSignal([&](const std::string &node_name) {
        dependency_updated_equation.push_back(node_name);
    });
}

ScopedConnection EquationManager::ConnectGraphDependentUpdated(std::vector<std::string> &dependent_updated_equation
) const
{
    return graph_->ConnectNodeDependentChangedSignal([&](const std::string &node_name) {
        dependent_updated_equation.push_back(node_name);
    });
}

void EquationManager::NotifyEquationDependentsUpdated(const std::string &equation_name) const
{
    if (IsEquationExist(equation_name))
    {
        signals_manager_->Emit<EquationEvent::kEquationUpdated>(
            GetEquation(equation_name), EquationUpdateFlag::kDependents
        );
    }
}

void EquationManager::NotifyEquationDependenciesUpdated(const std::string &equation_name) const
{
    if (IsEquationExist(equation_name))
    {
        signals_manager_->Emit<EquationEvent::kEquationUpdated>(
            GetEquation(equation_name), EquationUpdateFlag::kDependencies
        );
    }
}

std::string EquationManager::GenerateEquationDotNodeLabel(const std::string &equation_name) const
{
    const Equation *equation = GetEquation(equation_name);
    if (!equation)
    {
        return equation_name;
    }

    // Helper lambda to escape text for record labels
    auto escape_for_record = [](const std::string& text) -> std::string {
        std::string escaped;
        for (char c : text)
        {
            // Escape all special characters for record format
            if (c == '{' || c == '}' || c == '|' || c == '<' || c == '>')
            {
                escaped += "\\";
                escaped += c;
            }
            else if (c == '"')
            {
                escaped += "\\\"";
            }
            else if (c == '\\')
            {
                escaped += "\\\\";
            }
            else if (c == '\n')
            {
                escaped += "\\n";
            }
            else if (c == '\r')
            {
                // Skip carriage return
            }
            else if (c == '\t')
            {
                escaped += "    "; // Replace tab with spaces
            }
            else
            {
                escaped += c;
            }
        }
        return escaped;
    };

    // Escape equation name
    std::string escaped_name = escape_for_record(equation_name);

    // Truncate content if too long
    std::string content = equation->content;
    const size_t max_content_length = 100;
    bool truncated = false;

    if (content.length() > max_content_length)
    {
        content = content.substr(0, max_content_length);
        truncated = true;
    }

    if (truncated)
    {
        content += "...";
    }

    // Escape content
    std::string escaped_content = escape_for_record(content);

    // Build record format: {name|content}
    std::ostringstream oss;
    oss << "{" << escaped_name << "|" << escaped_content << "}";
    return oss.str();
}

bool EquationManager::WriteDependencyGraphToDotFile(const std::string &file_path) const
{
    return graph_->WriteDotFile(file_path, [this](const std::string &node_name) {
        return GenerateEquationDotNodeLabel(node_name);
    });
}

// =========================================================================
//  Persistence
// =========================================================================

namespace
{

// Escape a string for inclusion in a JSON string literal.
std::string JsonEscape(const std::string &s)
{
    std::ostringstream oss;
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"':  oss << "\\\""; break;
        case '\\': oss << "\\\\"; break;
        case '\b': oss << "\\b";  break;
        case '\f': oss << "\\f";  break;
        case '\n': oss << "\\n";  break;
        case '\r': oss << "\\r";  break;
        case '\t': oss << "\\t";  break;
        default:
            if (c < 0x20)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                oss << buf;
            }
            else
            {
                oss << static_cast<char>(c);
            }
        }
    }
    return oss.str();
}

// Minimal pretty-printer (boost::json::serialize is compact only).
std::string PrettyJson(const boost::json::value &v, int indent)
{
    const std::string pad(indent * 2, ' ');
    const std::string pad_next((indent + 1) * 2, ' ');

    if (v.is_object())
    {
        const boost::json::object &obj = v.as_object();
        if (obj.empty())
            return "{}";
        std::ostringstream oss;
        oss << "{\n";
        bool first = true;
        for (const auto &kv : obj)
        {
            if (!first)
                oss << ",\n";
            first = false;
            boost::json::string_view key = kv.key();
            oss << pad_next << "\"" << std::string(key.data(), key.size()) << "\": "
                << PrettyJson(kv.value(), indent + 1);
        }
        oss << "\n" << pad << "}";
        return oss.str();
    }
    if (v.is_array())
    {
        const boost::json::array &arr = v.as_array();
        if (arr.empty())
            return "[]";
        std::ostringstream oss;
        oss << "[\n";
        bool first = true;
        for (const auto &item : arr)
        {
            if (!first)
                oss << ",\n";
            first = false;
            oss << pad_next << PrettyJson(item, indent + 1);
        }
        oss << "\n" << pad << "]";
        return oss.str();
    }
    if (v.is_string())
    {
        const boost::json::string& s = v.as_string();
        return "\"" + JsonEscape(std::string(s.data(), s.size())) + "\"";
    }
    if (v.is_int64())
        return std::to_string(v.as_int64());
    if (v.is_uint64())
        return std::to_string(v.as_uint64());
    if (v.is_double())
    {
        std::ostringstream oss;
        oss << v.as_double();
        return oss.str();
    }
    if (v.is_bool())
        return v.as_bool() ? "true" : "false";
    return "null";
}

// Read a string member if present and a string, else return `def`.
std::string GetString(const boost::json::object &o, const char *key,
                      const std::string &def = std::string())
{
    auto it = o.find(key);
    if (it == o.end() || !it->value().is_string())
        return def;
    const boost::json::string& s = it->value().as_string();
    return std::string(s.data(), s.size());
}

} // anonymous namespace

void EquationManager::SaveToFile(const std::string &path) const
{
    namespace json = boost::json;

    json::object doc;
    doc["version"] = 1;

    // ---- datasets (references to dataset files, not their data) ----
    {
        json::array datasets;
        for (const auto &ds : rel::Environment::DatasetConfigs())
        {
            json::object o;
            o["name"]   = json::string(ds.name);
            o["format"] = json::string(ds.format);
            o["path"]   = json::string(ds.path);
            datasets.push_back(std::move(o));
        }
        doc["datasets"] = std::move(datasets);

        const std::string default_ds = rel::Environment::DefaultDatasetName();
        if (!default_ds.empty())
            doc["default_dataset"] = json::string(default_ds);

        const std::vector<std::string> plugins = rel::Environment::PythonPlugins();
        if (!plugins.empty())
        {
            json::array pj;
            for (const auto &p : plugins)
                pj.push_back(json::string(p));
            doc["python_plugins"] = std::move(pj);
        }
    }

    // ---- equations (insertion order) ----
    {
        json::array equations;
        for (const auto &entry : equation_map_)
        {
            const Equation &e = *entry.second;
            json::object o;
            o["name"]    = json::string(e.name);
            o["content"] = json::string(e.content);
            o["tag"]     = json::string(e.tag);
            equations.push_back(std::move(o));
        }
        doc["equations"] = std::move(equations);
    }

    // ---- registered expressions (source only; id is regenerated on load) ----
    {
        json::array expressions;
        for (const auto &entry : expression_map_)
        {
            const Expression &e = entry.second;
            json::object o;
            o["content"] = json::string(e.content);
            o["tag"]     = json::string(e.tag);
            expressions.push_back(std::move(o));
        }
        doc["expressions"] = std::move(expressions);
    }

    std::ofstream out(path);
    if (!out)
        throw std::runtime_error("cannot write equation state file: " + path);
    out << PrettyJson(doc, 0);
    out.close();
}

void EquationManager::LoadFromFile(const std::string &path)
{
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("cannot open equation state file: " + path);

    std::stringstream buf;
    buf << in.rdbuf();

    boost::json::value parsed;
    try
    {
        parsed = boost::json::parse(buf.str());
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error("JSON parse error in " + path +
                                 ": " + e.what());
    }
    if (!parsed.is_object())
        throw std::runtime_error("project file root must be a JSON object: " + path);

    const boost::json::object &doc = parsed.as_object();

    // Clear current state (equations / expressions / external inputs / env),
    // preserving external signal observers so the host UI stays connected.
    ClearState();

    // ---- load datasets from the project file's "datasets" section ----
    // The project file references dataset files (not their data).  Reuse
    // Environment's in-memory LoadFromConfig, resolving relative paths
    // against the project file's directory.
    auto datasets_it = doc.find("datasets");
    if (datasets_it != doc.end() && datasets_it->value().is_array())
    {
        rel::EnvironmentConfig env_cfg = rel::EnvironmentConfig::Parse(doc);
        const std::string base_dir =
            boost::filesystem::path(path).parent_path().string();
        rel::Environment::LoadFromConfig(env_cfg, base_dir);
    }

    // ---- equations ----
    auto equations_it = doc.find("equations");
    if (equations_it != doc.end() && equations_it->value().is_array())
    {
        for (const auto &v : equations_it->value().as_array())
        {
            if (!v.is_object())
                continue;
            const boost::json::object &o = v.as_object();
            AddEquation(GetString(o, "name"), GetString(o, "content"),
                        GetString(o, "tag"));
        }
    }

    // ---- expressions ----
    auto expressions_it = doc.find("expressions");
    if (expressions_it != doc.end() && expressions_it->value().is_array())
    {
        for (const auto &v : expressions_it->value().as_array())
        {
            if (!v.is_object())
                continue;
            const boost::json::object &o = v.as_object();
            AddExpression(GetString(o, "content"), GetString(o, "tag"));
        }
    }

    // Recompute all values (equations bound to env; expressions computed).
    Update();
}

EquationManager &EquationManager::GetInstance()
{
    // C++11 magic-static, thread-safe lazy construction. REL builtin
    // constants/functions are registered during construction (idempotent).
    static EquationManager instance;
    return instance;
}

} // namespace xequation
