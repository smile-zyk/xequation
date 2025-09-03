#pragma once
#include "expression.h"
#include "value.h"
#include "expr_common.h"

namespace xexprengine 
{
    class ExprContext;
    class Variable
    {
      public:
        virtual ~Variable() = default;
        const std::string &name() const { return name_; }
        virtual Value GetValue() = 0;
      private:
        std::string name_;
    };

    class RawVariable : public Variable
    {
      public:
        RawVariable(const std::string &name, const Value &value) : name_(name), value_(value) {}
        Value GetValue() override { return value_; }
      private:
        std::string name_;
        Value value_;
    };

    class ExprVariable : public Variable
    {
      public:
        ExprVariable(const std::string &name, const std::string &expression, const ExprContext* context) : name_(name), expression_(expression), context_(context) {}
        Value GetValue() override;

      private:
        std::string name_;
        std::string expression_;
        const ExprContext *context_;
        bool is_evaluated_ = false;
        std::string error_message_;
        ErrorCode error_code_ = ErrorCode::Success;
    };
}