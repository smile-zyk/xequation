#include "variable_dependency_graph.h"
#include "variable.h"
#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <vector>

using namespace xexprengine;

std::unordered_set<std::string> VariableDependencyGraph::GetNodeDependencies(const std::string &node_name) const
{
    if (IsNodeExist(node_name) == true)
    {
        return node_map_.at(node_name)->dependencies_;
    }
    else
    {
        return {};
    }
}

std::unordered_set<std::string> VariableDependencyGraph::GetNodeDependents(const std::string &node_name) const
{
    if (IsNodeExist(node_name) == true)
    {
        return node_map_.at(node_name)->dependents_;
    }
    else
    {
        return {};
    }
}

bool VariableDependencyGraph::IsNodeExist(const std::string &node_name) const
{
    return node_map_.find(node_name) != node_map_.end();
}

bool VariableDependencyGraph::IsNodeDirty(const std::string &node_name) const
{
    if (IsNodeExist(node_name) == true)
    {
        return node_map_.at(node_name)->is_dirty_;
    }
    return false;
}

std::vector<VariableDependencyGraph::Edge> VariableDependencyGraph::GetEdgesByFrom(const std::string &from) const
{
    std::vector<Edge> result;
    auto &idx = edges_.get<EdgeContainer::ByFrom>();
    auto range = idx.equal_range(from);
    for (auto it = range.first; it != range.second; ++it)
    {
        result.push_back(*it);
    }
    return result;
}

std::vector<VariableDependencyGraph::Edge>
VariableDependencyGraph::GetEdgesByFrom(const std::vector<std::string> &from_list) const
{
    std::vector<Edge> result;

    // remove duplicate
    std::vector<std::string> unique_froms = from_list;
    std::sort(unique_froms.begin(), unique_froms.end());
    unique_froms.erase(std::unique(unique_froms.begin(), unique_froms.end()), unique_froms.end());

    auto &idx = edges_.get<EdgeContainer::ByFrom>();
    for (const auto &from : unique_froms)
    {
        auto range = idx.equal_range(from);
        for (auto it = range.first; it != range.second; ++it)
        {
            result.push_back(*it);
        }
    }

    return result;
}

std::vector<VariableDependencyGraph::Edge> VariableDependencyGraph::GetEdgesByTo(const std::string &to) const
{
    std::vector<Edge> result;
    auto &idx = edges_.get<EdgeContainer::ByTo>();
    auto range = idx.equal_range(to);
    for (auto it = range.first; it != range.second; ++it)
    {
        result.push_back(*it);
    }
    return result;
}

std::vector<VariableDependencyGraph::Edge> VariableDependencyGraph::GetEdgesByTo(const std::vector<std::string> &to_list
) const
{
    std::vector<Edge> result;

    // remove duplicate
    std::vector<std::string> unique_tos = to_list;
    std::sort(unique_tos.begin(), unique_tos.end());
    unique_tos.erase(std::unique(unique_tos.begin(), unique_tos.end()), unique_tos.end());

    auto &idx = edges_.get<EdgeContainer::ByTo>();
    for (const auto &to : unique_tos)
    {
        auto range = idx.equal_range(to);
        for (auto it = range.first; it != range.second; ++it)
        {
            result.push_back(*it);
        }
    }

    return result;
}

std::vector<VariableDependencyGraph::Edge> VariableDependencyGraph::GetAllEdges() const
{
    return std::vector<Edge>(edges_.begin(), edges_.end());
}

Variable* VariableDependencyGraph::GetVariable(const std::string &node_name) const
{
    if (IsNodeExist(node_name) == true)
    {
        return node_map_.at(node_name)->variable_.get();
    }
    return nullptr;
}

bool VariableDependencyGraph::AddNode(std::unique_ptr<Variable> var)
{
    const std::string& node_name = var->name();
    if (IsNodeExist(node_name) == true)
    {
        return false;
    }

    std::unique_ptr<Node> node = std::unique_ptr<Node>(new Node(std::move(var)));
    node_map_.insert({node_name, std::move(node)});

    auto node_dependency_edges = GetEdgesByFrom(node_name);
    for (const auto &edge : node_dependency_edges)
    {
        ActiveEdgeToNodes(edge);
    }

    auto node_dependent_edges = GetEdgesByTo(node_name);
    for (const auto &edge : node_dependent_edges)
    {
        ActiveEdgeToNodes(edge);
    }

    if (HasCycle())
    {
        auto cycle_path = FindNodeCyclePath(node_name);
        RemoveNode(node_name);
        throw DependencyCycleException({cycle_path});
    }

    return true;
}

