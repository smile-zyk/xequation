#pragma once
#include <memory>
#include <string>

#include "equation_manager.h"
#include "equation_common.h"
#include "equation_context.h"

namespace xequation
{
template <typename T>
class EquationEngine
{
  public:
    static T &GetInstance()
    {
        static T instance;
        return instance;
    }

    virtual ExecResult Exec(const std::string& code, const EquationContext *context = nullptr) = 0;
    virtual ParseResult Parse(const std::string & code) = 0;
    virtual EvalResult Eval(const std::string &code, const EquationContext *context = nullptr) = 0;

    virtual std::unique_ptr<EquationManager> CreateEquationManager()
    {
        ExecCallback exec_callback = [this](const std::string &code, EquationContext *context) -> ExecResult {
            return Exec(code, context);
        };

        EvalCallback eval_callback = [this](const std::string &code, EquationContext *context) -> EvalResult {
            return Eval(code, context);
        };

        ParseCallback parse_callback = [this](const std::string &code) -> ParseResult {
            return Parse(code);
        };

        return std::unique_ptr<EquationManager>(new EquationManager(CreateContext(), exec_callback, parse_callback, eval_callback));
    }

    virtual std::unique_ptr<EquationContext> CreateContext() = 0;

  protected:
    EquationEngine() = default;
    virtual ~EquationEngine() = default;
    EquationEngine(const EquationEngine &) = delete;
    EquationEngine(EquationEngine &&) = delete;
    EquationEngine &operator=(const EquationEngine &) = delete;
    EquationEngine &operator=(EquationEngine &&) = delete;
};
} // namespace xequation