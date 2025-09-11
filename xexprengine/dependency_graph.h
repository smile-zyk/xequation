#pragma once
#include <boost/container_hash/hash_fwd.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index_container.hpp>

namespace xexprengine
{
class DependencyCycleException : public std::runtime_error
{
  public:
    explicit DependencyCycleException(const std::vector<std::string> &cycle_path)
        : std::runtime_error(BuildErrorMessage(cycle_path)), cycle_path_(cycle_path)
    {
    }

    DependencyCycleException(const DependencyCycleException &) = default;
    DependencyCycleException &operator=(const DependencyCycleException &) = default;

    DependencyCycleException(DependencyCycleException &&) = default;
    DependencyCycleException &operator=(DependencyCycleException &&) = default;

    const std::vector<std::string> &cycle_path() const noexcept
    {
        return cycle_path_;
    }

  private:
    static std::string BuildErrorMessage(const std::vector<std::string> &cycle_path);
    std::vector<std::string> cycle_path_;
};

class DependencyGraph
{
  public:
    class Node
    {
      public:
        Node() {}

        ~Node() = default;
        Node(const Node &) = default;
        Node &operator=(const Node &) = default;
        Node(Node &&) = default;
        Node &operator=(Node &&) = default;

        const std::unordered_set<std::string> &dependencies() const
        {
            return dependencies_;
        }

        const std::unordered_set<std::string> &dependents() const
        {
            return dependents_;
        }

        bool dirty_flag() const
        {
            return dirty_flag_;
        }

        uint64_t event_stamp() const
        {
            return event_stamp_;
        }

      private:
        std::unordered_set<std::string> dependencies_;
        std::unordered_set<std::string> dependents_;
        bool dirty_flag_;
        uint64_t event_stamp_;
        friend class DependencyGraph;
    };

    class Edge
    {
      public:
        Edge(const std::string &from, const std::string &to) : from_(from), to_(to) {}
        ~Edge() = default;

        Edge(const Edge &) = default;
        Edge &operator=(const Edge &) = default;
        Edge(Edge &&) = default;
        Edge &operator=(Edge &&) = default;

        const std::string &from() const
        {
            return from_;
        }
        const std::string &to() const
        {
            return to_;
        }

      private:
        std::string from_;
        std::string to_;
    };

    struct EdgeContainer
    {
        struct ByFrom
        {};

        struct ByTo
        {};

        struct EdgeHash
        {
            size_t operator()(const Edge &edge) const
            {
                size_t seed = 0;
                boost::hash_combine(seed, edge.from());
                boost::hash_combine(seed, edge.to());
                return seed;
            }
        };

        struct EdgeEqual
        {
            bool operator()(const Edge &lhs, const Edge &rhs) const
            {
                return lhs.from() == rhs.from() && lhs.to() == rhs.to();
            }
        };

        typedef boost::multi_index::multi_index_container<
            Edge, boost::multi_index::indexed_by<
                      boost::multi_index::hashed_unique<boost::multi_index::identity<Edge>, EdgeHash, EdgeEqual>,
                      boost::multi_index::hashed_non_unique<
                          boost::multi_index::tag<ByFrom>, boost::multi_index::const_mem_fun<Edge, const std::string&, &Edge::from>>,
                      boost::multi_index::hashed_non_unique<
                          boost::multi_index::tag<ByTo>, boost::multi_index::const_mem_fun<Edge, const std::string&, &Edge::to>>>>
            Type;

        typedef std::pair<
            Type::index<ByFrom>::type::const_iterator, EdgeContainer::Type::index<ByFrom>::type::const_iterator>
            RangeByFrom;
        typedef std::pair<
            Type::index<ByTo>::type::const_iterator, EdgeContainer::Type::index<ByTo>::type::const_iterator>
            RangeByTo;
        typedef std::pair<Type::const_iterator, Type::const_iterator> Range;
    };

    class BatchUpdateGuard
    {
      public:
        explicit BatchUpdateGuard(DependencyGraph *graph) : graph_(graph), committed_(false)
        {
            can_update_ = graph_->BeginBatchUpdate();
        }
        ~BatchUpdateGuard() noexcept
        {
            if (can_update_ && !committed_)
            {
                graph_->EndBatchUpdateNoThrow();
            }
        }
        void commit()
        {
            if(can_update_)
            {
              committed_ = true;
              graph_->EndBatchUpdate();
            }
        }

