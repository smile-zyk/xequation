#include "value.h"
#include <memory>

using namespace xequation;

Value::Value(const Value &other)
{
    value_ptr_ = other.value_ptr_ != nullptr ? other.value_ptr_->Clone() : nullptr;
}

Value& Value::operator=(const Value &other)
{
    if(&other != this)
    {
        Value(other).swap(*this);
    }
    return *this;
}

Value::Value(Value &&other) noexcept
{
    value_ptr_ = std::move(other.value_ptr_);
    other.value_ptr_.reset(new ValueHolder<void>());
}

Value &Value::operator=(Value &&other) noexcept
{
    if (&other != this)
    {
        value_ptr_ = std::move(other.value_ptr_);
        other.value_ptr_.reset(new ValueHolder<void>());
    }
    return *this;
}

bool Value::operator==(const Value &other) const
{
    if (IsNull() != other.IsNull())
        return false;
    if (IsNull())
        return true;

    if (value_ptr_->Type() != other.value_ptr_->Type())
        return false;

    return ToString() == other.ToString();
}

void Value::swap(Value& other) noexcept
{
    using std::swap;
    swap(value_ptr_, other.value_ptr_);
}

bool Value::operator<(const Value &other) const
{
    if (IsNull() != other.IsNull())
    {
        return IsNull();
    }

    if (IsNull())
        return false;

    if (value_ptr_->Type() != other.value_ptr_->Type())
    {
        return value_ptr_->Type().before(other.value_ptr_->Type());
    }

    return ToString() < other.ToString();
}

bool Value::operator!=(const Value &other) const
{
    return !(*this == other);
}
bool Value::operator>(const Value &other) const
{
    return other < *this;
}
bool Value::operator<=(const Value &other) const
{
    return !(other < *this);
}
bool Value::operator>=(const Value &other) const
{
    return !(*this < other);
}

bool Value::IsNull() const
{
    return value_ptr_->IsNull();
}

const std::type_info &Value::Type() const
{
    return value_ptr_->Type();
}

std::string Value::ToString() const
{
    return value_ptr_.get()->ToString();
}