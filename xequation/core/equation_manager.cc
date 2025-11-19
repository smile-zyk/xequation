#include "equation_manager.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/equation_signals_manager.h"
#include "dependency_graph.h"
#include "equation.h"
#include "equation_common.h"
#include "equation_group.h"

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
        AddEquationToGroup(group.get(), std::move(equation));
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

    if(group->statement() == equation_statement)
    {
        return;
    }

    const EquationPtrOrderedMap& old_name_equation_map = group->equation_map();

    ParseResult new_items = parse_handler_(equation_statement);
    std::unordered_map<std::string, ParseResultItem> new_name_item_map;
    for (const auto &item : new_items)
    {
        new_name_item_map.insert({item.name, item});
    }

    for (const auto &new_item : new_items)
    {
        if (group->IsEquationExist(new_item.name) == false && IsEquationExist(new_item.name))
        {
            throw EquationException::EquationAlreadyExists(new_item.name);
        }
    }

    std::vector<std::string> to_remove_equation_names;
    std::vector<ParseResultItem> to_add_items;
    std::vector<ParseResultItem> to_update_items;

    for (const auto &old_eqn_entry : old_name_equation_map)
    {
        std::string old_eqn_name = old_eqn_entry.first;
        auto new_item_it = new_name_item_map.find(old_eqn_name);
        if (new_item_it == new_name_item_map.end())
        {
            to_remove_equation_names.push_back(old_eqn_name);
        }
        else if (group->GetEquation(old_eqn_name)->content() != new_item_it->second.content)
        {
            to_update_items.push_back(new_item_it->second);
        }
    }

    for (const auto &new_item : new_items)
    {
        if (group->IsEquationExist(new_item.name) == false)
        {
            to_add_items.push_back(new_item);
        }
    }

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    for (const auto &equation_name : to_remove_equation_names)
    {
        RemoveNodeInGraph(equation_name);
    }

    for (const auto &item : to_add_items)
    {
        AddNodeToGraph(item.name, item.dependencies);
    }

    for (const auto &item : to_update_items)
    {
        RemoveNodeInGraph(item.name);
        AddNodeToGraph(item.name, item.dependencies);
    }

    guard.commit();

    for (const auto &remove_eqn_name : to_remove_equation_names)
    {
        signals_manager_->emit<EquationEvent::kEquationRemoving>(group->GetEquation(remove_eqn_name));
        auto range = graph_->GetEdgesByTo(remove_eqn_name);
        for (auto it = range.first; it != range.second; it++)
        {
            graph_->InvalidateNode(it->from());
        }
        RemoveEquationInGroup(group, remove_eqn_name);
        context_->Remove(remove_eqn_name);
    }

    for (const auto &update_item : to_update_items)
    {
        graph_->InvalidateNode(update_item.name);
        Equation* update_eqn = group->GetEquation(update_item.name);
        update_eqn->set_content(update_item.content);
        update_eqn->set_type(update_item.type);
        update_eqn->set_dependencies(update_item.dependencies);
        context_->Remove(update_item.name);
        signals_manager_->emit<EquationEvent::kEquationUpdate>(update_eqn, EquationField::kContent | EquationField::kType | EquationField::kDependencies);
    }

    for (const auto &add_item : to_add_items)
    {
        graph_->InvalidateNode(add_item.name);
        EquationPtr equation = Equation::Create(add_item, group->id(), this);
        AddEquationToGroup(group, std::move(equation));
        signals_manager_->emit<EquationEvent::kEquationAdded>(group->GetEquation(add_item.name));
    }

    if(to_add_items.size() != 0 || to_remove_equation_names.size() != 0)
    {
        signals_manager_->emit<EquationEvent::kEquationGroupUpdate>(group, EquationGroupField::kEquationCount | EquationGroupField::kStatement);
    }
    else 
    {
        signals_manager_->emit<EquationEvent::kEquationGroupUpdate>(group, EquationGroupField::kStatement);
    }
}

