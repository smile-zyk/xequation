#pragma once
#include <memory>
#include <string>
#include <functional>

#include "equation_manager.h"
#include "equation_common.h"
#include "equation_context.h"

namespace xequation
{
// 输出处理回调类型
using OutputHandler = std::function<void(const std::string&)>;

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
    
    const EquationEngineInfo& GetEngineInfo() const { return engine_info_; }
    
    // 设置输出处理函数（用于捕获Python输出等）
    virtual void SetOutputHandler(OutputHandler handler) {}
    
    virtual std::unique_ptr<EquationManager> CreateEquationManager()
    {

        InterpretHandler interpret_handler = [this](const std::string &code, EquationContext *context, InterpretMode mode) -> InterpretResult {
            return Interpret(code, context, mode);
        };

        ParseHandler parse_callback = [this](const std::string &code, ParseMode mode) -> ParseResult {
            return Parse(code, mode);
        };
        
        return std::unique_ptr<EquationManager>(new EquationManager(CreateContext(), interpret_handler, parse_callback, engine_info_));
    }

    virtual std::unique_ptr<EquationContext> CreateContext() = 0;

  protected:
    EquationEngine() = default;
    virtual ~EquationEngine() = default;
    EquationEngine(const EquationEngine &) = delete;
    EquationEngine(EquationEngine &&) = delete;
    EquationEngine &operator=(const EquationEngine &) = delete;
    EquationEngine &operator=(EquationEngine &&) = delete;
    
    EquationEngineInfo engine_info_;
};
} // namespace xequation