bool VariableDependencyGraph::AddNodes(const std::vector<std::unique_ptr<Variable>> &var_list)
{
    std::vector<std::unique_ptr<Variable>> unique_vars = std::move(var_list);
    // remove duplicate
    std::unordered_set<std::string> seen_names;
    auto new_end = std::remove_if(unique_vars.begin(), unique_vars.end(),
        [&seen_names](const std::unique_ptr<Variable>& var) {
            return !seen_names.insert(var->name()).second;
        });
    unique_vars.erase(new_end, var_list.end());

    size_t unique_size = unique_vars.size();

    unique_vars.erase(
        std::remove_if(
            unique_vars.begin(), unique_vars.end(),
            [this](const std::unique_ptr<Variable> &var) { return IsNodeExist(var->name()); }
        ),
        unique_vars.end()
    );

    if (unique_vars.size() != unique_size)
    {
        // some node is already in node_map
        return false;
    }

    for (std::unique_ptr<Variable> &var : unique_vars)
    {
        std::unique_ptr<Node> node = std::unique_ptr<Node>(new Node(std::move(var)));
        node_map_.insert({var->name(), std::move(node)});
    }

    std::vector<std::string> var_name_list;
    var_name_list.reserve(var_list.size());

    for (const auto& var : unique_vars) {
        var_name_list.push_back(var->name());
    }

    std::vector<Edge> dependency_edge_list = GetEdgesByFrom(var_name_list);
    for (const Edge &edge : dependency_edge_list)
    {
        ActiveEdgeToNodes(edge);
    }

    std::vector<Edge> dependent_edge_list = GetEdgesByTo(var_name_list);
    for (const Edge &edge : dependent_edge_list)
    {
        ActiveEdgeToNodes(edge);
    }

    if (HasCycle())
    {
        auto cycle_path_list = FindCyclePath();
        RemoveNodes(var_name_list);
        throw DependencyCycleException(cycle_path_list);
    }

    return true;
}

bool VariableDependencyGraph::RemoveNode(const std::string &node_name)
{
    if (IsNodeExist(node_name) == false)
    {
        return false;
    }

    node_map_.erase(node_name);

    auto node_dependency_edges = GetEdgesByFrom(node_name);
    for (const auto &edge : node_dependency_edges)
    {
        DeactiveEdgeToNodes(edge);
    }

    auto node_dependent_edges = GetEdgesByTo(node_name);
    for (const auto &edge : node_dependent_edges)
    {
        DeactiveEdgeToNodes(edge);
    }

    return true;
}

bool VariableDependencyGraph::RemoveNodes(const std::vector<std::string> &node_list)
{
    // remove duplicate
    std::vector<std::string> unique_nodes = node_list;
    std::sort(unique_nodes.begin(), unique_nodes.end());
    unique_nodes.erase(std::unique(unique_nodes.begin(), unique_nodes.end()), unique_nodes.end());

    size_t unique_size = unique_nodes.size();

    unique_nodes.erase(
        std::remove_if(
            unique_nodes.begin(), unique_nodes.end(),
            [this](const std::string &node_name) { return !IsNodeExist(node_name); }
        ),
        unique_nodes.end()
    );

    if (unique_nodes.size() != unique_size)
    {
        // some node is already in node_map
        return false;
    }

    for (const std::string &node_name : unique_nodes)
    {
        node_map_.erase(node_name);
    }

    auto node_dependency_edges = GetEdgesByFrom(unique_nodes);
    for (const auto &edge : node_dependency_edges)
    {
        DeactiveEdgeToNodes(edge);
    }

    auto node_dependent_edges = GetEdgesByTo(unique_nodes);
    for (const auto &edge : node_dependent_edges)
    {
        DeactiveEdgeToNodes(edge);
    }

    return true;
}

bool VariableDependencyGraph::RenameNode(const std::string &old_name, const std::string &new_name)
{
    if(IsNodeExist(old_name) == false || IsNodeExist(new_name) == true)
    {
        return false;
    }

    auto node_it = node_map_.find(old_name);
    if (node_it == node_map_.end())
    {
        return false;
    }

    auto node_ptr = std::move(node_it->second);
    node_map_.erase(node_it);
    node_ptr->variable_->name_ = new_name;
    node_map_.insert({new_name, std::move(node_ptr)});

    // Update edges
    std::vector<Edge> edges_to_update;

    auto from_edges = GetEdgesByFrom(old_name);
    edges_to_update.insert(edges_to_update.end(), from_edges.begin(), from_edges.end());

    auto to_edges = GetEdgesByTo(old_name);
    edges_to_update.insert(edges_to_update.end(), to_edges.begin(), to_edges.end());

    for (const auto &edge : edges_to_update)
    {
        RemoveEdge(edge);
        Edge new_edge = (edge.from_ == old_name) ? Edge(new_name, edge.to_) : Edge(edge.from_, new_name);
        AddEdge(new_edge);
    }

    return true;
}

