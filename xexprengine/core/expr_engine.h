#pragma once
#include "expr_common.h"
#include "expr_context.h"
#include "variable_manager.h"

namespace xexprengine
{
template <typename T>
class ExprEngine
{
  public:
    static T &GetInstance()
    {
        static T instance;
        return instance;
    }

    virtual EvalResult Evaluate(const std::string &expr, const ExprContext *context = nullptr) = 0;
    virtual ParseResult Parse(const std::string &expr) = 0;

    virtual std::unique_ptr<VariableManager> CreateVariableManager()
    {
        EvalCallback evaluate_callback = [this](const std::string &expression, ExprContext *context) -> EvalResult {
            return Evaluate(expression, context);
        };

        ParseCallback parse_callback = [this](const std::string &expression) -> ParseResult {
            return Parse(expression);
        };

        return std::unique_ptr<VariableManager>(new VariableManager(evaluate_callback, parse_callback, CreateContext()));
    }

    virtual std::unique_ptr<ExprContext> CreateContext() = 0;

  protected:
    ExprEngine() = default;
    virtual ~ExprEngine() = default;
    ExprEngine(const ExprEngine &) = delete;
    ExprEngine(ExprEngine &&) = delete;
    ExprEngine &operator=(const ExprEngine &) = delete;
    ExprEngine &operator=(ExprEngine &&) = delete;
};
} // namespace xexprengine