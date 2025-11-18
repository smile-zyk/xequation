#include "equation_manager.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/equation_signals_manager.h"
#include "dependency_graph.h"
#include "equation.h"
#include "equation_common.h"
#include "equation_group.h"
#include "event_stamp.h"

namespace xequation
{
EquationManager::EquationManager(
    std::unique_ptr<EquationContext> context, ExecHandler eval_callback, ParseHandler parse_callback,
    EvalHandler exec_handler
) noexcept
    : graph_(std::unique_ptr<DependencyGraph>(new DependencyGraph())),
      context_(std::move(context)),
      exec_handler_(eval_callback),
      parse_handler_(parse_callback),
      eval_handler_(exec_handler)
{
}

bool EquationManager::IsEquationGroupExist(const EquationGroupId &group_id) const
{
    return equation_group_map_.contains(group_id);
}

bool EquationManager::IsEquationExist(const std::string &equation_name) const
{
    bool is_name_exist = equation_name_to_group_id_map_.count(equation_name) != 0;
    const EquationGroupId &id = equation_name_to_group_id_map_.at(equation_name);
    bool is_group_exist = equation_group_map_.contains(id);
    const EquationGroup *group = equation_group_map_.at(id).get();
    bool is_equation_exist = group->IsEquationExist(equation_name);

    return is_name_exist && is_group_exist && is_equation_exist;
}

const EquationGroup *EquationManager::GetEquationGroup(const EquationGroupId &group_id) const
{
    if (equation_group_map_.contains(group_id))
    {
        return equation_group_map_.at(group_id).get();
    }
    return nullptr;
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

EquationGroupId EquationManager::AddEquationGroup(const std::string &equation_statement)
{
    auto res = parse_handler_(equation_statement);

    for (const auto &item : res)
    {
        if (IsEquationExist(item.name))
        {
            throw EquationException::EquationAlreadyExists(item.name);
        }
    }

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    for (const auto &item : res)
    {
        AddNodeToGraph(item.name, item.dependencies);
    }
    guard.commit();

    EquationGroupPtr group = EquationGroup::Create(this);
    group->set_statement(equation_statement);
    const EquationGroupId &id = group->id();
    for (const auto &item : res)
    {
        graph_->InvalidateNode(item.name);
        EquationPtr equation = Equation::Create(item, id, this);
        group->AddEquation(std::move(equation));
        equation_name_to_group_id_map_.insert({item.name, id});
    }
    equation_group_map_.insert({id, std::move(group)});
    NotifyEquationGroupAdded(id);
    return id;
}

void EquationManager::EditEquationGroup(const EquationGroupId &group_id, const std::string &equation_statement)
{
    if (IsEquationGroupExist(group_id) == false)
    {
        throw EquationException::EquationGroupNotFound(group_id);
    }

    EquationGroup *group = GetEquationGroupInternal(group_id);

    std::vector<std::string> old_equation_names = group->GetEquationNames();

    ParseResult new_res = parse_handler_(equation_statement);

    std::unordered_map<std::string, ParseResultItem> new_name_map;
    for (const auto &item : new_res)
    {
        new_name_map.insert({item.name, item});
    }

    for (const auto &new_eqn : new_name_map)
    {
        std::string new_eqn_name = new_eqn.first;
        if (std::find(old_equation_names.begin(), old_equation_names.end(), new_eqn_name) == old_equation_names.end() &&
            IsEquationExist(new_eqn_name))
        {
            throw EquationException::EquationAlreadyExists(new_eqn_name);
        }
    }

    std::vector<std::string> to_remove_names;
    std::vector<ParseResultItem> to_add_items;
    std::vector<std::pair<std::string, ParseResultItem>> to_update_items; // pair<old_name, new_item>

    for (const auto &old_eqn_name : old_equation_names)
    {
        auto new_it = new_name_map.find(old_eqn_name);
        if (new_it == new_name_map.end())
        {
            to_remove_names.push_back(old_eqn_namem);
        }
        else if (group->GetEquation(old_eqn_name)->content() != )
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

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
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
        const auto &old_item = entry.first;
        const auto &new_item = entry.second;
        RemoveNodeInGraph(old_item.name);
        AddNodeToGraph(new_item.name, new_item.dependencies);
    }

    guard.commit();

    for (const auto &item : to_remove)
    {
        signals_manager_->emit<EquationEvent::kEquationRemoving>(GetEquation(item.name));
        if (new_name_map.find(item.name) == new_name_map.end())
        {
            auto range = graph_->GetEdgesByTo(item.name);
            for (auto it = range.first; it != range.second; it++)
            {
                graph_->InvalidateNode(it->from());
            }
        }
        RemoveValueInContext(item.name);
        group->RemoveEquation(item.name);
    }

    for (const auto &entry : to_update)
    {
        const auto &old_item = entry.first;
        const auto &new_item = entry.second;

        UpdateEquationPtr(new_item);
        RemoveValueInContext(old_item.name);
        graph_->InvalidateNode(new_item.name);
    }

    for (const auto &item : to_add)
    {
        graph_->InvalidateNode(item.name);
        equation_map_.insert({item.name, std::move(ConstructEquationPtr(item))});
        NotifyEquationAdded(item.name);
    }
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

std::string EquationManager::AddMultiEquations(const std::string &eqn_code)
{
    AddEquationStatement(eqn_code);
    multiple_variable_equation_code_set_.insert(eqn_code);
}

void EquationManager::EditMultiEquations(const std::string &old_eqn_code, const std::string &new_eqn_code)
{
    if (multiple_variable_equation_code_set_.count(old_eqn_code) == 0)
    {
        throw std::runtime_error("statement '" + old_eqn_code + "' is not exist");
    }
    EditEquationStatement(old_eqn_code, new_eqn_code);
    multiple_variable_equation_code_set_.erase(old_eqn_code);
    multiple_variable_equation_code_set_.insert(new_eqn_code);
}

void EquationManager::RemoveMultiEquations(const std::string &eqn_code)
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
    ParseResult res = parse_handler_(eqn_code);

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
        NotifyEquationAdded(item.name);
    }
}

void EquationManager::EditEquationStatement(const std::string &old_eqn_code, const std::string &new_eqn_code)
{
    ParseResult old_res = parse_handler_(old_eqn_code);
    ParseResult new_res = parse_handler_(new_eqn_code);

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
        const auto &old_item = entry.first;
        const auto &new_item = entry.second;
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
        RemoveValueInContext(item.name);
        NotifyEquationRemoving(item.name);
        equation_map_.erase(item.name);
    }