bool VariableDependencyGraph::AddEdge(const std::string &from, const std::string &to)
{
    return AddEdge({from, to});
}

bool VariableDependencyGraph::SetNodeDirty(const std::string &node_name, bool dirty)
{
    if (IsNodeExist(node_name) == false)
    {
        return false;
    }

    node_map_[node_name]->is_dirty_ = dirty;
    return true;
}

bool VariableDependencyGraph::ClearNodeDependencyEdges(const std::string &node_name)
{
    if (IsNodeExist(node_name) == false)
    {
        return false;
    }

    bool res = true;

    auto node_dependency_edges = GetEdgesByFrom(node_name);
    for (const auto &edge : node_dependency_edges)
    {
        res &= RemoveEdge(edge);
    }

    return true;
}

bool VariableDependencyGraph::RemoveEdge(const Edge &edge)
{
    if (edges_.contains(edge) == false)
    {
        return false;
    }

    edges_.erase(edge);
    DeactiveEdgeToNodes(edge);
    return true;
}

std::vector<std::vector<std::string>> VariableDependencyGraph::FindCyclePath() const
{
    std::vector<std::vector<std::string>> result;
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursion_stack;
    std::vector<std::string> cycle_path;

    for (const auto &entry : node_map_)
    {
        const std::string &node_name = entry.first;
        if (!visited.count(node_name))
        {
            if (FindCycleDFS(node_name, visited, recursion_stack, cycle_path))
            {
                result.push_back(cycle_path);
                cycle_path.clear();
                recursion_stack.clear();
            }
        }
    }
    return result;
}

std::vector<std::string> VariableDependencyGraph::FindNodeCyclePath(const std::string &node_name) const
{
    if (!IsNodeExist(node_name))
        return {};

    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursion_stack;
    std::vector<std::string> cycle_path;
    if (FindCycleDFS(node_name, visited, recursion_stack, cycle_path))
    {
        return cycle_path;
    }
    else
    {
        return {};
    }
}

bool VariableDependencyGraph::FindCycleDFS(
    const std::string &node_name, std::unordered_set<std::string> &visited,
    std::unordered_set<std::string> &current_path_set, std::vector<std::string> &cycle_path
) const
{
    if (!visited.count(node_name) && IsNodeExist(node_name))
    {
        visited.insert(node_name);
        current_path_set.insert(node_name);
        cycle_path.push_back(node_name);

        for (const auto &dependency : node_map_.at(node_name)->dependencies_)
        {
            if (!visited.count(dependency) && FindCycleDFS(dependency, visited, current_path_set, cycle_path))
            {
                return true;
            }
            else if (current_path_set.count(dependency))
            {
                auto it = std::find(cycle_path.begin(), cycle_path.end(), dependency);
                if (it != cycle_path.end())
                {
                    cycle_path.erase(cycle_path.begin(), it);
                }
                cycle_path.push_back(dependency);
                return true;
            }
        }

        cycle_path.pop_back();
        current_path_set.erase(node_name);
    }
    return false;
}

bool VariableDependencyGraph::AddEdge(const Edge &edge)
{
    if (edges_.contains(edge) == true)
    {
        return false;
    }

    edges_.insert(edge);

    ActiveEdgeToNodes(edge);

    if (HasCycle())
    {
        auto cycle_path = FindNodeCyclePath(edge.from_);
        RemoveEdge(edge);
        throw DependencyCycleException({cycle_path});
    }
    return true;
}

bool VariableDependencyGraph::AddEdges(const std::vector<Edge> &edge_list)
{
    std::vector<Edge> unique_edges = edge_list;
    std::unordered_set<Edge, EdgeHash, EdgeEqual> seen;
    auto predicate = [&seen](const Edge &e) { return !seen.insert(e).second; };
    unique_edges.erase(std::remove_if(unique_edges.begin(), unique_edges.end(), predicate), unique_edges.end());

    size_t unique_size = unique_edges.size();

    unique_edges.erase(
        std::remove_if(
            unique_edges.begin(), unique_edges.end(), [this](const Edge &edge) { return edges_.contains(edge); }
        ),
        unique_edges.end()
    );

    if (unique_edges.size() != unique_size)
    {
        // some node is already in node_map
        return false;
    }

    for (const Edge &edge : unique_edges)
    {
        edges_.insert(edge);
        ActiveEdgeToNodes(edge);
    }

    if (HasCycle())
    {
        auto cycle_path_list = FindCyclePath();
        RemoveEdges(unique_edges);
        throw DependencyCycleException(cycle_path_list);
    }

    return true;
}

