#include "variable_manager.h"
#include "core/expr_common.h"
#include "core/variable.h"
#include "event_stamp.h"
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

using namespace xexprengine;

VariableManager::VariableManager(
    std::unique_ptr<ExprContext> context, EvalCallback eval_callback, ParseCallback parse_callback, ImportCallback import_callback
) noexcept
    : graph_(std::unique_ptr<DependencyGraph>(new DependencyGraph())),
      context_(std::move(context)),
      evaluate_callback_(eval_callback),
      parse_callback_(parse_callback)
{
}

const Variable *VariableManager::GetVariable(const std::string &var_name) const
{
    if (IsVariableExist(var_name) == true)
    {
        return variable_map_.at(var_name).get();
    }
    return nullptr;
}

Variable *VariableManager::GetVariable(const std::string &var_name)
{
    if (IsVariableExist(var_name) == true)
    {
        return variable_map_.at(var_name).get();
    }
    return nullptr;
}

bool VariableManager::AddVariable(std::unique_ptr<Variable> var)
{
    std::string var_name = var->name();
    if (IsVariableExist(var_name) == false)
    {
        // update graph
        try
        {
            AddVariableToGraph(var.get());
        }
        catch (xexprengine::DependencyCycleException e)
        {
            throw;
        }

        // for invalidate graph node to trigger dependent node update
        graph_->InvalidateNode(var_name);
        // update variable_map
        UpdateVariableStatus(var.get());
        variable_map_.insert({var_name, std::move(var)});
        return true;
    }
    return false;
}

bool VariableManager::AddVariables(std::vector<std::unique_ptr<Variable>> var_list)
{
    bool res = true;
    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    std::unordered_set<std::string> should_insert_variables;
    for (std::unique_ptr<Variable> &var : var_list)
    {
        bool add_res = AddVariableToGraph(var.get());
        res &= add_res;
        if (add_res)
        {
            should_insert_variables.insert(var->name());
        }
    }
    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        throw;
    }

    for (std::unique_ptr<Variable> &var : var_list)
    {
        std::string var_name = var->name();
        if (should_insert_variables.count(var->name()))
        {
            graph_->InvalidateNode(var_name);
            UpdateVariableStatus(var.get());
            variable_map_.insert({var_name, std::move(var)});
        }
    }
    return res;
}

void VariableManager::SetValue(const std::string &var_name, const Value &value)
{
    auto var = VariableFactory::CreateRawVariable(var_name, value);
    SetVariable(var_name, std::move(var));
}

void VariableManager::SetExpression(const std::string &var_name, const std::string &expression)
{
    auto var = VariableFactory::CreateExprVariable(var_name, expression);
    SetVariable(var_name, std::move(var));
}

bool VariableManager::SetVariable(const std::string &var_name, std::unique_ptr<Variable> variable)
{
    if (variable->name() != var_name)
    {
        return false;
    }

    // update graph
    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    bool is_variable_exist = IsVariableExist(var_name);
    if (is_variable_exist == true)
    {
        RemoveVariableToGraph(var_name);
    }
    AddVariableToGraph(variable.get());
    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        throw;
    }

    if (is_variable_exist)
    {
        // remove old variable
        variable_map_.erase(var_name);
    }

    graph_->InvalidateNode(var_name);
    // add variable to map
    UpdateVariableStatus(variable.get());
    variable_map_.insert({var_name, std::move(variable)});

    return true;
}

bool VariableManager::ImportDirectModule(const std::string& module_name)
{
    ModuleInfo info;
    info.name = module_name;
    info.is_import_to_global = false;
    info.type = ModuleType::kDirect;
    auto module_var = VariableFactory::CreateModuleVariable(info.name, info);
    return AddVariable(std::move(module_var));
}

bool VariableManager::ImportCustomModule(const std::string& module_path)
{
    boost::filesystem::path p(module_path);
    if(boost::filesystem::exists(p) == false)
    {
        return false;
    }
    ModuleInfo info;
    info.name = p.filename().string();
    info.path = module_path;
    info.is_import_to_global = false;
    info.type = ModuleType::kPath;
    auto module_var = VariableFactory::CreateModuleVariable(info.name, info);
    return AddVariable(std::move(module_var));
}

bool VariableManager::SetModule(const ModuleInfo& module_info)
{
    
}

