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

    virtual InterpretResult Interpret(const std::string& code, const EquationContext *context = nullptr, InterpretMode mode = InterpretMode::kExec) = 0;
    virtual ParseResult Parse(const std::string & code, ParseMode mode = ParseMode::kExpression) = 0;
    virtual std::string GetLanguage() const = 0;
    virtual std::unique_ptr<EquationManager> CreateEquationManager()
    {

        InterpretHandler interpret_handler = [this](const std::string &code, EquationContext *context, InterpretMode mode) -> InterpretResult {
            return Interpret(code, context, mode);
        };

        ParseHandler parse_callback = [this](const std::string &code, ParseMode mode) -> ParseResult {
            return Parse(code, mode);
        };
        
        return std::unique_ptr<EquationManager>(new EquationManager(CreateContext(), interpret_handler, parse_callback, GetLanguage()));
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