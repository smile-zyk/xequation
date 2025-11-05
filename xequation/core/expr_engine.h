#pragma once
#include <memory>
#include <string>

#include "equation_manager.h"
#include "expr_common.h"
#include "expr_context.h"

namespace xequation
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

    virtual ExecResult Exec(const std::string& code, const ExprContext *context = nullptr) = 0;
    virtual ParseResult Parse(const std::string & code) = 0;
    virtual EvalResult Eval(const std::string &code, const ExprContext *context = nullptr) = 0;

    virtual std::unique_ptr<EquationManager> CreateVariableManager()
    {
        ExecCallback exec_callback = [this](const std::string &code, ExprContext *context) -> ExecResult {
            return Exec(code, context);
        };

        EvalCallback eval_callback = [this](const std::string &code, ExprContext *context) -> EvalResult {
            return Eval(code, context);
        };

        ParseCallback parse_callback = [this](const std::string &code) -> ParseResult {
            return Parse(code);
        };

        return std::unique_ptr<EquationManager>(new EquationManager(CreateContext(), exec_callback, parse_callback, eval_callback));
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
} // namespace xequation