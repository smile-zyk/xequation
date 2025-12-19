#include "dependency_graph.h"
#include <iostream>
#include <queue>
#include <string>
#include <tsl/ordered_set.h>
#include <unordered_set>

using namespace xequation;

std::string DependencyCycleException::BuildErrorMessage(const std::vector<std::string> &cycle_path)
{
    std::string msg = "Dependency cycle detected: ";

    for (size_t i = 0; i < cycle_path.size(); ++i)
    {
        if (i != 0)
            msg += " -> ";
        msg += cycle_path[i];
    }

    return msg;
}

const DependencyGraph::Node *DependencyGraph::GetNode(const std::string &node_name) const
{
    auto it = node_map_.find(node_name);
    if (it != node_map_.end())
    {
        return it->second.get();
    }
    return nullptr;
}

DependencyGraph::EdgeContainer::RangeByFrom DependencyGraph::GetEdgesByFrom(const std::string &from) const
{
    return edge_container_.get<EdgeContainer::ByFrom>().equal_range(from);
}

DependencyGraph::EdgeContainer::RangeByTo DependencyGraph::GetEdgesByTo(const std::string &to) const
{
    return edge_container_.get<EdgeContainer::ByTo>().equal_range(to);
}

DependencyGraph::EdgeContainer::Range DependencyGraph::GetAllEdges() const
{
    return std::make_pair(edge_container_.begin(), edge_container_.end());
}

bool DependencyGraph::IsNodeExist(const std::string &node_name) const
{
    return node_map_.find(node_name) != node_map_.end();
}

bool DependencyGraph::IsEdgeExist(const Edge &edge) const
{
    return edge_container_.contains(edge);
}

bool DependencyGraph::BeginBatchUpdate()
{
    if (batch_update_in_progress_ == true)
    {
        return false;
    }

    batch_update_in_progress_ = true;

    while (!operation_stack_.empty())
    {
        operation_stack_.pop();
    }
    return true;
}

bool DependencyGraph::EndBatchUpdate()
{
    if (batch_update_in_progress_ == false)
    {
        return false;
    }

    batch_update_in_progress_ = false;
    std::vector<std::string> cycle_path;
    if (CheckCycle(cycle_path))
    {
        RollBack();
        throw DependencyCycleException(cycle_path);
    }
    else
    {
        while (!operation_stack_.empty())
        {
            operation_stack_.pop();
        }
    }
    return true;
}

bool DependencyGraph::EndBatchUpdateNoThrow() noexcept
{
    if (batch_update_in_progress_ == false)
    {
        return false;
    }

    batch_update_in_progress_ = false;
    std::vector<std::string> cycle_path;
    if (CheckCycle(cycle_path))
    {
        RollBack();
    }
    else
    {
        while (!operation_stack_.empty())
        {
            operation_stack_.pop();
        }
    }
    return true;
}

bool DependencyGraph::AddNode(const std::string &node_name)
{
    if (IsNodeExist(node_name) == true)
    {
        return false;
    }

    node_map_.insert({node_name, std::unique_ptr<Node>(new Node())});

    auto node_dependency_edges = GetEdgesByFrom(node_name);
    for (auto it = node_dependency_edges.first; it != node_dependency_edges.second; it++)
    {
        ActiveEdge(*it);
    }

    auto node_dependent_edges = GetEdgesByTo(node_name);
    for (auto it = node_dependent_edges.first; it != node_dependent_edges.second; it++)
    {
        ActiveEdge(*it);
    }

    if (batch_update_in_progress_ == true)
    {
        operation_stack_.push(Operation(Operation::Type::kAddNode, node_name));
        return true;
    }

    std::vector<std::string> cycle_path;
    if (CheckCycle(cycle_path))
    {
        RemoveNode(node_name);
        throw DependencyCycleException(cycle_path);
    }
    return true;
}

