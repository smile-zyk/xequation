#pragma once
#include <string>

#include "core/equation_common.h"
#include "rel.h"      // rel::Eval / rel::Exec
#include "environment.h"  // rel::Environment

namespace xequation
{
namespace rel_engine
{

// REL 表达式执行器：仿照 python/PythonExecutor，把 rel::Eval/rel::Exec
// 包装成 xequation 的 InterpretResult（异常映射到 ResultStatus）。
class RelExecutor
{
  public:
    RelExecutor() = default;
    ~RelExecutor() = default;

    RelExecutor(const RelExecutor &) = delete;
    RelExecutor &operator=(const RelExecutor &) = delete;

    // Executes REL code (赋值语句/表达式序列) in the given environment.
    InterpretResult Exec(const std::string &code, rel::Environment &env);

    // Evaluates a single REL expression in the given environment.
    InterpretResult Eval(const std::string &expression, rel::Environment &env);

  private:
    // 把 REL 抛出的 std::runtime_error 映射为 ResultStatus + message
    ResultStatus MapRelError(const std::string &message) const;
};

} // namespace rel_engine
} // namespace xequation
