#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>
#include <typeindex>
#include <unordered_map>


#include "value_string_converter.h"

namespace xequation
{
class ValueBase
{
  public:
    ValueBase() noexcept = default;
    virtual ~ValueBase() noexcept = default;
    ValueBase(const ValueBase &) noexcept = default;
    ValueBase &operator=(const ValueBase &) noexcept = default;
    ValueBase(ValueBase &&) noexcept = default;
    ValueBase &operator=(ValueBase &&) noexcept = default;
    virtual std::unique_ptr<ValueBase> Clone() const = 0;
    virtual const std::type_info &Type() const = 0;
    virtual std::string ToString() const = 0;
    virtual bool IsNull() const
    {
        return false;
    }
};

template <typename T>
class ValueHolder : public ValueBase
{
  public:
    explicit ValueHolder(const T &val) noexcept : value_(val) {}
    ~ValueHolder() noexcept = default;
    ValueHolder(const ValueHolder &other) noexcept : ValueBase(other), value_(other.value_) {}
    ValueHolder(ValueHolder &&other) noexcept : ValueBase(std::move(other)), value_(std::move(other.value_)) {}
    ValueHolder &operator=(const ValueHolder &other) noexcept
    {
        if (this != &other)
        {
            value_ = other.value_;
        }
        return *this;
    }
    ValueHolder &operator=(ValueHolder &&other) noexcept
    {
        if (this != &other)
        {
            value_ = std::move(other.value_);
        }
        return *this;
    }

    std::unique_ptr<ValueBase> Clone() const override
    {
        return std::unique_ptr<ValueHolder<T>>(new ValueHolder<T>(value_));
    }

    const std::type_info &Type() const override
    {
        return typeid(T);
    }

    std::string ToString() const override
    {
        return value_convert::StringConverter::ToString(value_);
    }

    bool IsNull() const override
    {
        return false;
    }

    const T &value() const
    {
        return value_;
    }

  private:
    T value_;
};

template <>
class ValueHolder<void> : public ValueBase
{
  public:
    ValueHolder() noexcept = default;
    ~ValueHolder() noexcept = default;

    ValueHolder(const ValueHolder &other) noexcept = default;
    ValueHolder(ValueHolder &&other) noexcept = default;
    ValueHolder &operator=(const ValueHolder &other) noexcept = default;
    ValueHolder &operator=(ValueHolder &&other) noexcept = default;

    std::unique_ptr<ValueBase> Clone() const override
    {
        return std::unique_ptr<ValueHolder<void>>(new ValueHolder<void>());
    }

    const std::type_info &Type() const override
    {
        return typeid(void);
    }

    std::string ToString() const override
    {
        return "null";
    }

    bool IsNull() const override
    {
        return true;
    }
};

class Value
{
  public:
    Value() noexcept : value_ptr_(nullptr)
    {
        NotifyBeforeOperation(typeid(void));
        value_ptr_.reset(new ValueHolder<void>());
        NotifyAfterOperation(typeid(void));
    }
    ~Value() noexcept;

    template <typename T>
    Value(const T &val) noexcept : value_ptr_(nullptr)
    {
        NotifyBeforeOperation(typeid(T));
        value_ptr_.reset(new ValueHolder<T>(val));
        NotifyAfterOperation(typeid(T));
    }

    Value(const char *val) noexcept : value_ptr_(nullptr)
    {
        NotifyBeforeOperation(typeid(std::string));
        value_ptr_.reset(new ValueHolder<std::string>(val));
        NotifyAfterOperation(typeid(std::string));
    }

    Value(const Value &other);
    Value &operator=(const Value &other);

    Value(Value &&) noexcept;
    Value &operator=(Value &&) noexcept;

    void swap(Value &other) noexcept;

    static const Value &Null()
    {
        static const Value nullValue;
        return nullValue;
    }

    operator bool() const
    {
        return !IsNull();
    }

    bool operator==(const Value &other) const;
    bool operator!=(const Value &other) const;
    bool operator<(const Value &other) const;
    bool operator>(const Value &other) const;
    bool operator<=(const Value &other) const;
    bool operator>=(const Value &other) const;

    bool IsNull() const;

    const std::type_info &Type() const;

    template <typename T>
    typename std::decay<T>::type Cast() const
    {
        static_assert(!std::is_same<typename std::decay<T>::type, Value>::value, "Cannot cast Value to Value type");

        if (IsNull())
        {
            throw std::runtime_error("Cannot cast null value");
        }
        typedef typename std::decay<T>::type DecayedT;
        ValueHolder<DecayedT> *derived = dynamic_cast<ValueHolder<DecayedT> *>(value_ptr_.get());
        if (!derived)
        {
            throw std::runtime_error(
                "Bad cast from " + std::string(Type().name()) + " to " + std::string(typeid(DecayedT).name())
            );
        }
        return derived->value();
    }

    std::string ToString() const;

    friend std::ostream &operator<<(std::ostream &os, const Value &value)
    {
        os << value.ToString();
        return os;
    }

  private:
    std::unique_ptr<ValueBase> value_ptr_;

  public:
    using BeforeOperationCallback = std::function<void(const std::type_info &)>;
    using AfterOperationCallback = std::function<void(const std::type_info &)>;

    template <typename T>
    static void RegisterBeforeOperation(BeforeOperationCallback cb)
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        before_operation_callbacks_by_type_[std::type_index(typeid(T))].push_back(std::move(cb));
    }

    template <typename T>
    static void RegisterAfterOperation(AfterOperationCallback cb)
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        after_operation_callbacks_by_type_[std::type_index(typeid(T))].push_back(std::move(cb));
    }

  private:
    static void NotifyBeforeOperation(const std::type_info &typeInfo);
    static void NotifyAfterOperation(const std::type_info &typeInfo);

    static std::unordered_map<std::type_index, std::vector<BeforeOperationCallback>>
        before_operation_callbacks_by_type_;
    static std::unordered_map<std::type_index, std::vector<AfterOperationCallback>>
        after_operation_callbacks_by_type_;
    static std::mutex callbacks_mutex_;
};
} // namespace xequation

namespace std
{
template <>
struct hash<xequation::Value>
{
    size_t operator()(const xequation::Value &value) const
    {
        return std::hash<std::string>()(value.ToString());
    }
};
} // namespace std
