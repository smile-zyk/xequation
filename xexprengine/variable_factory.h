#pragma once
#include "variable.h"

namespace xexprengine
{

class VariableFactory
{
  public:
    static std::unique_ptr<Variable>
    CreateRawVariable(const std::string &name, const Value &value, ExprContext *context = nullptr);

    static std::unique_ptr<Variable>
    CreateExprVariable(const std::string &name, const std::string &expression, ExprContext *context = nullptr);

    static std::unique_ptr<Variable>
    CreateVariable(const std::string &name, const Value &value, ExprContext *context = nullptr);

    static std::unique_ptr<Variable>
    CreateVariable(const std::string &name, const std::string &expression, ExprContext *context = nullptr);
};

} // namespace xexprengine