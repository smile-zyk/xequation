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

    void set_cached_value(const Value &value)
    {
        cached_value_ = value;
    }

    const Value &cached_value() const
    {
        return cached_value_;
    }

    template <typename T, typename std::enable_if<std::is_base_of<Variable, T>::value, int>::type = 0>
    T *As() noexcept
    {
        return dynamic_cast<T *>(this);
    }

    virtual Type GetType() const = 0;

  private:
    Value cached_value_;
    std::string name_;
};

class VariableFactory
{
  public:
    static std::unique_ptr<Variable> CreateRawVariable(const std::string &name, const Value &value);

    static std::unique_ptr<Variable> CreateExprVariable(const std::string &name, const std::string &expression);

    static std::unique_ptr<Variable> CreateVariable(const std::string &name, const Value &value);

    static std::unique_ptr<Variable> CreateVariable(const std::string &name, const std::string &expression);
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

    void SetEvalResult(const EvalResult &result)
    {
        error_code_ = result.error_code;
        error_message_ = result.error_message;
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
    ExprVariable(const std::string &name, const std::string &expression) : Variable(name), expression_(expression) {}

    friend class VariableFactory;

  private:
    std::string expression_;
    std::string error_message_;
    ErrorCode error_code_ = ErrorCode::Success;
};
} // namespace xexprengine