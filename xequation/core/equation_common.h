#pragma once

#include "core/equation.h"
#include "core/value.h"
#include <exception>
#include <functional>
#include <string>
#include <vector>

namespace xequation
{
class EquationContext;

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

typedef std::function<ExecResult(const std::string &, EquationContext *)>ExecCallback;
typedef std::function<EvalResult(const std::string &, EquationContext *)>EvalCallback;
typedef std::function<ParseResult(const std::string &)> ParseCallback;
} // namespace xequation
