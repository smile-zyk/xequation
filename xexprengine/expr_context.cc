#include "expr_context.h"
#include "expr_engine.h"
#include "value.h"
#include "variable.h"
#include "variable_factory.h"

using namespace xexprengine;

ExprContext::ExprContext(const std::string& name) : name_(name), graph_(new VariableDependencyGraph())
{
}

Value ExprContext::GetValue(const Variable* var) const
{
    return engine_->GetVariable(var->name(), this);
}

Variable* ExprContext::GetVariable(const std::string &var_name) const
{
    return graph_->GetVariable(var_name);
}

bool ExprContext::SetVariable(std::unique_ptr<Variable> var)
{
    var->set_context(this);
    if(var->Type() == "expression")
    {
        ExprVariable* expr_var = dynamic_cast<ExprVariable*>(var.get());
        if(expr_var != nullptr)
        {
            auto analyze_result = engine_->Analyze(expr_var->expression());
            for (const std::string &dep_var_name : analyze_result.variables)
            {
                graph_->AddEdge(var->name(), dep_var_name);
            }
        }
    }
    return graph_->AddNode(std::move(var));
}

bool ExprContext::SetVariables(const std::vector<std::unique_ptr<Variable>> &var_list)
{
    for (const auto &var : var_list)
    {
        var->set_context(this);
    }
    return graph_->AddNodes(var_list);
}

bool ExprContext::SetRawVariable(std::unique_ptr<RawVariable> var)
{
    var->set_context(this);
    return SetVariable(std::move(var));
}

bool ExprContext::SetRawVariable(const std::string& var_name, const Value& value)
{
    return SetRawVariable(var_name, VariableFactory::CreateRawVariable(var_name, value, this));
}

bool ExprContext::SetExprVariable(std::unique_ptr<ExprVariable> var)
{
    var->set_context(this);
    return SetVariable(std::move(var));
}

bool ExprContext::SetExprVariable(const std::string& var_name, const std::string& expression)
{
    return SetVariable(VariableFactory::CreateExprVariable(var_name, expression, this));
}

bool ExprContext::RemoveVariable(const std::string &var_name)
{
    if(graph_->RemoveNode(var_name))
    {
        engine_->RemoveVariable(var_name, this);
        return true;
    }
    return false;
}

bool ExprContext::RemoveVariable(Variable* var)
{
    if(var == nullptr)
        return false;
    return RemoveVariable(var->name());
}

bool ExprContext::RemoveVariables(const std::vector<std::string> &var_name_list)
{
    if(graph_->RemoveNodes(var_name_list) == true)
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
    if(graph_->RenameNode(old_name, new_name))
    {
        engine_->RenameVariable(old_name, new_name, this);
        return true;
    }
    return false;
}

bool ExprContext::SetVariableDirty(const std::string &var_name, bool dirty)
{
    return graph_->SetNodeDirty(var_name, dirty);
}

bool ExprContext::SetVariableDirty(Variable* var, bool dirty)
{
    if(var == nullptr)
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

void ExprContext::Update()
{
    graph_->UpdateGraph([this](const std::string &var_name)
    {
        Variable* var = graph_->GetVariable(var_name);
        if(var != nullptr)
        {
            Value value = var->Evaluate();
            engine_->SetVariable(var_name, value, this);
        }
    });
}

EvalResult ExprContext::Evaluate(const std::string &expr) const
{
    if(engine_ != nullptr)
    {
        return engine_->Evaluate(expr, const_cast<ExprContext*>(this));
    }
    else 
    {
        throw std::runtime_error("ExprEngine is not set for this context");
    }
}
