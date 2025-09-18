#include "expr_context.h"
#include "dependency_graph.h"
#include "event_stamp.h"
#include "expr_common.h"
#include "value.h"
#include "variable.h"
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

using namespace xexprengine;

ExprContext::ExprContext() noexcept
    : graph_(std::unique_ptr<DependencyGraph>(new DependencyGraph())),
      evaluate_callback_(nullptr),
      parse_callback_(nullptr)
{
}

Variable *ExprContext::GetVariable(const std::string &var_name) const
{
    if (IsVariableExist(var_name) == true)
    {
        return variable_map_.at(var_name).get();
    }
    return nullptr;
}

bool ExprContext::AddVariable(std::unique_ptr<Variable> var)
{
    std::string var_name = var->name();
    if (IsVariableExist(var_name) == false)
    {
        // update graph
        try
        {
            AddVariableToGraph(var.get());
        }
        catch (xexprengine::DependencyCycleException e)
        {
            throw;
        }

        // update variable_map
        AddVariableToMap(std::move(var));
        return true;
    }
    return false;
}

bool ExprContext::AddVariables(std::vector<std::unique_ptr<Variable>> var_list)
{
    bool res = true;
    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    for (std::unique_ptr<Variable> &var : var_list)
    {
        res &= AddVariableToGraph(var.get());
    }
    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        throw;
    }

    for (std::unique_ptr<Variable> &var : var_list)
    {
        res &= AddVariableToMap(std::move(var));
    }
    return res;
}

void ExprContext::SetValue(const std::string &var_name, const Value &value)
{
    auto var = VariableFactory::CreateRawVariable(var_name, value);
    SetVariable(var_name, std::move(var));
}

void ExprContext::SetExpression(const std::string &var_name, const std::string &expression)
{
    auto var = VariableFactory::CreateExprVariable(var_name, expression);
    SetVariable(var_name, std::move(var));
}

bool ExprContext::SetVariable(const std::string &var_name, std::unique_ptr<Variable> variable)
{
    if (variable->name() != var_name)
    {
        return false;
    }

    // update graph
    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    bool is_variable_exist = IsVariableExist(var_name);
    if (is_variable_exist == true)
    {
        RemoveVariableToGraph(var_name);
    }
    AddVariableToGraph(variable.get());
    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        throw;
    }

    if (is_variable_exist)
    {
        // remove old variable
        variable_map_.erase(var_name);
    }

    // add variable to map
    AddVariableToMap(std::move(variable));

    return true;
}

bool ExprContext::RemoveVariable(const std::string &var_name) noexcept
{
    if (IsVariableExist(var_name) == true)
    {
        // update graph
        RemoveVariableToGraph(var_name);

        // update variable map
        variable_map_.erase(var_name);

        return true;
    }
    return false;
}

bool ExprContext::RemoveVariables(const std::vector<std::string> &var_name_list) noexcept
{
    bool res = true;
    for (const std::string &var_name : var_name_list)
    {
        res &= RemoveVariable(var_name);
    }
    return res;
}

bool ExprContext::RenameVariable(const std::string &old_name, const std::string &new_name)
{
    if (IsVariableExist(old_name) == true && IsVariableExist(new_name) == false)
    {
        // process graph
        DependencyGraph::BatchUpdateGuard guard(graph_.get());
        graph_->RemoveNode(old_name);
        graph_->AddNode(new_name);
        auto old_edge_iterator = graph_->GetEdgesByFrom(old_name);
        std::vector<DependencyGraph::Edge> old_edges;
        std::vector<DependencyGraph::Edge> new_edges;
        for (auto it = old_edge_iterator.first; it != old_edge_iterator.second; it++)
        {
            old_edges.push_back(*it);
            new_edges.push_back({new_name, it->to()});
        }
        graph_->RemoveEdges(old_edges);
        graph_->AddEdges(new_edges);
        try
        {
            guard.commit();
        }
        catch (xexprengine::DependencyCycleException e)
        {
            throw;
        }

        // process variable map
        std::unique_ptr<Variable> origin_var = std::move(variable_map_[old_name]);
        variable_map_.erase(old_name);
        origin_var->set_name(new_name);
        variable_map_.insert({new_name, std::move(origin_var)});

        return true;
    }
    return false;
}

