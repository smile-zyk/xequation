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
    ExecResult Exec(const std::string &expr, const EquationContext *context = nullptr) override;
    ParseResult Parse(const std::string &expr) override;
    EvalResult Eval(const std::string &expr, const EquationContext *context = nullptr) override;
    std::unique_ptr<EquationContext> CreateContext() override;

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