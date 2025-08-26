#include "expr_context.h"
#include "expr_engine.h"
#include "expr_value.h"

using namespace xexprengine;

void ExprContext::SetVariable(const std::string& var_name, const ExprValue& value)
{
    engine_->SetVariable(var_name, value, this);
    var_set_.insert(var_name);
    
    // add to graph
    if(!var_graph_.count(var_name))
    {
        var_graph_.insert({var_name, ExprNode()});
    }

    // remove expression
    if(expr_map_.count(var_name))
    {
        expr_map_.erase(var_name);
    }
}

ExprValue ExprContext::GetVariable(const std::string& var_name)
{
    if(var_set_.count(var_name))
    {
        return engine_->GetVariable(var_name, this);
    }
    else
    {
        return ExprValue::Null();
    }
}

void ExprContext::RemoveVariable(const std::string& var_name)
{
    if(var_set_.count(var_name))
    {
        engine_->RemoveVariable(var_name, this);
    }
}

void ExprContext::RenameVariable(const std::string& old_name, const std::string& new_name)
{
}