bool ExprContext::CheckNodeDependenciesComplete(
    const std::string &node_name, std::vector<std::string> &missing_dependencies
) const
{
    if (graph_->IsNodeExist(node_name) == false)
    {
        return false;
    }
    const DependencyGraph::Node *node = graph_->GetNode(node_name);
    DependencyGraph::EdgeContainer::RangeByFrom edges = graph_->GetEdgesByFrom(node_name);
    const auto &dependencies = node->dependencies();
    if (dependencies.size() == std::distance(edges.first, edges.second))
    {
        return true;
    }
    for (auto it = edges.first; it != edges.second; it++)
    {
        if (dependencies.count(it->to()) == 0)
        {
            missing_dependencies.push_back(it->to());
        }
    }
    return false;
}

bool ExprContext::UpdateNodeDependencies(
    const std::string &node_name, const std::unordered_set<std::string> &node_dependencies
)
{
    if (graph_->IsNodeExist(node_name) == false)
    {
        return false;
    }
    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    const DependencyGraph::Node *node = graph_->GetNode(node_name);
    const DependencyGraph::EdgeContainer::RangeByFrom edges = graph_->GetEdgesByFrom(node_name);
    std::vector<DependencyGraph::Edge> edges_to_remove;
    for (auto it = edges.first; it != edges.second; it++)
    {
        edges_to_remove.push_back(*it);
    }
    graph_->RemoveEdges(edges_to_remove);

    for (const std::string &dep : node_dependencies)
    {
        graph_->AddEdge({node_name, dep});
    }

    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        throw;
    }

    return true;
}

bool ExprContext::IsVariableExist(const std::string &var_name) const
{
    return graph_->IsNodeExist(var_name) && variable_map_.count(var_name);
}

void ExprContext::Reset()
{
    graph_->Reset();
    variable_map_.clear();
    parse_cached_map_.clear();
    ClearContextValue();
}

bool ExprContext::UpdateVariable(const std::string &var_name)
{
    if (!IsVariableExist(var_name))
    {
        return false;
    }

    if (evaluate_callback_ == nullptr)
    {
        return false;
    }

    DependencyGraph::Node *node = graph_->GetNode(var_name);
    Variable *var = GetVariable(var_name);

    // check missing dependencies;
    std::vector<std::string> missing_dependencies;
    if (CheckNodeDependenciesComplete(var_name, missing_dependencies) == false)
    {
        if (IsContextValueExist(var_name) == true)
        {
            RemoveContextValue(var_name);
            graph_->UpdateNodeEventStamp(var_name);
        }
        var->set_status(VariableStatus::kMissingDependency);
        std::string error_message = "missing dependencies is: ";
        for (int i = 0; i < missing_dependencies.size(); i++)
        {
            if (i != 0)
                error_message += ", ";
            error_message += missing_dependencies[i];
        }
        var->set_error_message(error_message);
        node->set_dirty_flag(false);
        return false;
    }

    bool is_should_update = var->status() == VariableStatus::kParseSuccess || node->dirty_flag();

    if (is_should_update == false)
    {
        EventStamp max_event_stamp;
        for (const std::string &dependency : node->dependencies())
        {
            const DependencyGraph::Node *dep_node = graph_->GetNode(dependency);
            if (dep_node && dep_node->event_stamp() > max_event_stamp)
            {
                max_event_stamp = dep_node->event_stamp();
            }
        }
        if (node->event_stamp() <= max_event_stamp)
        {
            is_should_update = true;
        }
    }

    if (is_should_update == false)
    {
        node->set_dirty_flag(false);
        return false;
    }

    // check parse error
    if (var->status() == VariableStatus::kParseSyntaxError)
    {
        if (IsContextValueExist(var_name) == true)
        {
            RemoveContextValue(var_name);
            graph_->UpdateNodeEventStamp(var_name);
        }
        node->set_dirty_flag(false);
        return false;
    }

    if (var->GetType() == Variable::Type::Expr)
    {
        const std::string &expression = var->As<ExprVariable>()->expression();
        EvalResult result = evaluate_callback_(expression);
        if (result.status != VariableStatus::kExprEvalSuccess)
        {
            if (RemoveContextValue(var_name) == true)
            {
                graph_->UpdateNodeEventStamp(var_name);
            }
        }
        else if (result.value != GetContextValue(var_name))
        {
            SetContextValue(var_name, result.value);
            graph_->UpdateNodeEventStamp(var_name);
        }
        var->set_status(result.status);
        var->set_error_message(result.eval_error_message);
    }
    else if (var->GetType() == Variable::Type::Raw)
    {
        const Value &value = var->As<RawVariable>()->value();
        if (value != GetContextValue(var_name))
        {
            SetContextValue(var_name, value);
            graph_->UpdateNodeEventStamp(var_name);
        }
        var->set_status(VariableStatus::kRawVar);
        var->set_error_message("");
    }
    node->set_dirty_flag(false);
    return true;
}