bool VariableDependencyGraph::RemoveEdges(const std::vector<Edge> &edge_list)
{
    std::vector<Edge> unique_edges = edge_list;
    std::unordered_set<Edge, EdgeHash, EdgeEqual> seen;
    auto predicate = [&seen](const Edge &e) { return !seen.insert(e).second; };
    unique_edges.erase(std::remove_if(unique_edges.begin(), unique_edges.end(), predicate), unique_edges.end());

    size_t unique_size = unique_edges.size();

    unique_edges.erase(
        std::remove_if(
            unique_edges.begin(), unique_edges.end(), [this](const Edge &edge) { return !edges_.contains(edge); }
        ),
        unique_edges.end()
    );

    if (unique_edges.size() != unique_size)
    {
        // some node is already in node_map
        return false;
    }

    for (const Edge &edge : unique_edges)
    {
        edges_.erase(edge);
        DeactiveEdgeToNodes(edge);
    }

    return true;
}

bool VariableDependencyGraph::HasCycle() const
{
    std::vector<std::string> top_order = TopologicalSort();

    return top_order.size() != node_map_.size();
}

std::vector<std::string> VariableDependencyGraph::TopologicalSort() const
{
    // Kahn's Algorithm
    std::unordered_map<std::string, int> in_degree;
    std::queue<std::string> zero_in_degree_queue;
    std::vector<std::string> topo_order;

    for (const auto &entry : node_map_)
    {
        in_degree[entry.first] = entry.second->dependencies_.size();
        if (in_degree[entry.first] == 0)
        {
            zero_in_degree_queue.push(entry.first);
        }
    }

    while (!zero_in_degree_queue.empty())
    {
        auto node_name = zero_in_degree_queue.front();
        zero_in_degree_queue.pop();
        topo_order.push_back(node_name);

        for (const auto &dependent : node_map_.at(node_name)->dependents_)
        {
            if (--in_degree[dependent] == 0)
            {
                zero_in_degree_queue.push(dependent);
            }
        }
    }

    return topo_order;
}

void VariableDependencyGraph::UpdateGraph(std::function<void(const std::string &)> update_callback)
{
    if (HasCycle())
    {
        return;
    }

    std::unordered_set<std::string> processed_nodes;
    for (auto &entry : node_map_)
    {
        const std::string &node_name = entry.first;
        const auto &node = entry.second;

        if (node->is_dirty_)
        {
            MakeNodeDependentsDirty(node_name, processed_nodes);
        }
    }

    auto topo_order = TopologicalSort();

    for (const auto &node_name : topo_order)
    {
        if (processed_nodes.count(node_name))
        {
            update_callback(node_name);
            node_map_[node_name]->is_dirty_ = false;
        }
    }
}

void VariableDependencyGraph::MakeNodeDependentsDirty(
    const std::string &node_name, std::unordered_set<std::string> &processed_nodes
)
{
    if (IsNodeExist(node_name) == false)
    {
        return;
    }

    if (processed_nodes.count(node_name))
    {
        return;
    }

    processed_nodes.insert(node_name);

    const auto &node = node_map_.at(node_name);
    node->is_dirty_ = true;

    for (const auto &dependent : node->dependents_)
    {
        MakeNodeDependentsDirty(dependent, processed_nodes);
    }
}

void VariableDependencyGraph::Reset()
{
    node_map_.clear();
    edges_.clear();
}

void VariableDependencyGraph::ActiveEdgeToNodes(const VariableDependencyGraph::Edge &edge)
{
    if (node_map_.count(edge.from_) && node_map_.count(edge.to_))
    {
        node_map_[edge.from_]->dependencies_.insert(edge.to_);
        node_map_[edge.to_]->dependents_.insert(edge.from_);
    }
}

void VariableDependencyGraph::DeactiveEdgeToNodes(const VariableDependencyGraph::Edge &edge)
{
    if (node_map_.count(edge.from_))
    {
        node_map_[edge.from_]->dependencies_.erase(edge.to_);
    }
    if (node_map_.count(edge.to_))
    {
        node_map_[edge.to_]->dependents_.erase(edge.from_);
    }
}