bool DependencyGraph::RemoveNode(const std::string &node_name) noexcept
{
    if (IsNodeExist(node_name) == false)
    {
        return false;
    }

    node_map_.erase(node_name);

    auto node_dependency_edges = GetEdgesByFrom(node_name);
    for (auto it = node_dependency_edges.first; it != node_dependency_edges.second; it++)
    {
        DeactiveEdge(*it);
    }

    auto node_dependent_edges = GetEdgesByTo(node_name);
    for (auto it = node_dependent_edges.first; it != node_dependent_edges.second; it++)
    {
        DeactiveEdge(*it);
    }

    if (batch_update_in_progress_ == true)
    {
        operation_stack_.push(Operation(Operation::Type::kRemoveNode, node_name));
    }

    return true;
}

bool DependencyGraph::AddEdge(const Edge &edge)
{
    if (edge_container_.contains(edge) == true)
    {
        return false;
    }

    edge_container_.insert(edge);

    ActiveEdge(edge);

    if (batch_update_in_progress_ == true)
    {
        operation_stack_.push(Operation(Operation::Type::kAddEdge, edge));
        return true;
    }

    std::vector<std::string> cycle_path;
    if (CheckCycle(cycle_path))
    {
        RemoveEdge(edge);
        throw DependencyCycleException(cycle_path);
    }
    return true;
}

bool DependencyGraph::RemoveEdge(const Edge &edge) noexcept
{
    if (edge_container_.contains(edge) == false)
    {
        return false;
    }
    edge_container_.erase(edge);
    DeactiveEdge(edge);
    if (batch_update_in_progress_ == true)
    {
        operation_stack_.push(Operation(Operation::Type::kRemoveEdge, edge));
    }
    return true;
}

bool DependencyGraph::AddNodes(const std::vector<std::string> &node_list)
{
    BatchUpdateGuard guard(this);
    bool res = true;
    for (const std::string &node_name : node_list)
    {
        res &= AddNode(node_name);
    }
    guard.commit();
    return res;
}

bool DependencyGraph::RemoveNodes(const std::vector<std::string> &node_list) noexcept
{
    bool res = true;
    for (const std::string &node_name : node_list)
    {
        res &= RemoveNode(node_name);
    }
    return res;
}

bool DependencyGraph::AddEdges(const std::vector<Edge> &edge_list)
{
    BatchUpdateGuard guard(this);
    bool res = true;
    for (const Edge &edge : edge_list)
    {
        res &= AddEdge(edge);
    }
    guard.commit();
    return res;
}

bool DependencyGraph::RemoveEdges(const std::vector<Edge> &edge_list) noexcept
{
    bool res = true;
    for (const Edge &edge : edge_list)
    {
        res &= RemoveEdge(edge);
    }
    return res;
}

std::vector<std::string> DependencyGraph::TopologicalSort(const std::vector<std::string>& nodes) const
{
    if (nodes.empty())
    {
        return {};
    }

    // Kahn's Algorithm for specific nodes and their dependencies
    std::unordered_map<std::string, int> in_degree;
    std::queue<std::string> zero_in_degree_queue;
    std::vector<std::string> topo_order;

    std::unordered_set<std::string> relevant_nodes;
    std::queue<std::string> node_queue;
    
    for (const auto &node : nodes)
    {
        if (node_map_.find(node) == node_map_.end())
        {
            continue;
        }
        node_queue.push(node);
        relevant_nodes.insert(node);
    }

    while (!node_queue.empty())
    {
        auto current_node = node_queue.front();
        node_queue.pop();

        for (const auto &dep : node_map_.at(current_node)->dependents_)
        {
            if (relevant_nodes.find(dep) == relevant_nodes.end())
            {
                relevant_nodes.insert(dep);
                node_queue.push(dep);
            }
        }
    }

    for (const auto &node_name : relevant_nodes)
    {
        int count = 0;
        for (const auto &dep : node_map_.at(node_name)->dependencies_)
        {
            if (relevant_nodes.find(dep) != relevant_nodes.end())
            {
                count++;
            }
        }
        in_degree[node_name] = count;

        if (in_degree[node_name] == 0)
        {
            zero_in_degree_queue.push(node_name);
        }
    }

    while (!zero_in_degree_queue.empty())
    {
        auto node_name = zero_in_degree_queue.front();
        zero_in_degree_queue.pop();
        topo_order.push_back(node_name);

        for (const auto &dependent : node_map_.at(node_name)->dependents_)
        {
            if (relevant_nodes.find(dependent) != relevant_nodes.end())
            {
                if (--in_degree[dependent] == 0)
                {
                    zero_in_degree_queue.push(dependent);
                }
            }
        }
    }

    if (topo_order.size() != relevant_nodes.size())
    {
        return {};
    }

    return topo_order;
}

