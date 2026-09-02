#pragma once

#include <boost/optional.hpp>
#include <string>
#include <utility>

#include "value.h"  // rel::Value

namespace xequation
{

// Wraps a rel::Value in a boost::optional so a name can hold a "no value"
// state (uncomputed / failed / unbound), which rel::Value itself cannot express.
class EquationValue
{
  public:
    // Constructors
    EquationValue() noexcept : storage_(boost::none) {}
    EquationValue(boost::none_t) noexcept : storage_(boost::none) {}

    EquationValue(const rel::Value &v) : storage_(v) {}
    EquationValue(rel::Value &&v) : storage_(std::move(v)) {}

    // Convenience scalar constructors mapped to rel::Value.
    EquationValue(int v) : storage_(rel::Value::Integer(v)) {}
    EquationValue(double v) : storage_(rel::Value::Real(v)) {}
    EquationValue(bool v) : storage_(rel::Value::Boolean(v)) {}
    EquationValue(const char *v) : storage_(rel::Value::String(std::string(v))) {}
    EquationValue(const std::string &v) : storage_(rel::Value::String(v)) {}

    // State queries
    bool IsNull() const noexcept
    {
        return !storage_;
    }
    bool HasValue() const noexcept
    {
        return storage_.is_initialized();
    }

    // Access
    const rel::Value &Value() const
    {
        return *storage_;
    }
    rel::Value &Value()
    {
        return *storage_;
    }

  private:
    boost::optional<rel::Value> storage_;
};

} // namespace xequation
