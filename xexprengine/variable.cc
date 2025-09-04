#include "variable.h"
#include "expr_common.h"
#include "value.h"
#include "expr_context.h"

using namespace xexprengine;

Value Variable::GetValue()
{
    return context_->GetValue(this);
}

Value RawVariable::Evaluate()
{
    return value_;
}

Value ExprVariable::Evaluate()
{
    EvalResult result = context_->Evaluate(expression_);
    error_code_ = result.error_code;
    error_message_ = result.error_message;
    return result.value;
}