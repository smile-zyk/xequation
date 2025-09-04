#pragma once
#include "expr_common.h"
#include "value.h"

namespace xexprengine
{
class ExprContext;
class Variable
{
  public:
    Variable(const std::string& name, const ExprContext *context) : name_(name), context_(context){}
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

  protected:
    std::string name_;
    const ExprContext *context_;
    friend class ExprContext;
};

class RawVariable : public Variable
{
  public:
    RawVariable(const std::string &name, const Value &value, const ExprContext *context)
        : Variable(name, context), value_(value)
    {
    }
    Value Evaluate() override;
  private:
    Value value_;
};

class ExprVariable : public Variable
{
  public:
    ExprVariable(const std::string &name, const std::string &expression, const ExprContext *context)
        : Variable(name, context), expression_(expression)
    {
    }

    virtual Value Evaluate();
  private:
    std::string expression_;
    bool is_evaluated_ = false;
    std::string error_message_;
    ErrorCode error_code_ = ErrorCode::Success;
};
} // namespace xexprengine