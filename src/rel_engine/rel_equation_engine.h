#pragma once
#include <memory>
#include <string>

#include "core/equation_common.h"
#include "core/equation_engine.h"
#include "rel_executor.h"
#include "rel_parser.h"

namespace xequation
{
namespace rel_engine
{

// REL 方程引擎：仿照 python/PythonEquationEngine。
// 用 smile-zyk/REL 的语言前端（rel::Parse/Eval/Exec）解释执行，
// 变量存储在 rel::Environment（RelEquationContext）中。
class RelEquationEngine : public EquationEngine<RelEquationEngine>
{
  public:
    InterpretResult Eval(const std::string &expr, const EquationContext *context = nullptr) override;
    InterpretResult Exec(const std::string &code, const EquationContext *context = nullptr) override;
    ParseResult Parse(const std::string &expr, ParseMode mode = ParseMode::kExpression) override;

    std::unique_ptr<EquationContext> CreateContext() override;

    // 实现基类的输出处理接口（REL 无 stdout 捕获，留空）
    void SetOutputHandler(OutputHandler handler) override {}

  private:
    friend class EquationEngine<RelEquationEngine>;

    RelEquationEngine();
    ~RelEquationEngine() override;

  private:
    std::unique_ptr<RelParser> code_parser = nullptr;
    std::unique_ptr<RelExecutor> code_executor = nullptr;
};

} // namespace rel_engine
} // namespace xequation
