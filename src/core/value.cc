#include "value.h"
#include <memory>

using namespace xequation;

std::unordered_map<std::type_index, std::vector<Value::BeforeOperationCallback>>
    Value::before_operation_callbacks_by_type_;
std::unordered_map<std::type_index, std::vector<Value::AfterOperationCallback>>
    Value::after_operation_callbacks_by_type_;
std::mutex Value::callbacks_mutex_;

void Value::NotifyBeforeOperation(const std::type_info &typeInfo)
{
    std::vector<BeforeOperationCallback> cbs;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        auto it = before_operation_callbacks_by_type_.find(std::type_index(typeInfo));
        if (it != before_operation_callbacks_by_type_.end())
        {
            cbs = it->second;
        }
    }
    for (auto &cb : cbs)
    {
        cb(typeInfo);
    }
}

void Value::NotifyAfterOperation(const std::type_info &typeInfo)
{
    std::vector<AfterOperationCallback> cbs;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        auto it = after_operation_callbacks_by_type_.find(std::type_index(typeInfo));
        if (it != after_operation_callbacks_by_type_.end())
        {
            cbs = it->second;
        }
    }
    for (auto &cb : cbs)
    {
        cb(typeInfo);
    }
}



Value::Value(const Value &other)
{
    NotifyBeforeOperation(other.Type());
    value_ptr_ = other.value_ptr_ != nullptr ? other.value_ptr_->Clone() : nullptr;
    NotifyAfterOperation(other.Type());
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
    // Capture the original type before moving
    const std::type_info &originalType = other.Type();
    
    // Begin operation for the type we're moving
    NotifyBeforeOperation(originalType);

    // Transfer ownership to this instance
    value_ptr_ = std::move(other.value_ptr_);

    // Reset other to a valid null immediately to avoid null deref during callbacks
    other.value_ptr_.reset(new ValueHolder<void>());

    // End operation using the same type we started with
    NotifyAfterOperation(originalType);
}

Value &Value::operator=(Value &&other) noexcept
{
    if (&other != this)
    {
        // Capture the original type before moving
        const std::type_info &originalType = other.Type();
        NotifyBeforeOperation(originalType);

        // Transfer ownership
        value_ptr_ = std::move(other.value_ptr_);

        // Reset other to a valid null to avoid null deref during callbacks
        other.value_ptr_.reset(new ValueHolder<void>());

        // End operation using the same type we started with
        NotifyAfterOperation(originalType);
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
    const std::type_info &typeInfo = value_ptr_->Type();
    NotifyBeforeOperation(typeInfo);
    std::string result = value_ptr_.get()->ToString();
    NotifyAfterOperation(typeInfo);
    return result;
}

Value::~Value() noexcept
{
    if (value_ptr_ && !value_ptr_->IsNull())
    {
        const std::type_info &typeInfo = value_ptr_->Type();
        NotifyBeforeOperation(typeInfo);

        // Release underlying storage; After uses saved type only.
        value_ptr_.reset();

        NotifyAfterOperation(typeInfo);
    }
}