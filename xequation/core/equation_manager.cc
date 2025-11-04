#include "equation_manager.h"
#include "dependency_graph.h"
#include "equation.h"
#include "event_stamp.h"
#include "expr_common.h"
#include <iterator>
#include <string>

using namespace xequation;

EquationManager::EquationManager(
    std::unique_ptr<ExprContext> context, ExecCallback eval_callback, ParseCallback parse_callback
) noexcept
    : graph_(std::unique_ptr<DependencyGraph>(new DependencyGraph())),
      context_(std::move(context)),
      exec_callback_(eval_callback),
      parse_callback_(parse_callback)
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

void EquationManager::AddEquation(const std::string &equation_code)
{
    ParseResult res = parse_callback_(equation_code);

    if (IsEquationExist(res.name))
    {
        throw DuplicateEquationNameError(res.name, GetEquation(res.name)->content());
    }

    AddNodeToGraph(res.name, res.dependencies);
    graph_->InvalidateNode(res.name);
    Equation eqn(res.name);
    eqn.set_type(res.type);
    eqn.set_content(res.content);
    eqn.set_dependencies(res.dependencies);
    eqn.set_status(ExecStatus::kInit);
    equation_map_.insert({res.name, eqn});
}

void EquationManager::EditEquation(const std::string &eqn_name, const std::string &equation_code)
{
    ParseResult res = parse_callback_(equation_code);

    if (res.name != eqn_name && IsEquationExist(res.name))
    {
        throw DuplicateEquationNameError(res.name, GetEquation(res.name)->content());
    }

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    RemoveNodeInGraph(eqn_name);
    AddNodeToGraph(res.name, res.dependencies);
    guard.commit();

    if (res.name != eqn_name)
    {
        auto range = graph_->GetEdgesByTo(eqn_name);
        for (auto it = range.first; it != range.second; it++)
        {
            graph_->InvalidateNode(it->from());
        }
        equation_map_.erase(eqn_name);
        context_->Remove(eqn_name);
    }

    graph_->InvalidateNode(res.name);
    Equation eqn(res.name);
    eqn.set_type(res.type);
    eqn.set_content(res.content);
    eqn.set_dependencies(res.dependencies);
    eqn.set_status(ExecStatus::kInit);
    equation_map_[res.name] = eqn;
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

std::string BuildMissingDepsMessage(const std::vector<std::string> &missing_deps)
{
    if (missing_deps.empty())
    {
        return "Missing dependencies: unknown";
    }

    std::string message = "Missing dependencies: ";
    for (size_t i = 0; i < missing_deps.size(); ++i)
    {
        if (i > 0)
        {
            message += ", ";
        }
        message += missing_deps[i];
    }
    return message;
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
    eval_success = (result.status == ExecStatus::kSuccess);
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