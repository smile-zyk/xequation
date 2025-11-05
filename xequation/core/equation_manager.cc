#include "equation_manager.h"
#include "dependency_graph.h"
#include "equation.h"
#include "event_stamp.h"
#include "expr_common.h"
#include <iterator>
#include <string>
#include <unordered_set>
#include <vector>

using namespace xequation;

EquationManager::EquationManager(
    std::unique_ptr<ExprContext> context, ExecCallback eval_callback, ParseCallback parse_callback, EvalCallback exec_callback
) noexcept
    : graph_(std::unique_ptr<DependencyGraph>(new DependencyGraph())),
      context_(std::move(context)),
      exec_callback_(eval_callback),
      parse_callback_(parse_callback),
      eval_callback_(exec_callback)
{
}

const Equation *EquationManager::GetEquation(const std::string &eqn_name) const
{
    if (IsEquationExist(eqn_name) == true)
    {
        return &equation_map_.at(eqn_name);
    }
    return nullptr;
}

void EquationManager::SetEquation(const std::string &eqn_name, const std::string &expression)
{
    if (IsEquationExist(eqn_name))
    {
        EditEquationStatement(GetEquation(eqn_name)->content(), eqn_name + "=" + expression);
    }
    else
    {
        AddEquationStatement(eqn_name + "=" + expression);
    }
}

void EquationManager::RemoveEquation(const std::string &eqn_name) noexcept
{
    if (IsEquationExist(eqn_name) == false)
    {
        return;
    }
    graph_->InvalidateNode(eqn_name);
    RemoveNodeInGraph(eqn_name);
    equation_map_.erase(eqn_name);
    context_->Remove(eqn_name);
}

void EquationManager::AddEquationStatement(const std::string &equation_code)
{
    ParseResult res = parse_callback_(equation_code);

    for (const auto &eqn : res)
    {
        if (IsEquationExist(eqn.name()))
        {
            throw DuplicateEquationNameError(eqn.name(), GetEquation(eqn.name())->content());
        }
    }

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    for (const auto &eqn : res)
    {
        AddNodeToGraph(eqn.name(), eqn.dependencies());
    }
    guard.commit();
    for (const auto &eqn : res)
    {
        graph_->InvalidateNode(eqn.name());
        equation_map_.insert({eqn.name(), eqn});
    }
}

void EquationManager::EditEquationStatement(const std::string &old_equation_code, const std::string &equation_code)
{
    ParseResult old_res = parse_callback_(old_equation_code);
    ParseResult new_res = parse_callback_(equation_code);

    std::unordered_set<std::string> old_name_map;
    for (const auto &eqn : old_res)
    {
        old_name_map.insert(eqn.name());
    }

    std::unordered_set<std::string> new_name_map;
    for (const auto &eqn : new_res)
    {
        new_name_map.insert(eqn.name());
    }

    for (const auto &new_eqn_name : new_name_map)
    {
        if (old_name_map.find(new_eqn_name) == old_name_map.end() && IsEquationExist(new_eqn_name))
        {
            throw DuplicateEquationNameError(new_eqn_name, GetEquation(new_eqn_name)->content());
        }
    }

    std::unordered_set<Equation> old_set(old_res.begin(), old_res.end());
    std::unordered_set<Equation> new_set(new_res.begin(), new_res.end());

    std::vector<Equation> to_remove;
    std::vector<Equation> to_add;

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    for (const auto &eqn : old_set)
    {
        if (new_set.find(eqn) == new_set.end())
        {
            RemoveNodeInGraph(eqn.name());
            to_remove.push_back(eqn);
        }
    }

    for (const auto &eqn : new_set)
    {
        if (old_set.find(eqn) == old_set.end())
        {
            AddNodeToGraph(eqn.name(), eqn.dependencies());
            to_add.push_back(eqn);
        }
    }
    guard.commit();

    for (const auto &eqn : to_remove)
    {
        equation_map_.erase(eqn.name());
        context_->Remove(eqn.name());
        if (new_name_map.find(eqn.name()) == new_name_map.end())
        {
            auto range = graph_->GetEdgesByTo(eqn.name());
            for (auto it = range.first; it != range.second; it++)
            {
                graph_->InvalidateNode(it->from());
            }
        }
    }
    
    for (const auto &eqn : to_add)
    {
        graph_->InvalidateNode(eqn.name());
        equation_map_.insert({eqn.name(), eqn});
    }
}

void EquationManager::RemoveEquationStatement(const std::string &equation_code) noexcept
{
    ParseResult res = parse_callback_(equation_code);

    for (const auto &eqn : res)
    {
        RemoveEquation(eqn.name());
    }
}

std::vector<Equation> EquationManager::GetEquations(const std::string &equation_code) const
{
    auto parse_result = parse_callback_(equation_code);
    return parse_result;
}

bool EquationManager::IsEquationExist(const std::string &var_name) const
{
    return graph_->IsNodeExist(var_name) && equation_map_.count(var_name);
}

void EquationManager::Reset()
{
    graph_->Reset();
    equation_map_.clear();
    context_->Clear();
}

void EquationManager::UpdateEquationInternal(const std::string &eqn_name)
{
    if (!IsEquationExist(eqn_name))
    {
        return;
    }

    const DependencyGraph::Node *node = graph_->GetNode(eqn_name);
    Equation &eqn = equation_map_.at(eqn_name);

    if (!node->dirty_flag())
    {
        return;
    }

    bool needs_evaluation = true;

    auto range = graph_->GetEdgesByFrom(eqn_name);
    if (node->dependencies().size() == std::distance(range.first, range.second))
    {
        EventStamp max_dep_stamp;
        for (const std::string &dep : node->dependencies())
        {
            if (const DependencyGraph::Node *dep_node = graph_->GetNode(dep))
            {
                max_dep_stamp = std::max(max_dep_stamp, dep_node->event_stamp());
            }
        }
        needs_evaluation = (node->event_stamp() <= max_dep_stamp);
    }

    if (!needs_evaluation)
    {
        return;
    }

    bool eval_success = true;

    const std::string &eqn_code = eqn.content();
    Value origin_value = context_->Get(eqn_name);
    ExecResult result = exec_callback_(eqn_code, context_.get());
    eval_success = (result.status == Equation::Status::kSuccess);
    if (eval_success)
    {
        Value new_value = context_->Get(eqn_name);
        if (origin_value != new_value)
        {
            graph_->UpdateNodeEventStamp(eqn_name);
        }
    }
    else
    {
        if (context_->Remove(eqn_name))
        {
            graph_->UpdateNodeEventStamp(eqn_name);
        }
    }
    eqn.set_status(result.status);
    eqn.set_message(result.message);
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
    graph_->Traversal([&](const std::string &var_name) { UpdateEquationInternal(var_name); });
}

void EquationManager::UpdateEquation(const std::string &var_name)
{
    auto topo_order = graph_->TopologicalSort(var_name);

    for (const auto &node_name : topo_order)
    {
        UpdateEquationInternal(node_name);
    }
}