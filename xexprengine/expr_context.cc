#include "expr_context.h"
#include "dependency_graph.h"
#include "event_stamp.h"
#include "expr_common.h"
#include "value.h"
#include "variable.h"
#include <cstdio>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

using namespace xexprengine;

ExprContext::ExprContext()
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
        DependencyGraph::BatchUpdateGuard guard(graph_.get());
        graph_->AddNode(var_name);
        variable_map_.insert({var_name, std::move(var)});
        UpdateVariableDependencies(var_name);
        try
        {
            guard.commit();
        }
        catch (xexprengine::DependencyCycleException e)
        {
            // todo
            return false;
        }
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
        res &= AddVariable(std::move(var));
    }
    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        // todo
        return false;
    }
    return res;
}

void ExprContext::SetValue(const std::string &var_name, const Value &value)
{
    auto var = VariableFactory::CreateVariable(var_name, value);
    SetVariable(var_name, std::move(var));
}

void ExprContext::SetExpression(const std::string &var_name, const std::string &expression)
{
    auto var = VariableFactory::CreateVariable(var_name, expression);
    SetVariable(var_name, std::move(var));
}

void ExprContext::SetVariable(const std::string &var_name, std::unique_ptr<Variable> variable)
{
    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    if (IsVariableExist(var_name) == false)
    {
        AddVariable(std::move(variable));
    }
    else
    {
        variable_map_[var_name] = std::move(variable);
        UpdateVariableDependencies(var_name);
        graph_->InvalidateNode(var_name);
    }
    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        // todo
        return;
    }
}

bool ExprContext::RemoveVariable(const std::string &var_name)
{
    if (IsVariableExist(var_name) == true)
    {
        graph_->RemoveNode(var_name);
        variable_map_.erase(var_name);
        RemoveContextValue(var_name);
        return true;
    }
    return false;
}

bool ExprContext::RemoveVariables(const std::vector<std::string> &var_name_list)
{
    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    bool res = true;
    for (const std::string &var_name : var_name_list)
    {
        res &= RemoveVariable(var_name);
    }
    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        // todo
        return false;
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
            // todo
            return false;
        }
        // process variable map
        std::unique_ptr<Variable> origin_var = std::move(variable_map_[old_name]);
        variable_map_.erase(old_name);
        origin_var->set_name(new_name);
        variable_map_.insert({new_name, std::move(origin_var)});

        // proces context
        Value old_value = GetContextValue(old_name);
        RemoveContextValue(old_name);
        SetContextValue(new_name, old_value);
        return true;
    }
    return false;
}

bool ExprContext::UpdateVariableDependencies(const std::string &var_name)
{
    if (parse_callback_ == nullptr)
    {
        return false;
    }

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    if (IsVariableExist(var_name) == false)
    {
        return false;
    }

    // remove origin dependencies
    const DependencyGraph::Node *node = graph_->GetNode(var_name);
    const DependencyGraph::EdgeContainer::RangeByFrom edges = graph_->GetEdgesByFrom(var_name);
    std::vector<DependencyGraph::Edge> edges_to_remove;
    for (auto it = edges.first; it != edges.second; it++)
    {
        edges_to_remove.push_back(*it);
    }
    graph_->RemoveEdges(edges_to_remove);

    // add new dependencies
    Variable *var = GetVariable(var_name);
    if (var->GetType() == Variable::Type::Expr)
    {
        ExprVariable *expr_var = var->As<ExprVariable>();

        ParseResult result = parse_callback_(expr_var->expression());
        if (result.status == VariableStatus::kExprParseSuccess)
        {
            for (const std::string &dep_var : result.variables)
            {
                graph_->AddEdge({var_name, dep_var});
            }
        }
        var->set_status(result.status);
        var->set_error_message(result.parse_error_message);
    }
    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        // todo
        return false;
    }
    return true;
}

bool ExprContext::IsVariableDependencyEntire(
    const std::string &var_name, std::vector<std::string> &missing_dependencies
) const
{
    if (IsVariableExist(var_name) == false)
    {
        return false;
    }
    const DependencyGraph::Node *node = graph_->GetNode(var_name);
    DependencyGraph::EdgeContainer::RangeByFrom edges = graph_->GetEdgesByFrom(var_name);
    const auto &dependencies = node->dependencies();
    if(dependencies.size() == std::distance(edges.first, edges.second))
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

bool ExprContext::IsVariableExist(const std::string &var_name) const
{
    return graph_->IsNodeExist(var_name) && variable_map_.count(var_name);
}

void ExprContext::Reset()
{
    graph_->Reset();
    variable_map_.clear();
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
    if (IsVariableDependencyEntire(var_name, missing_dependencies) == false)
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

    bool is_should_update = node->dirty_flag();

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
    if(var->status() == VariableStatus::kExprParseSyntaxError)
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
        EvalResult result = evaluate_callback_(expression, this);
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

void ExprContext::Update()
{
    graph_->Traversal([&](const std::string &var_name) { UpdateVariable(var_name); });
}