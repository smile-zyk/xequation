#include "equation_manager.h"
#include "equation_common.h"
#include "core/equation_signals_manager.h"

namespace xequation
{
EquationManager::EquationManager(
    std::unique_ptr<EquationContext> context, InterpretHandler interpret_handler, ParseHandler parse_handler, const std::string &language
) noexcept
    : graph_(std::unique_ptr<DependencyGraph>(new DependencyGraph())),
      signals_manager_(std::unique_ptr<EquationSignalsManager>(new EquationSignalsManager())),
      context_(std::move(context)),
      interpret_handler_(interpret_handler),
      parse_handler_(parse_handler),
      language_(language)
{
}

bool EquationManager::IsEquationGroupExist(const EquationGroupId &group_id) const
{
    return equation_group_map_.contains(group_id);
}

bool EquationManager::IsEquationExist(const std::string &equation_name) const
{
    bool is_name_exist = equation_name_to_group_id_map_.count(equation_name) != 0;
    if (!is_name_exist)
    {
        return false;
    }

    const EquationGroupId &id = equation_name_to_group_id_map_.at(equation_name);
    bool is_group_exist = equation_group_map_.contains(id);
    if (!is_group_exist)
    {
        return false;
    }

    const EquationGroup *group = equation_group_map_.at(id).get();
    bool is_equation_exist = group->IsEquationExist(equation_name);
    return is_equation_exist;
}

bool EquationManager::IsStatementSingleEquation(const std::string &equation_statement) const
{
    try
    {
        auto res = parse_handler_(equation_statement, ParseMode::kStatement);
        return res.items.size() == 1;
    }
    catch (const ParseException &e)
    {
        // If parsing fails, we consider it a single equation.
        return true;
    }
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

std::vector<EquationGroupId> EquationManager::GetEquationGroupIds() const
{
    std::vector<EquationGroupId> result;
    for (const auto &entry : equation_group_map_)
    {
        result.push_back(entry.first);
    }
    return result;
}

std::vector<std::string> EquationManager::GetEquationNames() const
{
    std::vector<std::string> result;
    for( const auto &entry : equation_group_map_)
    {
        const EquationGroup *group = entry.second.get();
        auto equation_names = group->GetEquationNames();
        result.insert(result.end(), equation_names.begin(), equation_names.end());
    }
    return result;
}

const tsl::ordered_set<std::string> &EquationManager::GetExternalVariableNames() const
{
    return external_variable_names_;
}

EquationGroupId EquationManager::AddEquationGroup(const std::string &equation_statement)
{
    auto res = parse_handler_(equation_statement, ParseMode::kStatement);

    for (const auto &item : res.items)
    {
        if (IsEquationExist(item.name))
        {
            throw EquationException::EquationAlreadyExists(item.name);
        }
    }

    std::vector<std::string> dependency_updated_equation;
    ScopedConnection dependency_connection = ConnectGraphDependencyUpdated(dependency_updated_equation);

    std::vector<std::string> dependent_updated_equation;
    ScopedConnection dependent_connection = ConnectGraphDependentUpdated(dependent_updated_equation);

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    for (const auto &item : res.items)
    {
        AddNodeToGraph(item.name, item.dependencies);
    }
    guard.commit();

    EquationGroupPtr group = EquationGroup::Create(this);
    group->set_statement(equation_statement);
    const EquationGroupId &id = group->id();
    auto group_ptr = group.get();
    equation_group_map_.insert({id, std::move(group)});
    for (const auto &item : res.items)
    {
        graph_->InvalidateNode(item.name);
        EquationPtr equation = Equation::Create(item, id, this);
        AddEquationToGroup(group_ptr, std::move(equation));
        signals_manager_->Emit<EquationEvent::kEquationAdded>(group_ptr->GetEquation(item.name));
    }
    signals_manager_->Emit<EquationEvent::kEquationGroupAdded>(group_ptr);

    for (const auto &equation_name : dependency_updated_equation)
    {
        NotifyEquationDependenciesUpdated(equation_name);
    }

    for (const auto &equation_name : dependent_updated_equation)
    {
        NotifyEquationDependentsUpdated(equation_name);
    }

    return id;
}

void EquationManager::EditEquationGroup(const EquationGroupId &group_id, const std::string &equation_statement)
{
    if (IsEquationGroupExist(group_id) == false)
    {
        throw EquationException::EquationGroupNotFound(group_id);
    }

    EquationGroup *group = GetEquationGroupInternal(group_id);

    if (group->statement() == equation_statement)
    {
        return;
    }

    const EquationPtrOrderedMap &old_name_equation_map = group->equation_map();

    ParseResult new_result = parse_handler_(equation_statement, ParseMode::kStatement);
    std::unordered_map<std::string, ParseResultItem> new_name_item_map;
    for (const auto &item : new_result.items)
    {
        new_name_item_map.insert({item.name, item});
    }

    for (const auto &new_item : new_result.items)
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

    for (const auto &new_item : new_result.items)
    {
        if (group->IsEquationExist(new_item.name) == false)
        {
            to_add_items.push_back(new_item);
        }
    }

    std::vector<std::string> dependency_updated_equation;
    ScopedConnection dependency_connection = ConnectGraphDependencyUpdated(dependency_updated_equation);

    std::vector<std::string> dependent_updated_equation;
    ScopedConnection dependent_connection = ConnectGraphDependentUpdated(dependent_updated_equation);

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
        auto range = graph_->GetEdgesByTo(remove_eqn_name);
        for (auto it = range.first; it != range.second; it++)
        {
            graph_->InvalidateNode(it->from());
        }
        signals_manager_->Emit<EquationEvent::kEquationRemoving>(group->GetEquation(remove_eqn_name));
        RemoveEquationInGroup(group, remove_eqn_name);
        context_->Remove(remove_eqn_name);
        signals_manager_->Emit<EquationEvent::kEquationRemoved>(remove_eqn_name);
    }

    for (const auto &update_item : to_update_items)
    {
        graph_->InvalidateNode(update_item.name);
        Equation *update_eqn = group->GetEquation(update_item.name);
        update_eqn->set_content(update_item.content);
        update_eqn->set_type(update_item.type);
        context_->Remove(update_item.name);
        signals_manager_->Emit<EquationEvent::kEquationUpdated>(
            update_eqn, EquationUpdateFlag::kContent | EquationUpdateFlag::kType
        );
    }

    for (const auto &add_item : to_add_items)
    {
        graph_->InvalidateNode(add_item.name);
        EquationPtr equation = Equation::Create(add_item, group->id(), this);
        AddEquationToGroup(group, std::move(equation));
        signals_manager_->Emit<EquationEvent::kEquationAdded>(group->GetEquation(add_item.name));
    }

    group->set_statement(equation_statement);

    if (to_add_items.size() != 0 || to_remove_equation_names.size() != 0)
    {
        signals_manager_->Emit<EquationEvent::kEquationGroupUpdated>(
            group, EquationGroupUpdateFlag::kEquationCount | EquationGroupUpdateFlag::kStatement
        );
    }
    else
    {
        signals_manager_->Emit<EquationEvent::kEquationGroupUpdated>(group, EquationGroupUpdateFlag::kStatement);
    }

    for (const auto &equation_name : dependency_updated_equation)
    {
        NotifyEquationDependenciesUpdated(equation_name);
    }

    for (const auto &equation_name : dependent_updated_equation)
    {
        NotifyEquationDependentsUpdated(equation_name);
    }
}

void EquationManager::RemoveEquationGroup(const EquationGroupId &group_id)
{
    if (IsEquationGroupExist(group_id) == false)
    {
        throw EquationException::EquationGroupNotFound(group_id);
    }

    EquationGroup *group = GetEquationGroupInternal(group_id);
    auto group_equation_names = group->GetEquationNames();

    std::vector<std::string> dependency_updated_equation;
    ScopedConnection dependency_connection = ConnectGraphDependencyUpdated(dependency_updated_equation);

    std::vector<std::string> dependent_updated_equation;
    ScopedConnection dependent_connection = ConnectGraphDependentUpdated(dependent_updated_equation);

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    for (const auto &equation_name : group_equation_names)
    {
        RemoveNodeInGraph(equation_name);
    }
    guard.commit();

    signals_manager_->Emit<EquationEvent::kEquationGroupRemoving>(group);

    for (const std::string &equation_name : group_equation_names)
    {
        graph_->InvalidateNode(equation_name);
        signals_manager_->Emit<EquationEvent::kEquationRemoving>(group->GetEquation(equation_name));
        RemoveEquationInGroup(group, equation_name);
        context_->Remove(equation_name);
        signals_manager_->Emit<EquationEvent::kEquationRemoved>(equation_name);
    }
    equation_group_map_.erase(group_id);

    for (const auto &equation_name : dependency_updated_equation)
    {
        NotifyEquationDependenciesUpdated(equation_name);
    }

    for (const auto &equation_name : dependent_updated_equation)
    {
        NotifyEquationDependentsUpdated(equation_name);
    }
}

void EquationManager::SetExternalVariable(const std::string &var_name, const Value &value)
{
    context_->Set(var_name, value);
    external_variable_names_.insert(var_name);
}

void EquationManager::RemoveExternalVariable(const std::string &var_name)
{
    context_->Remove(var_name);
    external_variable_names_.erase(var_name);
}

ParseResult EquationManager::Parse(const std::string &expression, ParseMode mode) const
{
    return parse_handler_(expression, mode);
}

InterpretResult EquationManager::Eval(const std::string &expression) const
{
    return interpret_handler_(expression, context_.get(), InterpretMode::kEval);
}

void EquationManager::Reset()
{
    graph_->Reset();

    for (const auto &equation_group_entry : equation_group_map_)
    {
        for( const auto &equation_entry : equation_group_entry.second->equation_map())
        {
            signals_manager_->Emit<EquationEvent::kEquationRemoving>(equation_entry.second.get());
        }
    }
    equation_group_map_.clear();
    equation_name_to_group_id_map_.clear();
    context_->Clear();
    for (const auto &equation_group_entry : equation_group_map_)
    {
        for( const auto &equation_entry : equation_group_entry.second->equation_map())
        {
            signals_manager_->Emit<EquationEvent::kEquationRemoved>(equation_entry.first);
        }
    }
    signals_manager_->DisconnectAllEvent();
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

    const std::string &equation_statement = equation->type() == ItemType::kVariable
                                                ? equation->name() + " = " + equation->content()
                                                : equation->content();
    InterpretResult result = interpret_handler_(equation_statement, context_.get(), InterpretMode::kExec);
    equation->set_status(result.status);
    equation->set_message(result.message);
    if (equation->status() != ResultStatus::kSuccess)
    {
        context_->Remove(equation_name);
    }
    signals_manager_->Emit<EquationEvent::kEquationUpdated>(
        equation, EquationUpdateFlag::kStatus | EquationUpdateFlag::kMessage | EquationUpdateFlag::kValue
    );
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

void EquationManager::AddEquationToGroup(EquationGroup *group, EquationPtr equation)
{
    equation_name_to_group_id_map_.insert({equation->name(), equation->group_id()});
    group->AddEquation(std::move(equation));
}

void EquationManager::RemoveEquationInGroup(EquationGroup *group, const std::string &equation_name)
{
    equation_name_to_group_id_map_.erase(equation_name);
    group->RemoveEquation(equation_name);
}

void EquationManager::Update()
{
    graph_->Traversal([&](const std::string &eqn_name) { UpdateEquationInternal(eqn_name); });
}

void EquationManager::UpdateEquation(const std::string &equation_name)
{
    if (IsEquationExist(equation_name) == false)
    {
        throw EquationException::EquationNotFound(equation_name);
    }

    auto topo_order = graph_->TopologicalSort(equation_name);

    for (const auto &node_name : topo_order)
    {
        UpdateEquationInternal(node_name);
    }
}

void EquationManager::UpdateSingleEquation(const std::string &equation_name)
{
    if (IsEquationExist(equation_name) == false)
    {
        throw EquationException::EquationNotFound(equation_name);
    }

    UpdateEquationInternal(equation_name);
}

void EquationManager::UpdateEquationGroup(const EquationGroupId &group_id)
{
    if (IsEquationGroupExist(group_id) == false)
    {
        throw EquationException::EquationGroupNotFound(group_id);
    }

    const EquationGroup *group = GetEquationGroup(group_id);

    auto topo_order = graph_->TopologicalSort(group->GetEquationNames());

    for (const auto &node_name : topo_order)
    {
        UpdateEquationInternal(node_name);
    }
}

Equation *EquationManager::GetEquationInternal(const std::string &equation_name)
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

    EquationGroup *group = equation_group_map_.at(id).get();
    bool is_equation_exist = group->IsEquationExist(equation_name);
    if (!is_equation_exist)
    {
        return nullptr;
    }

    return group->GetEquation(equation_name);
}

EquationGroup *EquationManager::GetEquationGroupInternal(const EquationGroupId &group_id)
{
    if (equation_group_map_.contains(group_id))
    {
        return equation_group_map_.at(group_id).get();
    }
    return nullptr;
}

ScopedConnection EquationManager::ConnectGraphDependencyUpdated(std::vector<std::string> &dependency_updated_equation
) const
{
    return graph_->ConnectNodeDependencyChangedSignal([&](const std::string &node_name) {
        dependency_updated_equation.push_back(node_name);
    });
}

ScopedConnection EquationManager::ConnectGraphDependentUpdated(std::vector<std::string> &dependent_updated_equation
) const
{
    return graph_->ConnectNodeDependentChangedSignal([&](const std::string &node_name) {
        dependent_updated_equation.push_back(node_name);
    });
}

void EquationManager::NotifyEquationDependentsUpdated(const std::string &equation_name) const
{
    if (IsEquationExist(equation_name))
    {
        signals_manager_->Emit<EquationEvent::kEquationUpdated>(
            GetEquation(equation_name), EquationUpdateFlag::kDependents
        );
    }
}

void EquationManager::NotifyEquationDependenciesUpdated(const std::string &equation_name) const
{
    if (IsEquationExist(equation_name))
    {
        signals_manager_->Emit<EquationEvent::kEquationUpdated>(
            GetEquation(equation_name), EquationUpdateFlag::kDependencies
        );
    }
}

} // namespace xequation