#pragma once

#include "core/equation.h"
#include "core/value.h"
#include <exception>
#include <functional>
#include <string>
#include <vector>

namespace xequation
{
class ExprContext;

struct ExecResult
{
    Equation::Status status;
    std::string message;
};

struct EvalResult
{
    Value value;
    Equation::Status status;
    std::string message;
};

using ParseResult = std::vector<Equation>;

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
typedef std::function<EvalResult(const std::string &, ExprContext *)>EvalCallback;
typedef std::function<ParseResult(const std::string &)> ParseCallback;
} // namespace xequation