    for (const auto &entry : to_update)
    {
        const auto &old_item = entry.first;
        const auto &new_item = entry.second;

        UpdateEquationPtr(new_item);
        RemoveValueInContext(old_item.name);
        graph_->InvalidateNode(new_item.name);
    }

    for (const auto &item : to_add)
    {
        graph_->InvalidateNode(item.name);
        equation_map_.insert({item.name, std::move(ConstructEquationPtr(item))});
        NotifyEquationAdded(item.name);
    }
}

void EquationManager::RemoveEquationStatement(const std::string &eqn_code)
{
    ParseResult res = parse_handler_(eqn_code);

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
        RemoveValueInContext(item.name);
        NotifyEquationRemoving(item.name);
        equation_map_.erase(item.name);
    }
}

EvalResult EquationManager::Eval(const std::string &expression) const
{
    return eval_handler_(expression, context_.get());
}

void EquationManager::Reset()
{
    for (const auto &eqn_name : GetEquationNames())
    {
        NotifyEquationRemoving(eqn_name);
    }

    graph_->Reset();
    equation_map_.clear();
    context_->Clear();
    single_variable_equation_name_set_.clear();
    multiple_variable_equation_code_set_.clear();
    equation_callback_map_.clear();
    equation_add_callback_set_.clear();
    equation_remove_callback_set_.clear();
    next_callback_id = 0;
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
    ExecResult result = exec_handler_(eqn_code, context_.get());
    eval_success = (result.status == Equation::Status::kSuccess);
    if (eval_success)
    {
        UpdateValueToContext(eqn_name, origin_value);
    }
    else
    {
        RemoveValueInContext(eqn_name);
    }
    eqn->SetStatus(result.status);
    eqn->SetMessage(result.message);
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

void EquationManager::UpdateValueToContext(const std::string &equation_name, const Value &old_value)
{
    Value new_value = context_->Get(equation_name);
    if (new_value != old_value)
    {
        graph_->UpdateNodeEventStamp(equation_name);
        if (equation_map_.count(equation_name) != 0 && equation_map_.at(equation_name) != nullptr)
        {
            equation_map_.at(equation_name)->UpdateValue();
        }
    }
}

void EquationManager::RemoveValueInContext(const std::string &equation_name)
{
    if (context_->Remove(equation_name))
    {
        graph_->UpdateNodeEventStamp(equation_name);
        if (equation_map_.count(equation_name) != 0 && equation_map_.at(equation_name) != nullptr)
        {
            equation_map_.at(equation_name)->UpdateValue();
        }
    }
}

void EquationManager::NotifyEquationAdded(const std::string &equation_name) const
{
    for (auto callback_id : equation_add_callback_set_)
    {
        auto it = equation_callback_map_.find(callback_id);
        if (it != equation_callback_map_.end())
        {
            it->second(this, equation_name);
        }
    }
}

void EquationManager::NotifyEquationRemoving(const std::string &equation_name) const
{
    for (auto callback_id : equation_add_callback_set_)
    {
        auto it = equation_callback_map_.find(callback_id);
        if (it != equation_callback_map_.end())
        {
            it->second(this, equation_name);
        }
    }
}

std::unique_ptr<Equation> EquationManager::ConstructEquationPtr(const ParseResultItem &item)
{
    std::unique_ptr<Equation> equation = std::unique_ptr<Equation>(new Equation(item.name, this));
    equation->SetContent(item.content);
    equation->SetType(item.type);
    equation->SetDependencies(item.dependencies);
    return equation;
}

void EquationManager::UpdateEquationPtr(const ParseResultItem &item)
{
    if (equation_map_.count(item.name) == 0)
    {
        return;
    }

    Equation *eqn = GetEquationInteral(item.name);

    eqn->SetContent(item.content);
    eqn->SetType(item.type);
    eqn->SetDependencies(item.dependencies);
}

EquationManager::CallbackId EquationManager::RegisterEquationAddedCallback(EquationManager::EquationCallback callback)
{
    CallbackId cur_id = next_callback_id++;
    equation_callback_map_.insert({cur_id, callback});
    equation_add_callback_set_.insert(cur_id);
    return cur_id;
}

void EquationManager::UnRegisterEquationAddedCallback(EquationManager::CallbackId callback_id)
{
    equation_callback_map_.erase(callback_id);
    equation_add_callback_set_.erase(callback_id);
}

EquationManager::CallbackId EquationManager::RegisterEquationRemovingCallback(EquationManager::EquationCallback callback
)
{
    CallbackId cur_id = next_callback_id++;
    equation_callback_map_.insert({cur_id, callback});
    equation_remove_callback_set_.insert(cur_id);
    return cur_id;
}

void EquationManager::UnRegisterEquationRemovingCallback(EquationManager::CallbackId callback_id)
{
    equation_callback_map_.erase(callback_id);
    equation_remove_callback_set_.erase(callback_id);
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

    ParseResult res = parse_handler_(eqn_code);
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
} // namespace xequation