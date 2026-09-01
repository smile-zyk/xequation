#include "rel_executor.h"

namespace xequation
{
namespace rel_engine
{

namespace
{
// REL 错误消息的关键字 -> ResultStatus 映射
ResultStatus MapMessageToStatus(const std::string &message)
{
    if (message.find("syntax error") != std::string::npos ||
        message.find("Syntax error") != std::string::npos ||
        message.find("Unexpected token") != std::string::npos)
    {
        return ResultStatus::kSyntaxError;
    }
    if (message.find("undefined") != std::string::npos ||
        message.find("not defined") != std::string::npos ||
        message.find("not found") != std::string::npos ||
        message.find("Unknown variable") != std::string::npos ||
        message.find("unknown variable") != std::string::npos)
    {
        return ResultStatus::kNameError;
    }
    if (message.find("divide by zero") != std::string::npos ||
        message.find("division by zero") != std::string::npos)
    {
        return ResultStatus::kZeroDivisionError;
    }
    if (message.find("type") != std::string::npos)
    {
        return ResultStatus::kTypeError;
    }
    return ResultStatus::kUnknownError;
}

} // namespace

ResultStatus RelExecutor::MapRelError(const std::string &message) const
{
    return MapMessageToStatus(message);
}

InterpretResult RelExecutor::Exec(const std::string &code, rel::Environment &env)
{
    InterpretResult res;

    try
    {
        rel::Exec(code, env);
        res.status = ResultStatus::kSuccess;
        res.value = EquationValue::Null();
    }
    catch (const std::exception &e)
    {
        res.status = MapRelError(e.what());
        res.message = e.what();
        res.value = EquationValue::Null();
    }
    return res;
}

InterpretResult RelExecutor::Eval(const std::string &expression, rel::Environment &env)
{
    InterpretResult res;

    try
    {
        rel::Value value = rel::Eval(expression, &env);
        res.status = ResultStatus::kSuccess;
        res.value = EquationValue(value);
    }
    catch (const std::exception &e)
    {
        res.status = MapRelError(e.what());
        res.message = e.what();
        res.value = EquationValue::Null();
    }
    return res;
}

} // namespace rel_engine
} // namespace xequation