      private:
        DependencyGraph *graph_;
        bool committed_;
        bool can_update_;
    };

    DependencyGraph() = default;
    ~DependencyGraph() = default;

    DependencyGraph(const DependencyGraph &) = delete;
    DependencyGraph &operator=(const DependencyGraph &) = delete;

    DependencyGraph(DependencyGraph &&) = default;
    DependencyGraph &operator=(DependencyGraph &&) = default;

    // get node and edge
    const Node* GetNode(const std::string& node_name) const;
    Node* GetNode(const std::string& node_name);
    EdgeContainer::RangeByFrom GetEdgesByFrom(const std::string &from) const;
    EdgeContainer::RangeByTo GetEdgesByTo(const std::string &to) const;
    EdgeContainer::Range GetAllEdges() const;
    bool IsNodeExist(const std::string &node_name) const;
    bool IsEdgeExist(const Edge &edge) const;

    bool BeginBatchUpdate();
    bool EndBatchUpdate();
    bool EndBatchUpdateNoThrow() noexcept;

    // single operation
    bool AddNode(const std::string &node_name);
    bool RemoveNode(const std::string &node_name);
    bool AddEdge(const Edge &edge);
    bool RemoveEdge(const Edge &edge);

    // batch operation
    bool AddNodes(const std::vector<std::string> &node_list);
    bool RemoveNodes(const std::vector<std::string> &node_list);
    bool AddEdges(const std::vector<Edge> &edge_list);
    bool RemoveEdges(const std::vector<Edge> &edge_list);

    // call when node is modified
    bool InvalidateNode(const std::string& node_name);
    // call when node bind data actual change
    bool UpdateEventStamp(const std::string& node_name);

    void Traversal(std::function<void(const std::string &)> callback) const;
    void Reset();

    // topological sort
    std::vector<std::string> TopologicalSort() const;

  private:
    // for rollback
    struct Operation
    {
        enum class Type
        {
            kAddNode,
            kRemoveNode,
            kAddEdge,
            kRemoveEdge,
        };

        Operation(Type type_, const std::string &node_) : type(type_)
        {
            new (&node) std::string(node_);
        }
        Operation(Type type_, const Edge &edge_) : type(type_)
        {
            new (&edge) Edge(edge_);
        }

        ~Operation()
        {
            switch (type)
            {
            case Type::kAddNode:
            case Type::kRemoveNode:
                node.~basic_string();
                break;
            case Type::kAddEdge:
            case Type::kRemoveEdge:
                edge.~Edge();
                break;
            }
        }

        Operation(const Operation &other) : type(other.type)
        {
            switch (type)
            {
            case Type::kAddNode:
            case Type::kRemoveNode:
                new (&node) std::string(other.node);
                break;
            case Type::kAddEdge:
            case Type::kRemoveEdge:
                new (&edge) Edge(other.edge);
                break;
            }
        }

        Operation(Operation &&other) noexcept : type(other.type)
        {
            switch (type)
            {
            case Type::kAddNode:
            case Type::kRemoveNode:
                new (&node) std::string(std::move(other.node));
                break;
            case Type::kAddEdge:
            case Type::kRemoveEdge:
                new (&edge) Edge(std::move(other.edge));
                break;
            }
        }

        Operation &operator=(const Operation &other)
        {
            if (this != &other)
            {
                this->~Operation();
                new (this) Operation(other);
            }
            return *this;
        }

        Operation &operator=(Operation &&other) noexcept
        {
            if (this != &other)
            {
                this->~Operation();
                new (this) Operation(std::move(other));
            }
            return *this;
        }

        Type type;
        union
        {
            std::string node;
            Edge edge;
        };
    };

    void RollBack() noexcept;
    void ActiveEdge(const Edge &edge);
    void DeactiveEdge(const Edge &edge);
    bool CheckCycle(std::vector<std::string>& cycle_path);

    std::unordered_map<std::string, std::unique_ptr<Node>> node_map_;
    EdgeContainer::Type edge_container_;
    bool batch_update_in_progress_{false};
    std::stack<Operation> operation_stack_;
};
} // namespace xexprengine