std::vector<std::string> DependencyGraph::TopologicalSort(const std::string &node) const
{
    if (node_map_.find(node) == node_map_.end())
    {
        return {};
    }

    // Kahn's Algorithm for specific node and its dependencies
    std::unordered_map<std::string, int> in_degree;
    std::queue<std::string> zero_in_degree_queue;
    std::vector<std::string> topo_order;

    std::unordered_set<std::string> relevant_nodes;
    std::queue<std::string> node_queue;
    node_queue.push(node);
    relevant_nodes.insert(node);

    while (!node_queue.empty())
    {
        auto current_node = node_queue.front();
        node_queue.pop();

        for (const auto &dep : node_map_.at(current_node)->dependents_)
        {
            if (relevant_nodes.find(dep) == relevant_nodes.end())
            {
                relevant_nodes.insert(dep);
                node_queue.push(dep);
            }
        }
    }

    for (const auto &node_name : relevant_nodes)
    {
        int count = 0;
        for (const auto &dep : node_map_.at(node_name)->dependencies_)
        {
            if (relevant_nodes.find(dep) != relevant_nodes.end())
            {
                count++;
            }
        }
        in_degree[node_name] = count;

        if (in_degree[node_name] == 0)
        {
            zero_in_degree_queue.push(node_name);
        }
    }

    while (!zero_in_degree_queue.empty())
    {
        auto node_name = zero_in_degree_queue.front();
        zero_in_degree_queue.pop();
        topo_order.push_back(node_name);

        for (const auto &dependent : node_map_.at(node_name)->dependents_)
        {
            if (relevant_nodes.find(dependent) != relevant_nodes.end())
            {
                if (--in_degree[dependent] == 0)
                {
                    zero_in_degree_queue.push(dependent);
                }
            }
        }
    }

    if (topo_order.size() != relevant_nodes.size())
    {
        return {};
    }

    return topo_order;
}

std::vector<std::string> DependencyGraph::TopologicalSort() const
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

void DependencyGraph::InvalidateNode(const std::string &node_name)
{
    MakeNodeDirty(node_name, true, true);
}

void DependencyGraph::MakeNodeDirty(const std::string &node_name, bool dirty, bool make_dependent)
{
    if(node_map_.count(node_name) == 0)
    {
        return;
    }
    auto node = node_map_.at(node_name).get();
    node->set_dirty_flag(dirty);
    if (make_dependent)
    {
        for (const auto &dependent : node->dependents_)
        {
            MakeNodeDirty(dependent, dirty, make_dependent);
        }
    }
}

void DependencyGraph::Traversal(std::function<void(const std::string &)> callback) const
{
    auto topo_order = TopologicalSort();

    for (const auto &node_name : topo_order)
    {
        callback(node_name);
    }
}

void DependencyGraph::Reset()
{
    node_map_.clear();
    edge_container_.clear();
    while (!operation_stack_.empty())
    {
        operation_stack_.pop();
    }
    batch_update_in_progress_ = false;
}

