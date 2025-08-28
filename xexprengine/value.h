#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>

#include "value_convert.h"

namespace xexprengine {

class ValueBase {
public:
  ValueBase() noexcept = default;
  virtual ~ValueBase() noexcept = default;
  virtual const std::type_info &Type() const = 0;
  virtual std::string ToString() const = 0;
  virtual bool IsNull() const { return false; }
};

template <typename T> class ValueHolder : public ValueBase {
public:
  explicit ValueHolder(const T &val) noexcept : value_(val) {}
  ~ValueHolder() noexcept = default;
  const std::type_info &Type() const override { return typeid(T); }
  std::string ToString() const override {
    return value_convert::StringConverter::ToString(value_);
  }
  bool IsNull() const override { return false; }
  const T &value() const { return value_; }

private:
  T value_;
};

template <> class ValueHolder<void> : public ValueBase {
public:
  ValueHolder() noexcept = default;
  ~ValueHolder() noexcept = default;
  const std::type_info &Type() const override { return typeid(void); }
  std::string ToString() const override { return "null"; }
  bool IsNull() const override { return true; }
};

class Value {
public:
  Value() noexcept : value_ptr_(new ValueHolder<void>()) {}
  ~Value() noexcept {}

  template <typename T>
  Value(const T &val) noexcept : value_ptr_(new ValueHolder<T>(val)) {}

  Value(const char *val) noexcept
      : value_ptr_(new ValueHolder<std::string>(val)) {}

  static const Value &Null() {
    static const Value nullValue;
    return nullValue;
  }

  void ToNull();

  operator bool() const { return !IsNull(); }

  bool operator==(const Value &other) const;
  bool operator!=(const Value &other) const;
  bool operator<(const Value &other) const;
  bool operator>(const Value &other) const;
  bool operator<=(const Value &other) const;
  bool operator>=(const Value &other) const;

  Value(const Value &other) = default;
  Value &operator=(const Value &other) = default;

  Value(Value &&) noexcept;
  Value &operator=(Value &&) noexcept;

  bool IsNull() const;

  const std::type_info &Type() const;

  template <typename T> typename std::decay<T>::type Cast() const {
    static_assert(!std::is_same<typename std::decay<T>::type, Value>::value,
                  "Cannot cast Value to Value type");

    if (IsNull()) {
      throw std::runtime_error("Cannot cast null value");
    }
    typedef typename std::decay<T>::type DecayedT;
    ValueHolder<DecayedT> *derived =
        dynamic_cast<ValueHolder<DecayedT> *>(value_ptr_.get());
    if (!derived) {
      throw std::runtime_error("Bad cast from " + std::string(Type().name()) +
                               " to " + std::string(typeid(DecayedT).name()));
    }
    return derived->value();
  }

  std::string ToString() const;

private:
  std::shared_ptr<ValueBase> value_ptr_;
};
} // namespace xexprengine

namespace std {
template <> struct hash<xexprengine::Value> {
  size_t operator()(const xexprengine::Value &value) const {
    return std::hash<std::string>()(value.ToString());
  }
};
} // namespace std