bool VariableManager::RemoveVariable(const std::string &var_name) noexcept
{
    if (IsVariableExist(var_name) == true)
    {
        graph_->InvalidateNode(var_name);

        // update graph
        RemoveVariableToGraph(var_name);

        // update variable map
        variable_map_.erase(var_name);

        return true;
    }
    return false;
}

bool VariableManager::RemoveVariables(const std::vector<std::string> &var_name_list) noexcept
{
    bool res = true;
    for (const std::string &var_name : var_name_list)
    {
        res &= RemoveVariable(var_name);
    }
    return res;
}

bool VariableManager::RenameVariable(const std::string &old_name, const std::string &new_name)
{
    if (IsVariableExist(old_name) != true || IsVariableExist(new_name) != false)
    {
        return false;
    }

    // store old node dependents
    auto old_dependents = graph_->GetNode(old_name)->dependents();
    // process graph
    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    graph_->RemoveNode(old_name);
    graph_->AddNode(new_name);
    auto old_edge_iterator = graph_->GetEdgesByFrom(old_name);
    std::vector<DependencyGraph::Edge> old_edges;
    std::vector<DependencyGraph::Edge> new_edges;
    for (auto it = old_edge_iterator.first; it != old_edge_iterator.second; it++)
    {
        old_edges.push_back(*it);
        new_edges.push_back({new_name, it->to()});
    }
    graph_->RemoveEdges(old_edges);
    graph_->AddEdges(new_edges);
    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        throw;
    }

    // for invalidate graph node to trigger dependent node update
    for (const std::string &old_dependent : old_dependents)
    {
        graph_->InvalidateNode(old_dependent);
    }
    graph_->InvalidateNode(new_name);
    // process variable map
    std::unique_ptr<Variable> origin_var = std::move(variable_map_[old_name]);
    variable_map_.erase(old_name);
    origin_var->set_name(new_name);
    variable_map_.insert({new_name, std::move(origin_var)});

    return true;
}

bool VariableManager::CheckNodeDependenciesComplete(
    const std::string &node_name, std::vector<std::string> &missing_dependencies
) const
{
    if (graph_->IsNodeExist(node_name) == false)
    {
        return false;
    }
    const DependencyGraph::Node *node = graph_->GetNode(node_name);
    DependencyGraph::EdgeContainer::RangeByFrom edges = graph_->GetEdgesByFrom(node_name);
    const auto &dependencies = node->dependencies();
    if (dependencies.size() == std::distance(edges.first, edges.second))
    {
        return true;
    }
    for (auto it = edges.first; it != edges.second; it++)
    {
        if (dependencies.count(it->to()) == 0)
        {
            missing_dependencies.push_back(it->to());
        }
    }
    return false;
}

bool VariableManager::UpdateNodeDependencies(
    const std::string &node_name, const std::unordered_set<std::string> &node_dependencies
)
{
    if (graph_->IsNodeExist(node_name) == false)
    {
        return false;
    }
    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    const DependencyGraph::Node *node = graph_->GetNode(node_name);
    const DependencyGraph::EdgeContainer::RangeByFrom edges = graph_->GetEdgesByFrom(node_name);
    std::vector<DependencyGraph::Edge> edges_to_remove;
    for (auto it = edges.first; it != edges.second; it++)
    {
        edges_to_remove.push_back(*it);
    }
    graph_->RemoveEdges(edges_to_remove);

    for (const std::string &dep : node_dependencies)
    {
        graph_->AddEdge({node_name, dep});
    }

    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        throw;
    }

    return true;
}

void VariableManager::RemoveObsoleteVariablesInContext()
{
    std::unordered_set<std::string> context_value_to_remove = context_->keys();

    for (const auto &entry : variable_map_)
    {
        context_value_to_remove.erase(entry.first);
    }

    for (const auto &entry : context_value_to_remove)
    {
        context_->Remove(entry);
    }
}

bool VariableManager::IsVariableExist(const std::string &var_name) const
{
    return graph_->IsNodeExist(var_name) && variable_map_.count(var_name);
}

