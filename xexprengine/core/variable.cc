#include "variable.h"
#include "expr_common.h"

using namespace xexprengine;

std::unique_ptr<Variable> VariableFactory::CreateRawVariable(const std::string &name, const Value &value)
{
    return std::unique_ptr<Variable>(new RawVariable(name, value));
}

std::unique_ptr<Variable> VariableFactory::CreateExprVariable(const std::string &name, const std::string &expression)
{
    return std::unique_ptr<Variable>(new ExprVariable(name, expression));
}

std::unique_ptr<Variable> VariableFactory::CreateModuleVariable(const std::string &name, const ModuleInfo& module_info)
{
    return std::unique_ptr<Variable>(new ModuleVariable(name, module_info));
}