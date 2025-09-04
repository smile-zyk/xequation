#pragma once
#include "expr_common.h"
#include "value.h"

namespace xexprengine
{
class ExprContext;
class Variable
{
  public:
    Variable(const std::string& name, const ExprContext *context = nullptr) : name_(name), context_(context){}
    virtual ~Variable() = default;
    const std::string &name() const
    {
        return name_;
    }
    const ExprContext *context() const
    {
        return context_;
    }

    Value GetValue();
    virtual Value Evaluate() = 0;
    virtual std::string Type() const = 0;

  protected:
    // for force set context when add to ExprContext 
    void set_context(const ExprContext *context)
    {
        context_ = context;
    }
    friend class ExprContext;

    // for access name_ when rename node
    void set_name(const std::string &name)
    {
        name_ = name;
    }
    friend class VariableDependencyGraph;

  private:
    std::string name_;
    const ExprContext *context_;

};

class RawVariable : public Variable
{
  public:
    RawVariable(const std::string &name, const Value &value, const ExprContext *context = nullptr)
        : Variable(name, context), value_(value)
    {
    }
    Value Evaluate() override;
    std::string Type() const override
    {
        return value_.Type().name();
    }
  private:
    Value value_;
};

class ExprVariable : public Variable
{
  public:
    ExprVariable(const std::string &name, const std::string &expression, const ExprContext *context = nullptr)
        : Variable(name, context), expression_(expression)
    {
    }

    const std::string &expression() const
    {
        return expression_;
    }

    void set_expression(const std::string &expression)
    {
        expression_ = expression;
    }

    std::string error_message() const
    {
        return error_message_;
    }

    ErrorCode error_code() const
    {
        return error_code_;
    }

    std::string Type() const override
    {
        return "expression";
    }

    virtual Value Evaluate() override;
  private:
    std::string expression_;
    std::string error_message_;
    ErrorCode error_code_ = ErrorCode::Success;
};
} // namespace xexprengine