void VariableManager::Reset()
{
    graph_->Reset();
    variable_map_.clear();
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

bool VariableManager::UpdateVariableInternal(const std::string &var_name)
{
    if (!IsVariableExist(var_name) || evaluate_callback_ == nullptr)
    {
        return false;
    }

    DependencyGraph::Node *node = graph_->GetNode(var_name);
    Variable *var = GetVariable(var_name);

    if (!node->dirty_flag())
    {
        node->set_dirty_flag(false);
        return false;
    }

    auto RemoveVariableInContext = [this](const std::string &var_name) {
        if (context_->Remove(var_name))
        {
            graph_->UpdateNodeEventStamp(var_name);
        }
    };

    auto UpdateValueToContext = [this](const std::string &var_name, const Value &value) {
        if (value != context_->Get(var_name))
        {
            context_->Set(var_name, value);
            graph_->UpdateNodeEventStamp(var_name);
        }
    };

    bool needs_evaluation = true;

    if (var->status() == VariableStatus::kParseSyntaxError)
    {
        needs_evaluation = false;
        RemoveVariableInContext(var_name);
    }
    else
    {
        std::vector<std::string> missing_deps;
        if (!CheckNodeDependenciesComplete(var_name, missing_deps))
        {
            needs_evaluation = false;
            var->set_status(VariableStatus::kMissingDependency);
            var->set_error_message(BuildMissingDepsMessage(missing_deps));
            RemoveVariableInContext(var_name);
        }
        else
        {
            // Check if dependencies have newer event stamps
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
    }

    if (!needs_evaluation)
    {
        node->set_dirty_flag(false);
        return false;
    }

    bool eval_success = true;

    if (var->GetType() == Variable::Type::Expr)
    {
        const std::string &expression = var->As<ExprVariable>()->expression();
        EvalResult result = evaluate_callback_(expression, context_.get());
        eval_success = (result.status == VariableStatus::kExprEvalSuccess);
        if (eval_success)
        {
            UpdateValueToContext(var_name, result.value);
        }
        else
        {
            RemoveVariableInContext(var_name);
        }
        var->set_status(result.status);
        var->set_error_message(result.eval_error_message);
    }
    else if (var->GetType() == Variable::Type::Raw)
    {
        const Value &value = var->As<RawVariable>()->value();
        UpdateValueToContext(var_name, value);
        var->set_status(VariableStatus::kRawVar);
        var->set_error_message("");
    }
    node->set_dirty_flag(false);
    return eval_success;
}

void VariableManager::UpdateVariableStatus(Variable *var)
{
    if (var->GetType() == Variable::Type::Expr)
    {
        const ExprVariable *expr_var = var->As<ExprVariable>();
        const std::string &expression = expr_var->expression();
        ParseResult parse_result = parse_callback_(expression);
        var->set_status(parse_result.status);
        var->set_error_message(parse_result.parse_error_message);
    }
}

bool VariableManager::AddVariableToGraph(const Variable *var)
{
    const std::string &var_name = var->name();
    if (graph_->IsNodeExist(var_name))
    {
        return false;
    }

    DependencyGraph::BatchUpdateGuard guard(graph_.get());
    graph_->AddNode(var_name);
    if (var->GetType() == Variable::Type::Expr)
    {
        const ExprVariable *expr_var = var->As<ExprVariable>();
        const std::string &expression = expr_var->expression();
        ParseResult parse_result = parse_callback_(expression);
        UpdateNodeDependencies(var_name, parse_result.variables);
    }

    try
    {
        guard.commit();
    }
    catch (xexprengine::DependencyCycleException e)
    {
        throw;
    }
    return true;
}

bool VariableManager::RemoveVariableToGraph(const std::string &var_name) noexcept
{
    if (graph_->IsNodeExist(var_name) == false)
    {
        return false;
    }
    // for invalidate graph node to trigger dependent node update
    graph_->InvalidateNode(var_name);
    graph_->RemoveNode(var_name);
    auto edges = graph_->GetEdgesByFrom(var_name);
    std::vector<DependencyGraph::Edge> edges_to_remove;
    for (auto it = edges.first; it != edges.second; it++)
    {
        edges_to_remove.push_back(*it);
    }
    graph_->RemoveEdges(edges_to_remove);
    return true;
}

void VariableManager::Update()
{
    graph_->Traversal([&](const std::string &var_name) { UpdateVariableInternal(var_name); });

    RemoveObsoleteVariablesInContext();
}

bool VariableManager::UpdateVariable(const std::string &var_name)
{
    if (IsVariableExist(var_name) == false)
    {
        return false;
    }

    auto topo_order = graph_->TopologicalSort(var_name);

    for (const auto &node_name : topo_order)
    {
        UpdateVariableInternal(node_name);
    }

    RemoveObsoleteVariablesInContext();
    
    return true;
}