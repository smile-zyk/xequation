#include <regex>
#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "equation_manager.h"
#include "equation_common.h"
#include "core/equation_signals_manager.h"

namespace xequation
{
EquationManager::EquationManager(
    std::unique_ptr<EquationContext> context, EvalHandler eval_handler, ExecHandler exec_handler, ParseHandler parse_handler, const EquationEngineInfo &engine_info
) noexcept
    : graph_(std::unique_ptr<DependencyGraph>(new DependencyGraph())),
      signals_manager_(std::unique_ptr<EquationSignalsManager>(new EquationSignalsManager())),
      context_(std::move(context)),
      eval_handler_(eval_handler),
      exec_handler_(exec_handler),
      parse_handler_(parse_handler),
      engine_info_(engine_info)
{
}

bool EquationManager::IsEquationGroupExist(const EquationGroupId &group_id) const
{
    return equation_group_map_.contains(group_id);
}

bool EquationManager::IsEquationExist(const std::string &equation_name) const
{
    bool is_name_exist = equation_name_to_group_id_map_.count(equation_name) != 0;
    if (!is_name_exist)
    {
        return false;
    }

    const EquationGroupId &id = equation_name_to_group_id_map_.at(equation_name);
    bool is_group_exist = equation_group_map_.contains(id);
    if (!is_group_exist)
    {
        return false;
    }

    const EquationGroup *group = equation_group_map_.at(id).get();
    bool is_equation_exist = group->IsEquationExist(equation_name);
    return is_equation_exist;
}

const EquationGroup *EquationManager::GetEquationGroup(const EquationGroupId &group_id) const
{
    if (equation_group_map_.contains(group_id))
    {
        return equation_group_map_.at(group_id).get();
    }
    return nullptr;
}

const EquationGroup *EquationManager::GetEquationGroup(const std::string &equation_name) const
{
    const auto it = equation_name_to_group_id_map_.find(equation_name);
    if (it == equation_name_to_group_id_map_.end())
    {
        return nullptr;
    }
    return GetEquationGroup(it->second);
}

const Equation *EquationManager::GetEquation(const std::string &equation_name) const
{
    bool is_name_exist = equation_name_to_group_id_map_.count(equation_name) != 0;
    if (!is_name_exist)
    {
        return nullptr;
    }

    const EquationGroupId &id = equation_name_to_group_id_map_.at(equation_name);
    bool is_group_exist = equation_group_map_.contains(id);
    if (!is_group_exist)
    {
        return nullptr;
    }

    const EquationGroup *group = equation_group_map_.at(id).get();
    bool is_equation_exist = group->IsEquationExist(equation_name);
    if (!is_equation_exist)
    {
        return nullptr;
    }

    return group->GetEquation(equation_name);
}

std::vector<EquationGroupId> EquationManager::GetEquationGroupIds() const
{
    std::vector<EquationGroupId> result;
    for (const auto &entry : equation_group_map_)
    {
        result.push_back(entry.first);
    }
    return result;
}

std::vector<std::string> EquationManager::GetEquationNames() const
{
    std::vector<std::string> result;
    for( const auto &entry : equation_group_map_)
    {
        const EquationGroup *group = entry.second.get();
        auto equation_names = group->GetEquationNames();
        result.insert(result.end(), equation_names.begin(), equation_names.end());
    }
    return result;
}

EquationGroupId EquationManager::AddEquationGroup(const std::string &equation_statement)
{
    auto res = Parse(equation_statement, ParseMode::kStatement);

    for (const auto &item : res.items)
    {
        if (IsEquationExist(item.name))
        {
            throw EquationException::EquationAlreadyExists(item.name);
        }
    }

    std::vector<std::string> dependency_updated_equation;
    ScopedConnection dependency_connection = ConnectGraphDependencyUpdated(dependency_updated_equation);

    std::vector<std::string> dependent_updated_equation;
    ScopedConnection dependent_connection = ConnectGraphDependentUpdated(dependent_updated_equation);

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    for (const auto &item : res.items)
    {
        AddNodeToGraph(item.name, item.dependencies);
    }
    guard.commit();

    EquationGroupPtr group = EquationGroup::Create(this);
    group->set_statement(equation_statement);
    const EquationGroupId &id = group->id();
    auto group_ptr = group.get();
    equation_group_map_.insert({id, std::move(group)});
    for (const auto &item : res.items)
    {
        graph_->InvalidateNode(item.name);
        EquationPtr equation = Equation::Create(item, id, this);
        AddEquationToGroup(group_ptr, std::move(equation));
        signals_manager_->Emit<EquationEvent::kEquationAdded>(group_ptr->GetEquation(item.name));
    }
    signals_manager_->Emit<EquationEvent::kEquationGroupAdded>(group_ptr);

    for (const auto &equation_name : dependency_updated_equation)
    {
        NotifyEquationDependenciesUpdated(equation_name);
    }

    for (const auto &equation_name : dependent_updated_equation)
    {
        NotifyEquationDependentsUpdated(equation_name);
    }

    return id;
}

EquationGroupId EquationManager::AddEquation(const std::string& equation_name, const std::string& equation_content)
{
    if (IsEquationExist(equation_name))
    {
        throw EquationException::EquationAlreadyExists(equation_name);
    }

    // use regex to validate equation name
    static const std::regex name_regex("^[A-Za-z_][A-Za-z0-9_]*$");
    if (!std::regex_match(equation_name, name_regex))
    {
        throw ParseException("Invalid equation name: " + equation_name);
    }

    std::string statement = equation_name + " = " + equation_content;

    auto res = Parse(statement, ParseMode::kStatement);

    if(res.items.size() != 1)
    {
        throw ParseException("Failed to parse single equation: " + statement);
    }

    return AddEquationGroup(statement);
}

void EquationManager::EditEquationGroup(const EquationGroupId &group_id, const std::string &equation_statement)
{
    if (IsEquationGroupExist(group_id) == false)
    {
        throw EquationException::EquationGroupNotFound(group_id);
    }

    EquationGroup *group = GetEquationGroupInternal(group_id);

    if (group->statement() == equation_statement)
    {
        return;
    }

    const EquationPtrOrderedMap &old_name_equation_map = group->equation_map();

    ParseResult new_result = Parse(equation_statement, ParseMode::kStatement);
    std::unordered_map<std::string, ParseResultItem> new_name_item_map;
    for (const auto &item : new_result.items)
    {
        new_name_item_map.insert({item.name, item});
    }

    for (const auto &new_item : new_result.items)
    {
        if (group->IsEquationExist(new_item.name) == false && IsEquationExist(new_item.name))
        {
            throw EquationException::EquationAlreadyExists(new_item.name);
        }
    }

    std::vector<std::string> to_remove_equation_names;
    std::vector<ParseResultItem> to_add_items;
    std::vector<ParseResultItem> to_update_items;

    for (const auto &old_eqn_entry : old_name_equation_map)
    {
        std::string old_eqn_name = old_eqn_entry.first;
        auto new_item_it = new_name_item_map.find(old_eqn_name);
        if (new_item_it == new_name_item_map.end())
        {
            to_remove_equation_names.push_back(old_eqn_name);
        }
        else if (group->GetEquation(old_eqn_name)->content() != new_item_it->second.content)
        {
            to_update_items.push_back(new_item_it->second);
        }
    }

    for (const auto &new_item : new_result.items)
    {
        if (group->IsEquationExist(new_item.name) == false)
        {
            to_add_items.push_back(new_item);
        }
    }

    std::vector<std::string> dependency_updated_equation;
    ScopedConnection dependency_connection = ConnectGraphDependencyUpdated(dependency_updated_equation);

    std::vector<std::string> dependent_updated_equation;
    ScopedConnection dependent_connection = ConnectGraphDependentUpdated(dependent_updated_equation);

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    for (const auto &equation_name : to_remove_equation_names)
    {
        RemoveNodeInGraph(equation_name);
    }

    for (const auto &item : to_add_items)
    {
        AddNodeToGraph(item.name, item.dependencies);
    }

    for (const auto &item : to_update_items)
    {
        RemoveNodeInGraph(item.name);
        AddNodeToGraph(item.name, item.dependencies);
    }

    guard.commit();

    for (const auto &remove_eqn_name : to_remove_equation_names)
    {
        auto range = graph_->GetEdgesByTo(remove_eqn_name);
        for (auto it = range.first; it != range.second; it++)
        {
            graph_->InvalidateNode(it->from());
        }
        signals_manager_->Emit<EquationEvent::kEquationRemoving>(group->GetEquation(remove_eqn_name));
        RemoveEquationInGroup(group, remove_eqn_name);
        context_->Remove(remove_eqn_name);
        signals_manager_->Emit<EquationEvent::kEquationRemoved>(remove_eqn_name);
    }

    for (const auto &update_item : to_update_items)
    {
        graph_->InvalidateNode(update_item.name);
        Equation *update_eqn = group->GetEquation(update_item.name);
        update_eqn->set_content(update_item.content);
        update_eqn->set_type(update_item.type);
        update_eqn->set_status(ResultStatus::kPending);
        context_->Remove(update_item.name);
        signals_manager_->Emit<EquationEvent::kEquationUpdated>(
            update_eqn, EquationUpdateFlag::kContent | EquationUpdateFlag::kType | EquationUpdateFlag::kStatus
        );
    }

    for (const auto &add_item : to_add_items)
    {
        graph_->InvalidateNode(add_item.name);
        EquationPtr equation = Equation::Create(add_item, group->id(), this);
        AddEquationToGroup(group, std::move(equation));
        signals_manager_->Emit<EquationEvent::kEquationAdded>(group->GetEquation(add_item.name));
    }

    group->set_statement(equation_statement);

    if (to_add_items.size() != 0 || to_remove_equation_names.size() != 0)
    {
        signals_manager_->Emit<EquationEvent::kEquationGroupUpdated>(
            group, EquationGroupUpdateFlag::kEquationCount | EquationGroupUpdateFlag::kStatement
        );
    }
    else
    {
        signals_manager_->Emit<EquationEvent::kEquationGroupUpdated>(group, EquationGroupUpdateFlag::kStatement);
    }

    for (const auto &equation_name : dependency_updated_equation)
    {
        NotifyEquationDependenciesUpdated(equation_name);
    }

    for (const auto &equation_name : dependent_updated_equation)
    {
        NotifyEquationDependentsUpdated(equation_name);
    }
}

void EquationManager::EditSingleEquation(const EquationGroupId &group_id, const std::string& equation_name, const std::string& equation_content)
{
    if (IsEquationGroupExist(group_id) == false)
    {
        throw EquationException::EquationGroupNotFound(group_id);
    }

    EquationGroup *group = GetEquationGroupInternal(group_id);

    // NOTE: equation_name may be a NEW name (rename). GetEquation returns nullptr in that
    // case, so it must not be dereferenced. A rename flows through EditEquationGroup below,
    // which diffs the old group against the new statement and removes/creates equations.
    Equation *equation = group->GetEquation(equation_name);

    // No-op if the name is unchanged and the content is unchanged.
    if (equation != nullptr && equation->content() == equation_content)
    {
        return;
    }

    // use regex to validate equation name
    static const std::regex name_regex("^[A-Za-z_][A-Za-z0-9_]*$");
    if (!std::regex_match(equation_name, name_regex))
    {
        throw ParseException("Invalid equation name: " + equation_name);
    }

    std::string statement = equation_name + " = " + equation_content;

    ParseResult new_result = Parse(statement, ParseMode::kStatement);
    if (new_result.items.size() != 1)
    {
        throw ParseException("Failed to parse single equation: " + statement);
    }

    return EditEquationGroup(group_id, statement);
}

void EquationManager::RemoveEquationGroup(const EquationGroupId &group_id)
{
    if (IsEquationGroupExist(group_id) == false)
    {
        throw EquationException::EquationGroupNotFound(group_id);
    }

    EquationGroup *group = GetEquationGroupInternal(group_id);
    auto group_equation_names = group->GetEquationNames();

    std::vector<std::string> dependency_updated_equation;
    ScopedConnection dependency_connection = ConnectGraphDependencyUpdated(dependency_updated_equation);

    std::vector<std::string> dependent_updated_equation;
    ScopedConnection dependent_connection = ConnectGraphDependentUpdated(dependent_updated_equation);

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    for (const auto &equation_name : group_equation_names)
    {
        RemoveNodeInGraph(equation_name);
    }
    guard.commit();

    signals_manager_->Emit<EquationEvent::kEquationGroupRemoving>(group);

    for (const std::string &equation_name : group_equation_names)
    {
        graph_->InvalidateNode(equation_name);
        signals_manager_->Emit<EquationEvent::kEquationRemoving>(group->GetEquation(equation_name));
        RemoveEquationInGroup(group, equation_name);
        context_->Remove(equation_name);
        signals_manager_->Emit<EquationEvent::kEquationRemoved>(equation_name);
    }
    equation_group_map_.erase(group_id);

    for (const auto &equation_name : dependency_updated_equation)
    {
        NotifyEquationDependenciesUpdated(equation_name);
    }

    for (const auto &equation_name : dependent_updated_equation)
    {
        NotifyEquationDependentsUpdated(equation_name);
    }
}

ParseResult EquationManager::Parse(const std::string &expression, ParseMode mode) const
{
    auto res = parse_handler_(expression, mode);
    return res;
}

InterpretResult EquationManager::Eval(const std::string &expression) const
{
    return eval_handler_(expression, context_.get());
}

InterpretResult EquationManager::Exec(const std::string &statement) const
{
    return exec_handler_(statement, context_.get());
}

void EquationManager::Reset()
{
    graph_->Reset();

    for (const auto &equation_group_entry : equation_group_map_)
    {
        for( const auto &equation_entry : equation_group_entry.second->equation_map())
        {
            signals_manager_->Emit<EquationEvent::kEquationRemoving>(equation_entry.second.get());
        }
    }
    equation_group_map_.clear();
    equation_name_to_group_id_map_.clear();
    expression_map_.clear();
    expression_id_to_name_map_.clear();
    external_input_names_.clear();
    context_->Clear();
    signals_manager_->DisconnectAllEvent();
}

void EquationManager::ResetContext()
{
    context_->Clear();
    // The context has been cleared, so every value is gone. Re-dirty all nodes so that a
    // subsequent Update()/UpdateEquationGroup()/UpdateEquation() recalculates everything.
    // (Previously this relied on "dirty is never cleared"; now that updates clear dirty on
    // success, we must re-dirty explicitly.)
    graph_->Traversal([&](const std::string &equation_name) {
        graph_->SetNodeDirty(equation_name, true);
    });
}

void EquationManager::UpdateEquationInternal(const std::string &equation_name)
{
    if (IsEquationExist(equation_name) == false)
    {
        throw EquationException::EquationNotFound(equation_name);
    }

    Equation *equation = GetEquationInternal(equation_name);

    // set status and message to calculating before calculation
    equation->set_status(ResultStatus::kCalculating);
    equation->set_message("Calculating...");

    signals_manager_->Emit<EquationEvent::kEquationUpdated>(
        equation, EquationUpdateFlag::kStatus | EquationUpdateFlag::kMessage
    );

    const std::string &equation_statement = equation->type() == ItemType::kVariable
                                                ? equation->name() + " = " + equation->content()
                                                : equation->content();
    InterpretResult result = exec_handler_(equation_statement, context_.get());
    equation->set_status(result.status);
    equation->set_message(result.message);
    if (equation->status() != ResultStatus::kSuccess)
    {
        context_->Remove(equation_name);
    }
    else
    {
        // Clear the dirty flag on success; on failure (e.g. NameError) keep it dirty so the
        // next Update/UpdateEquation/UpdateEquationGroup retries until it succeeds.
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

void EquationManager::AddEquationToGroup(EquationGroup *group, EquationPtr equation)
{
    equation_name_to_group_id_map_.insert({equation->name(), equation->group_id()});
    group->AddEquation(std::move(equation));
}

void EquationManager::RemoveEquationInGroup(EquationGroup *group, const std::string &equation_name)
{
    equation_name_to_group_id_map_.erase(equation_name);
    group->RemoveEquation(equation_name);
}

void EquationManager::Update()
{
    graph_->Traversal([&](const std::string &node_name) { UpdateNode(node_name); });
}

void EquationManager::UpdateEquation(const std::string &equation_name)
{
    if (IsEquationExist(equation_name) == false)
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

void EquationManager::UpdateEquationGroup(const EquationGroupId &group_id)
{
    if (IsEquationGroupExist(group_id) == false)
    {
        throw EquationException::EquationGroupNotFound(group_id);
    }

    const EquationGroup *group = GetEquationGroup(group_id);

    // Update scope = the group's equations + all of their dependents (propagated by
    // TopologicalSort) plus every dirty (invalidated) node. Renaming/removing a variable
    // dirties equations that lost their dependency (e.g. in "a=1;b=a", renaming "a" to
    // "c" dirties "b"). They live outside the group and are unreachable from the group's
    // downstream, but skipping them would leave stale values/status (e.g. a NameError that
    // is never surfaced).
    std::vector<std::string> update_names = graph_->TopologicalSort(group->GetEquationNames());
    CollectDirtyNodes(update_names);

    auto topo_order = graph_->TopologicalSort(update_names);

    for (const auto &node_name : topo_order)
    {
        UpdateNode(node_name);
    }
}

// =========================================================================
// Registered expressions (只算不存)
// =========================================================================

ExpressionId EquationManager::AddExpression(const std::string &expression)
{
    static boost::uuids::random_generator rgen;

    ParseResult parse_result = Parse(expression, ParseMode::kExpression);
    if (parse_result.items.empty())
    {
        throw ParseException("Failed to parse expression: '" + expression + "'");
    }

    Expression expr;
    expr.id = rgen();
    const std::string name = "expr_" + boost::uuids::to_string(expr.id);
    expr.content = expression;
    expr.result.status = parse_result.items[0].status;
    expr.result.message = parse_result.items[0].message;
    expr.dependencies = parse_result.items[0].dependencies;

    // The expression node + its dependency edges (dependencies may not have graph
    // nodes yet -- e.g. an equation defined by a later AddEquation; edges stay
    // inactive until both endpoints exist).
    AddNodeToGraph(name, expr.dependencies);
    // Dirty the expression so it is computed on the next Update/UpdateExpression.
    graph_->InvalidateNode(name);

    const ExpressionId id = expr.id;
    expression_map_.insert({name, std::move(expr)});
    expression_id_to_name_map_.insert({id, name});
    return id;
}

void EquationManager::RemoveExpression(const ExpressionId &id)
{
    const auto it = expression_id_to_name_map_.find(id);
    if (it == expression_id_to_name_map_.end())
    {
        return;
    }
    const std::string &name = it->second;
    RemoveNodeInGraph(name);
    expression_map_.erase(name);
    expression_id_to_name_map_.erase(it);
}

const Expression *EquationManager::GetExpression(const ExpressionId &id) const
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

bool EquationManager::IsExpressionExist(const ExpressionId &id) const
{
    return expression_id_to_name_map_.count(id) != 0;
}

std::vector<ExpressionId> EquationManager::GetExpressionIds() const
{
    std::vector<ExpressionId> ids;
    ids.reserve(expression_map_.size());
    for (const auto &entry : expression_map_)
    {
        ids.push_back(entry.second.id);
    }
    return ids;
}

EquationValue EquationManager::GetExpressionValue(const ExpressionId &id) const
{
    const Expression *expr = GetExpression(id);
    if (expr == nullptr)
    {
        return EquationValue::Null();
    }
    return expr->result.value;
}

void EquationManager::UpdateExpression(const ExpressionId &id)
{
    if (!IsExpressionExist(id))
    {
        throw EquationException::ExpressionNotFound(boost::uuids::to_string(id));
    }
    UpdateExpressionInternal(id);
}

void EquationManager::UpdateExpressionInternal(const ExpressionId &id)
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

    InterpretResult eval_result = eval_handler_(expr->content, context_.get());
    expr->result = eval_result;
    // Keep the node dirty on failure so a later Update() retries it (mirrors
    // UpdateEquationInternal); clear it on success.
    graph_->SetNodeDirty(name, eval_result.status != ResultStatus::kSuccess);
    signals_manager_->Emit<EquationEvent::kExpressionUpdated>(
        expr, ExpressionUpdateFlag::kStatus | ExpressionUpdateFlag::kMessage | ExpressionUpdateFlag::kValue
    );
}

// =========================================================================
// External input symbols (外部输入)
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

void EquationManager::UpdateExternalInput(const std::string &symbol_name,
                                          const EquationValue &value)
{
    // Only a registered external input (or an existing graph node) has an anchor
    // to invalidate.  Unregistered names that resolve outside the graph (REL
    // Dataset) are simply ignored here -- the host drives their updates manually.
    if (graph_->IsNodeExist(symbol_name))
    {
        graph_->InvalidateNode(symbol_name);
    }
    else
    {
        // No graph node: nothing to invalidate/propagate.  A later
        // UpdateExternalInput with a value still injects + recomputes dependents
        // only if the name is a registered external input; otherwise it is a no-op.
        return;
    }

    const bool has_value = !value.IsNull();
    if (has_value)
    {
        // Transient injection: the value is visible to dependents while they are
        // recomputed, then removed again so it does not persist in the context.
        context_->Set(symbol_name, value);

        std::vector<std::string> update_names = graph_->TopologicalSort(symbol_name);
        auto topo_order = graph_->TopologicalSort(update_names);
        for (const auto &node_name : topo_order)
        {
            UpdateNode(node_name);
        }
        context_->Remove(symbol_name);
    }
    // Without a value: only invalidated (dirty propagated).  The recompute happens
    // on a later Update()/UpdateEquationGroup() via CollectDirtyNodes.
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
    if (equation_name_to_group_id_map_.count(node_name) != 0)
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

std::vector<std::string> EquationManager::GetEquationsToUpdate(const EquationGroupId &group_id) const
{
    if (IsEquationGroupExist(group_id) == false)
    {
        return {};
    }

    const EquationGroup *group = GetEquationGroup(group_id);
    std::vector<std::string> update_names = graph_->TopologicalSort(group->GetEquationNames());
    CollectDirtyNodes(update_names);
    return graph_->TopologicalSort(update_names);
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
    if (IsEquationExist(equation_name) == false)
    {
        throw EquationException::EquationNotFound(equation_name);
    }

    Equation *equation = GetEquationInternal(equation_name);
    equation->set_status(status);
    equation->set_message(message);
    context_->Remove(equation_name);
    // On failure (e.g. KeyBoardInterrupt) the value was removed from the context; re-dirty
    // the node so a later Update recomputes it (otherwise, under the "clear on success"
    // semantics, the node may already be clean and the value would be lost forever).
    graph_->SetNodeDirty(equation_name, true);

    signals_manager_->Emit<EquationEvent::kEquationUpdated>(
        equation, EquationUpdateFlag::kStatus | EquationUpdateFlag::kMessage | EquationUpdateFlag::kValue
    );
}

Equation *EquationManager::GetEquationInternal(const std::string &equation_name)
{
    bool is_name_exist = equation_name_to_group_id_map_.count(equation_name) != 0;
    if (!is_name_exist)
    {
        return nullptr;
    }

    const EquationGroupId &id = equation_name_to_group_id_map_.at(equation_name);
    bool is_group_exist = equation_group_map_.contains(id);
    if (!is_group_exist)
    {
        return nullptr;
    }

    EquationGroup *group = equation_group_map_.at(id).get();
    bool is_equation_exist = group->IsEquationExist(equation_name);
    if (!is_equation_exist)
    {
        return nullptr;
    }

    return group->GetEquation(equation_name);
}

EquationGroup *EquationManager::GetEquationGroupInternal(const EquationGroupId &group_id)
{
    if (equation_group_map_.contains(group_id))
    {
        return equation_group_map_.at(group_id).get();
    }
    return nullptr;
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

    // Get and escape type
    std::string type_str = ItemTypeConverter::ToString(equation->type());
    std::string escaped_type = escape_for_record(type_str);

    // Truncate content if too long
    std::string content = equation->content();
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

    // Build record format: {name|{type|content}}
    std::ostringstream oss;
    oss << "{" << escaped_name << "|{" << escaped_type << "|" << escaped_content << "}}";
    return oss.str();
}

bool EquationManager::WriteDependencyGraphToDotFile(const std::string &file_path) const
{
    return graph_->WriteDotFile(file_path, [this](const std::string &node_name) {
        return GenerateEquationDotNodeLabel(node_name);
    });
}

} // namespace xequation