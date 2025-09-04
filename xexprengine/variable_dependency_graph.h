#pragma once
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index_container.hpp>

#include "variable.h"

namespace xexprengine
{
class DependencyCycleException : public std::runtime_error
{
  public:
    explicit DependencyCycleException(const std::vector<std::vector<std::string>> &cycle_path_list)
        : std::runtime_error(BuildErrorMessage(cycle_path_list)), cycle_path_list_(cycle_path_list)
    {
    }

    const std::vector<std::vector<std::string>> &GetCyclePath() const noexcept
    {
        return cycle_path_list_;
    }

  private:
    static std::string BuildErrorMessage(const std::vector<std::vector<std::string>> &cycle_path_list)
    {
        std::string msg = "Dependency cycle detected: ";
        for (const std::vector<std::string> &cycle_path : cycle_path_list)
        {
            msg += "{";
            for (size_t i = 0; i < cycle_path.size(); ++i)
            {
                if (i != 0)
                    msg += " -> ";
                msg += cycle_path[i];
            }
            msg += "}";
        }
        return msg;
    }

    std::vector<std::vector<std::string>> cycle_path_list_;
};

class VariableDependencyGraph
{
  public:
    class Edge
    {
      public:
        Edge(const std::string &from, const std::string &to) : from_(from), to_(to) {}
        const std::string &from() const
        {
            return from_;
        }
        const std::string &to() const
        {
            return to_;
        }

      protected:
        std::string from_;
        std::string to_;
        friend class VariableDependencyGraph;
    };

    class Node
    {
      public:
        Node(std::unique_ptr<Variable> var) : variable_(std::move(var)) {}
        const std::unordered_set<std::string> &dependencies() const
        {
            return dependencies_;
        }
        const std::unordered_set<std::string> &dependents() const
        {
            return dependents_;
        }
        bool is_dirty() const
        {
            return is_dirty_;
        }

      private:
        std::unordered_set<std::string> dependencies_;
        std::unordered_set<std::string> dependents_;
        std::unique_ptr<Variable> variable_;
        bool is_dirty_;
        friend class VariableDependencyGraph;
    };

    std::vector<std::string> TopologicalSort() const;

    std::unordered_set<std::string> GetNodeDependencies(const std::string &node_name) const;
    std::unordered_set<std::string> GetNodeDependents(const std::string &node_name) const;

    bool IsNodeDirty(const std::string &node_name) const;
    bool IsNodeExist(const std::string &node_name) const;

    std::vector<Edge> GetEdgesByFrom(const std::string &from) const;
    std::vector<Edge> GetEdgesByFrom(const std::vector<std::string> &from_list) const;
    std::vector<Edge> GetEdgesByTo(const std::string &to) const;
    std::vector<Edge> GetEdgesByTo(const std::vector<std::string> &to_list) const;
    std::vector<Edge> GetAllEdges() const;

    Variable* GetVariable(const std::string &node_name) const;

  protected:
    bool AddNode(std::unique_ptr<Variable> var);
    bool AddNodes(const std::vector<std::unique_ptr<Variable>> &var_list);
    bool RemoveNode(const std::string &node_name);
    bool RemoveNodes(const std::vector<std::string> &node_list);
    bool RenameNode(const std::string &old_name, const std::string &new_name);
    bool SetNodeDirty(const std::string &node_name, bool dirty);
    bool ClearNodeDependencyEdges(const std::string &node_name);
    bool AddEdge(const std::string &from, const std::string &to);
    bool AddEdge(const Edge &edge);
    bool AddEdges(const std::vector<Edge> &edge_list);
    bool RemoveEdge(const Edge &edge);
    bool RemoveEdges(const std::vector<Edge> &edge_list);
    void UpdateGraph(std::function<void(const std::string &)> update_callback);
    void Reset();

    friend class ExprContext;

  private:
    void ActiveEdgeToNodes(const Edge &edge);
    void DeactiveEdgeToNodes(const Edge &edge);
    bool HasCycle() const;
    std::vector<std::string> FindNodeCyclePath(const std::string &node_name) const;
    std::vector<std::vector<std::string>> FindCyclePath() const;
    bool FindCycleDFS(
        const std::string &node_name, std::unordered_set<std::string> &visited,
        std::unordered_set<std::string> &recursionStack, std::vector<std::string> &cycle_path
    ) const;
    void MakeNodeDependentsDirty(const std::string &node_name, std::unordered_set<std::string> &processed_nodes);

    struct EdgeHash
    {
        size_t operator()(const Edge &edge) const
        {
            return std::hash<std::string>()(edge.from()) ^ (std::hash<std::string>()(edge.to()) << 1);
        }
    };

    struct EdgeEqual
    {
        bool operator()(const Edge &lhs, const Edge &rhs) const
        {
            return lhs.from() == rhs.from() && lhs.to() == rhs.to();
        }
    };

    struct EdgeContainer
    {
        struct ByFrom
        {};
        struct ByTo
        {};

        typedef boost::multi_index::multi_index_container<
            Edge, boost::multi_index::indexed_by<
                      boost::multi_index::hashed_unique<boost::multi_index::identity<Edge>, EdgeHash, EdgeEqual>,
                      boost::multi_index::hashed_non_unique<
                          boost::multi_index::tag<ByFrom>, boost::multi_index::member<Edge, std::string, &Edge::from_>>,
                      boost::multi_index::hashed_non_unique<
                          boost::multi_index::tag<ByTo>, boost::multi_index::member<Edge, std::string, &Edge::to_>>>>
            Type;
    };

    std::unordered_map<std::string, std::unique_ptr<Node>> node_map_;
    EdgeContainer::Type edges_;
};
} // namespace xexprengine