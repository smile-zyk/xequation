#pragma once
#include "expr_common.h"
#include "value.h"

namespace xexprengine
{
class Variable
{
  public:
    enum class Type
    {
        Raw,
        Expr,
        Func,
    };

    Variable(const std::string &name) : name_(name) {}
    virtual ~Variable() = default;

    void set_name(const std::string &name)
    {
        name_ = name;
    }

    const std::string &name() const
    {
        return name_;
    }

    template <typename T, typename std::enable_if<std::is_base_of<Variable, T>::value, int>::type = 0>
    T *As() noexcept
    {
        return dynamic_cast<T *>(this);
    }

    template <typename T, typename std::enable_if<std::is_base_of<Variable, T>::value, int>::type = 0> 
    const T *As() const noexcept 
    {
        return dynamic_cast<const T *>(this);
    }

    void set_error_message(const std::string &message)
    {
        error_message_ = message;
    }

    void set_status(VariableStatus code)
    {
        status_ = code;
    }

    std::string error_message() const
    {
        return error_message_;
    }

    VariableStatus status() const
    {
        return status_;
    }

    virtual Type GetType() const = 0;

  private:
    std::string name_;
    std::string error_message_;
    VariableStatus status_ = VariableStatus::kInit;
};

class VariableFactory
{
  public:
    static std::unique_ptr<Variable> CreateRawVariable(const std::string &name, const Value &value);

    static std::unique_ptr<Variable> CreateExprVariable(const std::string &name, const std::string &expression);
};

class RawVariable : public Variable
{
  public:
    void set_value(const Value &value)
    {
        value_ = value;
    }

    const Value &value() const
    {
        return value_;
    }

    Variable::Type GetType() const override
    {
        return Variable::Type::Raw;
    }

  protected:
    RawVariable(const std::string &name, const Value &value)
        : Variable(name), value_(value)
    {
    }
    friend class VariableFactory;
  private:
    Value value_;
};

class ExprVariable : public Variable
{
  public:
    void set_expression(const std::string &expression)
    {
        expression_ = expression;
    }

    const std::string &expression() const
    {
        return expression_;
    }

    Variable::Type GetType() const override
    {
        return Variable::Type::Expr;
    }

  protected:
    ExprVariable(const std::string &name, const std::string &expression) : Variable(name), expression_(expression) {}
    friend class VariableFactory;
  private:
    std::string expression_;
};
} // namespace xexprengine