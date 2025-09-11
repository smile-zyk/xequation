#pragma once
#include "expr_common.h"
#include "value.h"

namespace xexprengine
{
class ExprContext;

class Variable
{
  public:
    enum class Type
    {
        Raw,
        Expr,
        Func,
    };

    Variable(const std::string &name, ExprContext *context = nullptr) : name_(name){}
    virtual ~Variable() = default;
    const std::string &name() const
    {
        return name_;
    }

    const Value& cached_value() const
    {
      return cached_value_;
    }

    template <typename T, typename std::enable_if<std::is_base_of<Variable, T>::value, int>::type = 0>
    T* As() noexcept
    {
        return dynamic_cast<T *>(this);
    }

    virtual Value GetValue()  = 0;
    virtual Type GetType() const = 0;

    void set_name(const std::string &name)
    {
        name_ = name;
    }
  private:
    Value cached_value_;
    std::string name_;
};

class RawVariable : public Variable
{
  public:
    Variable::Type GetType() const override
    {
        return Variable::Type::Raw;
    }

  protected:
    RawVariable(const std::string &name, const Value &value, ExprContext *context = nullptr)
        : Variable(name, context), value_(value)
    {
    }
    friend class VariableFactory;

  private:
    Value value_;
};

class ExprVariable : public Variable
{
  public:
    const std::string &expression() const
    {
        return expression_;
    }

    std::string error_message() const
    {
        return error_message_;
    }

    ErrorCode error_code() const
    {
        return error_code_;
    }

    Variable::Type GetType() const override
    {
        return Variable::Type::Expr;
    }

  protected:
    ExprVariable(const std::string &name, const std::string &expression)
        : Variable(name), expression_(expression)
    {
    }

    friend class VariableFactory;

  private:
    std::string expression_;
    std::string error_message_;
    ErrorCode error_code_ = ErrorCode::Success;
};
} // namespace xexprengine