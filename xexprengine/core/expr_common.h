#pragma once
#include "value.h"
#include <map>
#include <string>
#include <functional>
#include <unordered_set>

namespace xexprengine
{
enum class VariableStatus
{
    kInit,
    kInvalidContext,
    kRawVar,
    kMissingDependency,
    kParseSuccess,
    kParseSyntaxError,
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
    kExprEvalAttributeError,
    kModuleImportSuccess,
    kModuleImportError,
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
};

enum class ModuleType
{
    kDirect,
    kPath
};

struct ModuleInfo
{
    std::string name;
    std::string alias;
    std::string path;
    ModuleType type;
    bool is_expose_symbol;
    std::vector<std::string> exposed_symbols;
};

struct ImportResult
{
    Value module;
    std::map<std::string, Value> exposed_module_symbol;
    std::string import_error_message;
    VariableStatus status;
};

class ExprContext;

typedef std::function<EvalResult(const std::string &, ExprContext*)> EvalCallback;
typedef std::function<ParseResult(const std::string &)> ParseCallback;
typedef std::function<ImportResult(ModuleInfo&)> ImportCallback;
} // namespace xexprengine
