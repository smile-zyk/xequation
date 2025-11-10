#include "equation_manager.h"
#include "core/equation.h"
#include "core/equation_common.h"
#include "dependency_graph.h"
#include "equation.h"
#include "equation_common.h"
#include "event_stamp.h"
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>


using namespace xequation;

EquationManager::EquationManager(
    std::unique_ptr<EquationContext> context, ExecCallback eval_callback, ParseCallback parse_callback,
    EvalCallback exec_callback
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
        return equation_map_.at(eqn_name).get();
    }
    return nullptr;
}

const std::vector<std::string> EquationManager::GetAllEquationNames() const
{
    std::vector<std::string> eqn_names;
    for (const auto &pair : equation_map_)
    {
        eqn_names.push_back(pair.first);
    }
    return eqn_names;
}

size_t EquationManager::GetEquationCount() const
{
    return equation_map_.size();
}

void EquationManager::AddEquation(const std::string &eqn_name, const std::string &expression)
{
    std::string statement = eqn_name + "=" + expression;
    auto res = parse_callback_(statement);
    if (res.size() != 1)
    {
        throw std::runtime_error("'" + statement + "' is not a single variable equation");
    }

    AddEquationStatement(statement);
    single_variable_equation_name_set_.insert(eqn_name);
}

void EquationManager::EditEquation(
    const std::string &old_eqn_name, const std::string &new_eqn_name, const std::string &new_eqn_expr
)
{
    if (single_variable_equation_name_set_.count(old_eqn_name) == 0)
    {
        throw std::runtime_error("equation '" + old_eqn_name + "' is not a single variable equation");
    }

    std::string statement = new_eqn_name + "=" + new_eqn_expr;
    auto res = parse_callback_(statement);
    if (res.size() != 1)
    {
        throw std::runtime_error("'" + statement + "' is not a single variable equation");
    }

    EditEquationStatement(GetEquation(old_eqn_name)->content(), statement);

    single_variable_equation_name_set_.erase(old_eqn_name);
    single_variable_equation_name_set_.insert(new_eqn_name);
}

void EquationManager::RemoveEquation(const std::string &eqn_name)
{
    if (single_variable_equation_name_set_.count(eqn_name) == 0)
    {
        throw std::runtime_error("equation '" + eqn_name + "' is not a single variable equation");
    }

    RemoveEquationStatement(GetEquation(eqn_name)->content());

    single_variable_equation_name_set_.erase(eqn_name);
}

void EquationManager::AddMultipleEquations(const std::string &eqn_code)
{
    AddEquationStatement(eqn_code);
    multiple_variable_equation_code_set_.insert(eqn_code);
}

void EquationManager::EditMultipleEquations(const std::string &old_eqn_code, const std::string &new_eqn_code)
{
    if (multiple_variable_equation_code_set_.count(old_eqn_code) == 0)
    {
        throw std::runtime_error("statement '" + old_eqn_code + "' is not exist");
    }
    EditEquationStatement(old_eqn_code, new_eqn_code);
    multiple_variable_equation_code_set_.erase(old_eqn_code);
    multiple_variable_equation_code_set_.insert(new_eqn_code);
}

void EquationManager::RemoveMultipleEquations(const std::string &eqn_code)
{
    if (multiple_variable_equation_code_set_.count(eqn_code) == 0)
    {
        throw std::runtime_error("statement '" + eqn_code + "' is not exist");
    }
    RemoveEquationStatement(eqn_code);
    multiple_variable_equation_code_set_.erase(eqn_code);
}

void EquationManager::AddEquationStatement(const std::string &eqn_code)
{
    ParseResult res = parse_callback_(eqn_code);

    for (const auto &item : res)
    {
        if (IsEquationExist(item.name))
        {
            throw DuplicateEquationNameError(item.name, GetEquation(item.name)->content());
        }
    }

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    for (const auto &item : res)
    {
        AddNodeToGraph(item.name, item.dependencies);
    }
    guard.commit();
    for (const auto &item : res)
    {
        graph_->InvalidateNode(item.name);
        equation_map_.insert({item.name, ConstructEquationPtr(item)});
    }
}