void ExprContext::UpdateVariableParseStatus(Variable *var)
{
    if (var->GetType() == Variable::Type::Raw)
    {
        var->set_status(VariableStatus::kParseSuccess);
        var->set_error_message("");
    }
    else if (var->GetType() == Variable::Type::Expr)
    {
        const ExprVariable *expr_var = var->As<ExprVariable>();
        const std::string &expression = expr_var->expression();
        auto it = parse_cached_map_.find(expression);
        ParseResult parse_result;
        if (it == parse_cached_map_.end())
        {
            if (parse_callback_ != nullptr)
            {
                parse_result = parse_callback_(expression);
                parse_cached_map_.insert({expression, parse_result});
            }
        }
        else
        {
            parse_result = it->second;
        }
        var->set_status(parse_result.status);
        var->set_error_message(parse_result.parse_error_message);
    }
}

bool ExprContext::AddVariableToGraph(const Variable *var)
{
    const std::string &var_name = var->name();
    if (graph_->IsNodeExist(var_name))
    {
        return false;
    }

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    graph_->AddNode(var_name);
    if (var->GetType() == Variable::Type::Expr)
    {
        const ExprVariable *expr_var = var->As<ExprVariable>();
        const std::string &expression = expr_var->expression();
        auto it = parse_cached_map_.find(expression);
        ParseResult parse_result;
        if (it == parse_cached_map_.end())
        {
            if (parse_callback_ != nullptr)
            {
                parse_result = parse_callback_(expression);
                parse_cached_map_.insert({expression, parse_result});
            }
        }
        else
        {
            parse_result = it->second;
        }
        UpdateNodeDependencies(var_name, parse_result.variables);
    }

    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        throw;
    }
    graph_->InvalidateNode(var_name);
    return true;
}

bool ExprContext::RemoveVariableToGraph(const std::string &var_name) noexcept
{
    if (graph_->IsNodeExist(var_name) == false)
    {
        return false;
    }
    graph_->RemoveNode(var_name);
    auto edges = graph_->GetEdgesByFrom(var_name);
    std::vector<DependencyGraph::Edge> edges_to_remove;
    for (auto it = edges.first; it != edges.second; it++)
    {
        edges_to_remove.push_back(*it);
    }
    graph_->RemoveEdges(edges_to_remove);
    return true;
}

bool ExprContext::AddVariableToMap(std::unique_ptr<Variable> var)
{
    const std::string &var_name = var->name();
    if (variable_map_.count(var_name))
    {
        return false;
    }
    // set parse status when add
    UpdateVariableParseStatus(var.get());
    variable_map_.insert({var_name, std::move(var)});
    return true;
}

void ExprContext::Update()
{
    std::unordered_set<std::string> current_exist_var_set;
    graph_->Traversal([&](const std::string &var_name) {
        UpdateVariable(var_name);
        current_exist_var_set.insert(var_name);
    });

    std::unordered_set<std::string> context_value_to_remove = GetContextExistVariables();

    for(const auto& entry : current_exist_var_set)
    {
        context_value_to_remove.erase(entry);
    }

    for(const auto& entry : context_value_to_remove)
    {
        RemoveContextValue(entry);
    }
}