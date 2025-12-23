#pragma once
#include "core/equation_common.h"
#include "core/equation_engine.h"
#include "python_executor.h"
#include "python_parser.h"
#include <memory>
#include <string>


namespace xequation
{
namespace python
{
class PythonEquationEngine : public EquationEngine<PythonEquationEngine>
{
  public:
    struct PyEnvConfig
    {
        std::string py_home;
        std::vector<std::string> lib_path_list;
    };
    static void SetPyEnvConfig(const PyEnvConfig &config);
    InterpretResult Interpret(const std::string &expr, const EquationContext *context = nullptr, InterpretMode mode = InterpretMode::kExec) override;
    ParseResult Parse(const std::string &expr, ParseMode mode = ParseMode::kExpression) override;
    std::unique_ptr<EquationContext> CreateContext() override;
    std::string GetLanguage() const override { return "Python"; }
  private:
    friend class EquationEngine<PythonEquationEngine>;

    void InitializePyEnv();
    void FinalizePyEnv();
    PythonEquationEngine();
    ~PythonEquationEngine() override;

  private:
    static PyEnvConfig config_;
    std::unique_ptr<PythonParser> code_parser = nullptr;
    std::unique_ptr<PythonExecutor> code_executor = nullptr;
    bool manage_python_context_ = false;
};
} // namespace python
} // namespace xequation