void EquationManager::EditEquationStatement(const std::string &old_eqn_code, const std::string &new_eqn_code)
{
    ParseResult old_res = parse_callback_(old_eqn_code);
    ParseResult new_res = parse_callback_(new_eqn_code);

    std::unordered_map<std::string, ParseResultItem> old_name_map;
    for (const auto &item : old_res)
    {
        old_name_map.insert({item.name, item});
    }

    std::unordered_map<std::string, ParseResultItem> new_name_map;
    for (const auto &item : new_res)
    {
        new_name_map.insert({item.name, item});
    }

    for (const auto &new_eqn : new_name_map)
    {
        std::string new_eqn_name = new_eqn.first;
        if (old_name_map.find(new_eqn_name) == old_name_map.end() && IsEquationExist(new_eqn_name))
        {
            throw DuplicateEquationNameError(new_eqn_name, GetEquation(new_eqn_name)->content());
        }
    }

    std::vector<ParseResultItem> to_remove;
    std::vector<ParseResultItem> to_add;
    std::vector<std::pair<ParseResultItem, ParseResultItem>> to_update; // pair<old_item, new_item>

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    for (const auto &old_item : old_res)
    {
        auto new_it = new_name_map.find(old_item.name);
        if (new_it == new_name_map.end())
        {
            to_remove.push_back(old_item);
        }
        else if (old_item != new_it->second)
        {
            to_update.push_back({old_item, new_it->second});
        }
    }

    for (const auto &new_item : new_res)
    {
        if (old_name_map.find(new_item.name) == old_name_map.end())
        {
            to_add.push_back(new_item);
        }
    }

    for (const auto &item : to_remove)
    {
        RemoveNodeInGraph(item.name);
    }

    for (const auto &item : to_add)
    {
        AddNodeToGraph(item.name, item.dependencies);
    }

    for (const auto &entry : to_update)
    {
        const auto& old_item = entry.first;
        const auto& new_item = entry.second;
        RemoveNodeInGraph(old_item.name);
        AddNodeToGraph(new_item.name, new_item.dependencies);
    }

    guard.commit();

    for (const auto &item : to_remove)
    {
        if (new_name_map.find(item.name) == new_name_map.end())
        {
            auto range = graph_->GetEdgesByTo(item.name);
            for (auto it = range.first; it != range.second; it++)
            {
                graph_->InvalidateNode(it->from());
            }
        }
        equation_map_.erase(item.name);
        context_->Remove(item.name);
    }

    for (const auto &entry : to_update)
    {
        const auto& old_item = entry.first;
        const auto& new_item = entry.second;

        context_->Remove(old_item.name);
        UpdateEquationPtr(new_item);
        graph_->InvalidateNode(new_item.name);
    }

    for (const auto &item : to_add)
    {
        graph_->InvalidateNode(item.name);
        equation_map_.insert({item.name, std::move(ConstructEquationPtr(item))});
    }
}

void EquationManager::RemoveEquationStatement(const std::string &eqn_code)
{
    ParseResult res = parse_callback_(eqn_code);

    for (const auto &item : res)
    {
        if (IsEquationExist(item.name) == false)
        {
            throw std::runtime_error("equation '" + item.name + "' is not exist.");
        }
    }

    for (const auto &item : res)
    {
        graph_->InvalidateNode(item.name);
        RemoveNodeInGraph(item.name);
        equation_map_.erase(item.name);
        context_->Remove(item.name);
    }
}

bool EquationManager::IsEquationExist(const std::string &var_name) const
{
    return graph_->IsNodeExist(var_name) && equation_map_.count(var_name);
}

EvalResult EquationManager::Eval(const std::string &expression) const
{
    return eval_callback_(expression, context_.get());
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
    Equation *eqn = equation_map_.at(eqn_name).get();

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

    const std::string &eqn_code = eqn->content();
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
    eqn->set_status(result.status);
    eqn->set_message(result.message);
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

std::unique_ptr<Equation> EquationManager::ConstructEquationPtr(const ParseResultItem &item)
{
    std::unique_ptr<Equation> equation = std::unique_ptr<Equation>(new Equation(this));
    equation->set_name(item.name);
    equation->set_content(item.content);
    equation->set_type(item.type);
    equation->set_dependencies(item.dependencies);
    return equation;
}

void EquationManager::UpdateEquationPtr(const ParseResultItem& item)
{
    if(equation_map_.count(item.name) == 0)
    {
        return;
    }

    Equation* eqn = equation_map_.at(item.name).get();
    
    eqn->set_content(item.content);
    eqn->set_type(item.type);
    eqn->set_dependencies(item.dependencies);
}

void EquationManager::Update()
{
    graph_->Traversal([&](const std::string &eqn_name) { UpdateEquationInternal(eqn_name); });
}

void EquationManager::UpdateEquation(const std::string &eqn_name)
{
    if (single_variable_equation_name_set_.count(eqn_name) == 0)
    {
        throw std::runtime_error("equation '" + eqn_name + "' is not a single variable equation");
    }
    auto topo_order = graph_->TopologicalSort(eqn_name);

    for (const auto &node_name : topo_order)
    {
        UpdateEquationInternal(node_name);
    }
}

void EquationManager::UpdateMultipleEquations(const std::string &eqn_code)
{
    if (multiple_variable_equation_code_set_.count(eqn_code) == 0)
    {
        throw std::runtime_error("statement '" + eqn_code + "' is not exist");
    }

    ParseResult res = parse_callback_(eqn_code);
    std::vector<std::string> eqn_list;
    for (const auto &item : res)
    {
        eqn_list.push_back(item.name);
    }

    auto topo_order = graph_->TopologicalSort(eqn_list);

    for (const auto &node_name : topo_order)
    {
        UpdateEquationInternal(node_name);
    }
}