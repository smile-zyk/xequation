#include "variable_factory.h"
#include "variable.h"

namespace xexprengine {

std::unique_ptr<Variable> VariableFactory::CreateRawVariable(
    const std::string& name, 
    const Value& value,
    const ExprContext* context)
{
    return std::unique_ptr<Variable>(new RawVariable(name, value, context));
}

std::unique_ptr<Variable> VariableFactory::CreateExprVariable(
    const std::string& name,
    const std::string& expression,
    const ExprContext* context)
{
    return std::unique_ptr<Variable>(new ExprVariable(name, expression, context));
}

std::unique_ptr<Variable> VariableFactory::CreateVariable(
    const std::string& name,
    const Value& value,
    const ExprContext* context)
{
    return CreateRawVariable(name, value, context);
}

std::unique_ptr<Variable> VariableFactory::CreateVariable(
    const std::string& name,
    const std::string& expression,
    const ExprContext* context)
{
    return CreateExprVariable(name, expression, context);
}

} // namespace xexprengine