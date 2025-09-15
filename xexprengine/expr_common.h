#pragma once
#include "value.h"
#include <string>
#include <unordered_set>

namespace xexprengine
{
enum class VariableStatus
{
    kInit,
    kRawVar,
    kMissingDependency,
    kExprParseSuccess,
    kExprParseSyntaxError,
    kExprEvalSuccess,
    kExprEvalSyntaxError,
    kExprEvalNameError,
    kExprEvalTypeError,
    kExprEvalZeroDivisionError,
    kExprEvalValueError,
    kExprEvalMemoryError,
    kExprEvalOverflowError,
    kExprEvalRecursionError,
    kExprEvalIndexError,
    kExprEvalKeyError,
    kExprEvalAttributeError
};

struct EvalResult
{
    Value value;
    VariableStatus status;
    std::string eval_error_message;
};

struct ParseResult
{
    VariableStatus status;
    std::string parse_error_message;
    std::unordered_set<std::string> variables;
    std::unordered_set<std::string> functions;
    std::unordered_set<std::string> modules;
};
} // namespace xexprengine
