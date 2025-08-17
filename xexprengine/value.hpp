#include <memory>
#include <string>
#include <typeinfo>

#include "value_helper.hpp"

class ValueBase {
public:
    virtual ~ValueBase() = default;
    virtual const std::type_info& Type() const = 0;
    virtual std::string ToString() const = 0;
    virtual std::shared_ptr<ValueBase> Clone() const = 0;
    virtual bool IsNull() const { return false; } 
};

class NullValue : public ValueBase {
public:
    const std::type_info& Type() const override {
        return typeid(void);
    }
    std::string ToString() const override {
        return "null";
    }
    std::shared_ptr<ValueBase> Clone() const override {
        return std::make_shared<NullValue>();
    }
    bool IsNull() const override { return true; }
};

template <typename T>
class Value : public ValueBase {
public:
    explicit Value(const T& val) : value_(val) {}

    const std::type_info& Type() const override { return typeid(T); }
    std::string ToString() const override
    {
        return ConvertToString(value_);
    }
    std::shared_ptr<ValueBase> Clone() const override {
        return std::make_shared<Value<T>>(value_);
    }
    const T& value() const { return value_; }
private:
    T value_;
};