void DependencyGraph::ActiveEdge(const DependencyGraph::Edge &edge)
{
    if (node_map_.count(edge.from()) && node_map_.count(edge.to()))
    {
        node_map_[edge.from()]->dependencies_.insert(edge.to());
        node_map_[edge.to()]->dependents_.insert(edge.from());
        node_dependency_changed_signal_(edge.from());
        node_dependent_changed_signal_(edge.to());
    }
}

void DependencyGraph::DeactiveEdge(const DependencyGraph::Edge &edge)
{
    if (node_map_.count(edge.from()))
    {
        node_map_[edge.from()]->dependencies_.erase(edge.to());
        node_dependency_changed_signal_(edge.from());
    }
    if (node_map_.count(edge.to()))
    {
        node_map_[edge.to()]->dependents_.erase(edge.from());
        node_dependent_changed_signal_(edge.to());
    }
}

bool DependencyGraph::CheckCycle(std::vector<std::string> &cycle_path) const
{
    std::unordered_map<std::string, int> visited; // 0: unvisited, 1: visiting, 2: visited
    std::stack<std::pair<std::string, tsl::ordered_set<std::string>::iterator>>
        stack;
    std::unordered_map<std::string, std::string> path_predecessor;

    for (const auto &entry : node_map_)
    {
        visited[entry.first] = 0;
    }

    for (const auto &entry : node_map_)
    {
        const std::string &start_node = entry.first;
        if (visited[start_node] == 0)
        {
            stack.push(std::make_pair(start_node, node_map_.at(start_node)->dependencies_.begin()));
            visited[start_node] = 1;
            path_predecessor[start_node] = "";

            while (!stack.empty())
            {
                std::string current_node = stack.top().first;
                auto &current_iter = stack.top().second;

                if (current_iter != node_map_.at(current_node)->dependencies_.end())
                {
                    std::string next_neighbor = *current_iter;
                    ++current_iter;

                    if (visited[next_neighbor] == 0)
                    {
                        visited[next_neighbor] = 1;
                        stack.push(std::make_pair(next_neighbor, node_map_.at(next_neighbor)->dependencies_.begin()));
                        path_predecessor[next_neighbor] = current_node; 
                    }
                    else if (visited[next_neighbor] == 1)
                    {
                        cycle_path.clear();
                        cycle_path.push_back(next_neighbor);
                        std::string temp = current_node;
                        while (temp != next_neighbor)
                        {
                            cycle_path.push_back(temp);
                            temp = path_predecessor[temp];
                        }
                        cycle_path.push_back(next_neighbor);
                        std::reverse(cycle_path.begin(), cycle_path.end());
                        return true;
                    }
                }
                else
                {
                    visited[current_node] = 2;
                    stack.pop();
                }
            }
        }
    }
    return false;
}

void DependencyGraph::RollBack() noexcept
{
    try
    {
        while (!operation_stack_.empty())
        {
            Operation op = operation_stack_.top();
            operation_stack_.pop();
            switch (op.type)
            {
            case Operation::Type::kAddNode:
                RemoveNode(op.node);
                break;
            case Operation::Type::kRemoveNode:
                AddNode(op.node);
                break;
            case Operation::Type::kAddEdge:
                RemoveEdge(op.edge);
                break;
            case Operation::Type::kRemoveEdge:
                AddEdge(op.edge);
                break;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Rollback failed: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Rollback failed due to unknown exception." << std::endl;
    }
}

boost::signals2::scoped_connection DependencyGraph::ConnectNodeDependencyChangedSignal(
    const boost::signals2::signal<void(const std::string &)> ::slot_type &slot)
{
    return node_dependency_changed_signal_.connect(slot);
}

boost::signals2::scoped_connection DependencyGraph::ConnectNodeDependentChangedSignal(
    const boost::signals2::signal<void(const std::string &)> ::slot_type &slot)
{
    return node_dependent_changed_signal_.connect(slot);
}