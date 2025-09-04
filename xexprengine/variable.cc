#include "variable.h"
#include "expr_common.h"
#include "expr_context.h"
#include "value.h"

using namespace xexprengine;

Value Variable::GetValue()
{
    if (context_ != nullptr)
        return context_->GetValue(this);
    return Value::Null();
}

Value RawVariable::Evaluate()
{
    return value_;
}

Value ExprVariable::Evaluate()
{
    const ExprContext *ctx = context();
    if (ctx != nullptr)
    {
        EvalResult result = ctx->Evaluate(expression_);
        error_code_ = result.error_code;
        error_message_ = result.error_message;
        return result.value;
    }
    return Value::Null();
}