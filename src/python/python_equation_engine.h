#pragma once
#include "core/equation_common.h"
#include "core/equation_engine.h"
#include "python_executor.h"
#include "python_parser.h"
#include <memory>
#include <string>

// Python 环境管理委托给 REL 的 python_manager（见 3rd/REL/src/python_manager）。
#include <python_manager.h>


namespace xequation
{
namespace python
{
class PythonEquationEngine : public EquationEngine<PythonEquationEngine>
{
  public:
    InterpretResult Eval(const std::string &expr, const EquationContext *context = nullptr) override;
    InterpretResult Exec(const std::string &code, const EquationContext *context = nullptr) override;
    ParseResult Parse(const std::string &expr, ParseMode mode = ParseMode::kExpression) override;

    std::unique_ptr<EquationContext> CreateContext() override;
    
    // 实现基类的输出处理接口
    void SetOutputHandler(OutputHandler handler) override;
    
  private:
    friend class EquationEngine<PythonEquationEngine>;

    PythonEquationEngine();
    ~PythonEquationEngine() override;

  private:
    std::unique_ptr<PythonParser> code_parser = nullptr;
    std::unique_ptr<PythonExecutor> code_executor = nullptr;
    // 主线程初始化 Python 后保存的线程状态；非空表示主线程已让出 GIL
    // （参见构造函数末尾的 PyEval_SaveThread）。
    void *g_main_thread_state_ = nullptr;
};
} // namespace python
} // namespace xequation