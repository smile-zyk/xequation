#include "expr_context.h"
#include "expr_engine.h"

using namespace xexprengine;

Value ExprContext::GetValue(const Variable* var) const
{
    return engine_->GetVariable(var->name(), this);
}

Variable* ExprContext::GetVariable(const std::string &var_name) const
{
}
