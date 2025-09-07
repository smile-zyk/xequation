#include "expr_context.h"
#include "expr_common.h"
#include "expr_engine.h"
#include "variable.h"
#include "variable_dependency_graph.h"
#include "variable_factory.h"
#include <memory>
#include <utility>

using namespace xexprengine;

ExprContext::ExprContext(const std::string &name) : name_(name), graph_(new VariableDependencyGraph()) {}

Value ExprContext::GetValue(const Variable *var) const
{
    return engine_->GetVariable(var->name(), this);
}

Variable *ExprContext::GetVariable(const std::string &var_name) const
{
    return graph_->GetVariable(var_name);
}

bool ExprContext::AddVariable(std::unique_ptr<Variable> var)
{
    if (var == nullptr)
    {
        return false;
    }

    try
    {
        var->set_context(this);
        if (graph_->AddNode(std::move(var)))
        {
            return false;
        }

        auto parse_result = var->GetParseResult();
        std::vector<VariableDependencyGraph::Edge> edge_list;
        for (const std::string &dep_var_name : parse_result.variables)
        {
            edge_list.push_back({var->name(), dep_var_name});
        }

        return graph_->AddEdges(edge_list);
    }
    catch (const DependencyCycleException &e)
    {
        if (e.operation() == DependencyCycleException::Operation::kAddEdge)
        {
            graph_->RemoveNode(var->name());
        }
        throw;
    }
}

bool ExprContext::AddVariables(std::vector<std::unique_ptr<Variable>> var_list)
{
    if (std::any_of(var_list.begin(), var_list.end(), [](const std::unique_ptr<Variable> &ptr) {
            return ptr == nullptr;
        }) == true)
    {
        return false;
    }

    try
    {
        for (const auto &var : var_list)
        {
            var->set_context(this);
        }

        if (graph_->AddNodes(std::move(var_list)) == false)
        {
            return false;
        }

        std::vector<VariableDependencyGraph::Edge> edge_list;
        for (const auto &var : var_list)
        {
            auto analyze_result = var->GetParseResult();
            for (const std::string &dep_var_name : analyze_result.variables)
            {
                edge_list.push_back({var->name(), dep_var_name});
            }
        }
        return graph_->AddEdges(edge_list);
    }
    catch (const DependencyCycleException &e)
    {
        if (e.operation() == DependencyCycleException::Operation::kAddEdge)
        {
            std::vector<std::string> var_name_list;
            var_name_list.reserve(var_list.size());

            std::transform(
                var_list.begin(), var_list.end(), std::back_inserter(var_name_list),
                [](const std::unique_ptr<Variable> &var) { return var->name(); }
            );

            graph_->RemoveNodes(var_name_list);
        }
        throw;
    }
}

bool ExprContext::SetRawVariable(const std::string &var_name, const Value &value)
{
    auto var = VariableFactory::CreateRawVariable(var_name, value, this);
    if (IsVariableExist(var_name))
    {
        try
        {
            if (graph_->AddNode(std::move(var)))
            {
                return false;
            }
            return true;
        }
        catch (const VariableDependencyGraph &e)
        {
            throw;
        }
    }
    else
    {
        Variable *old_var = GetVariable(var_name);
        if (old_var && old_var->GetType() == Variable::Type::Expr)
        {
            graph_->ClearNodeDependencyEdges(var_name);
        }
        graph_->SetNode(var_name, std::move(var));
        return true;
    }
}

bool ExprContext::SetExprVariable(const std::string &var_name, const std::string &expression)
{
    auto var = VariableFactory::CreateExprVariable(var_name, expression, this);
    if (IsVariableExist(var_name))
    {
        try
        {
            if (graph_->AddNode(std::move(var)))
            {
                return false;
            }
            return true;
        }
        catch (const VariableDependencyGraph &e)
        {
            throw;
        }
    }
    else
    {
        Variable *old_var = GetVariable(var_name);
        if (old_var && old_var->GetType() == Variable::Type::Expr)
        {
            graph_->ClearNodeDependencyEdges(var_name);
        }
        return graph_->SetNode(var_name, std::move(var));
        std::vector<VariableDependencyGraph::Edge> edge_list;

        auto analyze_result = var->GetParseResult();
        for (const std::string &dep_var_name : analyze_result.variables)
        {
            edge_list.push_back({var->name(), dep_var_name});
        }
        try
        {
            graph_->AddEdges(edge_list);
            return true;
        }
        catch (const DependencyCycleException &e)
        {
            throw;
        }
    }
}

bool ExprContext::RemoveVariable(const std::string &var_name)
{
    if (graph_->RemoveNode(var_name))
    {
        engine_->RemoveVariable(var_name, this);
        return true;
    }
    return false;
}

bool ExprContext::RemoveVariable(Variable *var)
{
    if (var == nullptr)
        return false;
    return RemoveVariable(var->name());
}

bool ExprContext::RemoveVariables(const std::vector<std::string> &var_name_list)
{
    if (graph_->RemoveNodes(var_name_list) == true)
    {
        for (const std::string &var_name : var_name_list)
        {
            engine_->RemoveVariable(var_name, this);
        }
        return true;
    }
    return false;
}

bool ExprContext::RenameVariable(const std::string &old_name, const std::string &new_name)
{
    try
    {
        if (graph_->RenameNode(old_name, new_name))
        {
            Variable *var = GetVariable(new_name);
            var->set_name(new_name);
            engine_->RenameVariable(old_name, new_name, this);
            return true;
        }
        return false;
    }
    catch (const DependencyCycleException &e)
    {
        throw;
    }
}

bool ExprContext::SetVariableDirty(const std::string &var_name, bool dirty)
{
    return graph_->SetNodeDirty(var_name, dirty);
}

bool ExprContext::SetVariableDirty(Variable *var, bool dirty)
{
    if (var == nullptr)
        return false;
    return SetVariableDirty(var->name(), dirty);
}

bool ExprContext::IsVariableExist(const std::string &var_name) const
{
    return graph_->IsNodeExist(var_name);
}

bool ExprContext::IsVariableDirty(const std::string &var_name) const
{
    return graph_->IsNodeDirty(var_name);
}

void ExprContext::Reset()
{
    engine_->Reset();
    graph_->Reset();
}

void ExprContext::Update()
{
    graph_->UpdateGraph([this](const std::string &var_name) {
        Variable *var = graph_->GetVariable(var_name);
        if (var != nullptr)
        {
            Value value = var->Evaluate();
            engine_->SetVariable(var_name, value, this);
        }
    });
}

EvalResult ExprContext::Evaluate(const std::string &expr) const
{
    if (engine_ != nullptr)
    {
        return engine_->Evaluate(expr, this);
    }
    else
    {
        throw std::runtime_error("ExprEngine is not set for this context");
    }
}

ParseResult ExprContext::Parse(const std::string &expr) const
{
    if (engine_ != nullptr)
    {
        return engine_->Parse(expr);
    }
    else
    {
        throw std::runtime_error("ExprEngine is not set for this context");
    }
}