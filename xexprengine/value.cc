#include "value.h"

using namespace xexprengine;

void Value::ToNull()
{
    value_ptr_.reset(new ValueHolder<void>());
}

Value::Value(Value&& other) noexcept
{
    value_ptr_ = std::move(other.value_ptr_);
    other.ToNull();
}

Value& Value::operator=(Value&& other) noexcept
{
    if(&other != this)
    {
        value_ptr_ = std::move(other.value_ptr_);
        other.ToNull();
    }
    return *this;
}

bool Value::operator==(const Value& other) const {
    if (IsNull() != other.IsNull()) return false;
    if (IsNull()) return true;
    
    if (value_ptr_->Type() != other.value_ptr_->Type()) return false;
    
    return ToString() == other.ToString();
}

bool Value::operator<(const Value& other) const {
    if (IsNull() != other.IsNull()) 
    {
        return IsNull();
    }
    
    if (IsNull()) return false;
    
    if (value_ptr_->Type() != other.value_ptr_->Type()) {
        return value_ptr_->Type().before(other.value_ptr_->Type());
    }
    
    return ToString() < other.ToString();
}

bool Value::operator!=(const Value& other) const { return !(*this == other); }
bool Value::operator>(const Value& other) const { return other < *this; }
bool Value::operator<=(const Value& other) const { return !(other < *this); }
bool Value::operator>=(const Value& other) const { return !(*this < other); }

bool Value::IsNull() const
{
    return value_ptr_->IsNull();
}

const std::type_info& Value::Type() const
{
    return value_ptr_->Type();
}

std::string Value::ToString() const
{
    return value_ptr_.get()->ToString();
}