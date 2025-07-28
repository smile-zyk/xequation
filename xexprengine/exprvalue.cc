#include "exprvalue.h"
#include <utility>

ExprValue::ExprValue(ExprValue&& other)
{
    value_ptr_ = std::move(other.value_ptr_);
}

ExprValue& ExprValue::operator=(ExprValue&& other)
{
    if(&other != this)
    {
        value_ptr_ = std::move(other.value_ptr_);
    }
    return *this;
}

bool ExprValue::IsNull() const
{
    return value_ptr_->IsNull();
}

const std::type_info& ExprValue::Type() const
{
    return value_ptr_->Type();
}

std::string ExprValue::ToString()
{
    return value_ptr_->ToString();
}


