#pragma once

#include <exception>
#include <functional>
#include <string>
#include <vector>

namespace xequation
{
class ExprContext;

enum class ExecStatus
{
    kInit,
    kSuccess,
    kSyntaxError,
    kNameError,
    kTypeError,
    kZeroDivisionError,
    kValueError,
    kMemoryError,
    kOverflowError,
    kRecursionError,
    kIndexError,
    kKeyError,
    kAttributeError,
};

enum class ParseType
{
    kErrorType,
    kImport,
    kImportFrom,
    kFuncDecl,
    kClassDecl,
    kVarDecl,
};

struct ExecResult
{
    ExecStatus status;
    std::string message;
};

struct ParseResult
{
    std::string name;
    std::string content;
    ParseType type;
    std::vector<std::string> dependencies;
};

class ParseException : public std::exception {
private:
    std::string error_message_;

public:
    ParseException(const std::string& message)
        :error_message_(message) {}

    const char* what() const noexcept override {
        return error_message_.c_str();
    }

    const std::string& error_message() const { return error_message_; }
};

class DuplicateEquationNameError : public std::exception
{
private:
    std::string exist_equation_name_;
    std::string exist_equation_content_;
public:
    DuplicateEquationNameError(const std::string& eqn_name, const std::string eqn_content) : exist_equation_name_(eqn_name), exist_equation_content_(eqn_content){}
    
    const char* what() const noexcept override {
        static std::string res;
        res = exist_equation_name_ + " is exist! exist equation content: " + exist_equation_content_;
        return res.c_str();
    }

    const std::string& exist_equation_name() const { return exist_equation_name_; }
    const std::string& exist_equation_content() const { return exist_equation_content_; }
};

typedef std::function<ExecResult(const std::string &, ExprContext *)>ExecCallback;
typedef std::function<ParseResult(const std::string &)> ParseCallback;
} // namespace xequation