void EquationManager::RemoveEquationGroup(const EquationGroupId &group_id)
{
    if (IsEquationGroupExist(group_id) == false)
    {
        throw EquationException::EquationGroupNotFound(group_id);
    }

    NotifyEquationGroupRemoving(group_id);

    EquationGroup* group = GetEquationGroupInternal(group_id);
    auto group_equation_names = group->GetEquationNames();

    for (const std::string& equation_name : group_equation_names)
    {
        RemoveNodeInGraph(equation_name);
        graph_->InvalidateNode(equation_name);
        RemoveEquationInGroup(group, equation_name);
        context_->Remove(equation_name);
    }
    equation_group_map_.erase(group_id);
}

EvalResult EquationManager::Eval(const std::string &expression) const
{
    return eval_handler_(expression, context_.get());
}

void EquationManager::Reset()
{
    for (const auto &equation_group_entry : equation_group_map_)
    {
        NotifyEquationGroupRemoving(equation_group_entry.first);
    }

    graph_->Reset();
    equation_group_map_.clear();
    equation_name_to_group_id_map_.clear();
    context_->Clear();
    signals_manager_->disconnect_all_event();
}

void EquationManager::UpdateEquationInternal(const std::string &equation_name)
{
    if (IsEquationExist(equation_name) == false)
    {
        throw EquationException::EquationNotFound(equation_name);
    }

    const DependencyGraph::Node *node = graph_->GetNode(equation_name);
    Equation *equation = GetEquationInternal(equation_name);

    if (!node->dirty_flag())
    {
        return;
    }

    const std::string &equation_statement = equation->content();
    ExecResult result = exec_handler_(equation_statement, context_.get());
    equation->set_status(result.status);
    equation->set_message(result.message);
    signals_manager_->emit<EquationEvent::kEquationUpdate>(equation, EquationField::kStatus | EquationField::kMessage | EquationField::kValue);
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

void EquationManager::NotifyEquationGroupAdded(const EquationGroupId& group_id) const
{
    const EquationGroup* group = GetEquationGroup(group_id);
    signals_manager_->emit<EquationEvent::kEquationGroupAdded>(group);
    
    std::vector<std::string> equation_names = group->GetEquationNames();
    for(const std::string& equation_name : equation_names)
    {
        signals_manager_->emit<EquationEvent::kEquationAdded>(group->GetEquation(equation_name));
    }
}

void EquationManager::NotifyEquationGroupRemoving(const EquationGroupId& group_id) const
{
    const EquationGroup* group = GetEquationGroup(group_id);
    signals_manager_->emit<EquationEvent::kEquationGroupRemoving>(group);
    
    std::vector<std::string> equation_names = group->GetEquationNames();
    for(const std::string& equation_name : equation_names)
    {
        signals_manager_->emit<EquationEvent::kEquationRemoving>(group->GetEquation(equation_name));
    }
}

void EquationManager::Update()
{
    graph_->Traversal([&](const std::string &eqn_name) { UpdateEquationInternal(eqn_name); });
}

void EquationManager::UpdateEquation(const std::string &equation_name)
{
    if(IsEquationExist(equation_name) == false)
    {
        throw EquationException::EquationNotFound(equation_name);
    }

    auto topo_order = graph_->TopologicalSort(equation_name);

    for (const auto &node_name : topo_order)
    {
        UpdateEquationInternal(node_name);
    }
}

void EquationManager::UpdateEquationGroup(const EquationGroupId &group_id)
{
    if(IsEquationGroupExist(group_id) == false)
    {
        throw EquationException::EquationGroupNotFound(group_id);
    }

    const EquationGroup* group = GetEquationGroup(group_id);

    auto topo_order = graph_->TopologicalSort(group->GetEquationNames());

    for (const auto &node_name : topo_order)
    {
        UpdateEquationInternal(node_name);
    }
}
} // namespace xequation