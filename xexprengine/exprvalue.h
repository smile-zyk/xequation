#include "value.hpp"

#include <stdexcept>

class ExprValue {
public:
    ExprValue();
    ~ExprValue(){}

    template <typename T>
    ExprValue(const T& val) : value_ptr_(new Value<T>(val)) {}

    ExprValue(const char* val) : value_ptr_(new Value<std::string>(val)) {}

    static ExprValue Null() {
        return ExprValue();
    }

    ExprValue(const ExprValue& other) = default;
    ExprValue& operator=(const ExprValue& other) = default;

    ExprValue(ExprValue&&);
    ExprValue& operator=(ExprValue&&);

    bool IsNull() const;

    const std::type_info& Type() const;

    template <typename T>
    T Cast() const {
        if (IsNull()) throw std::runtime_error("Cannot cast null");
        auto derived = dynamic_cast<Value<T>*>(value_ptr_.get());
        if (!derived) throw std::runtime_error("Bad cast");
        return derived->value();
    }

    std::string ToString() const;

private:
    std::shared_ptr<ValueBase> value_ptr_;
};