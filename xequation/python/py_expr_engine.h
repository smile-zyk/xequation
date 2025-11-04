#pragma once
#include "core/expr_common.h"
#include "core/expr_engine.h"
#include "py_code_executor.h"
#include "py_code_parser.h"
#include <memory>
#include <string>


namespace xequation
{
namespace python
{
class PyExprEngine : public ExprEngine<PyExprEngine>
{
  public:
    struct PyEnvConfig
    {
        std::string py_home;
        std::vector<std::string> lib_path_list;
    };
    static void SetPyEnvConfig(const PyEnvConfig &config);
    ExecResult Exec(const std::string &expr, const ExprContext *context = nullptr) override;
    ParseResult Parse(const std::string &expr) override;
    std::unique_ptr<ExprContext> CreateContext() override;

  private:
    friend class ExprEngine<PyExprEngine>;

    void InitializePyEnv();
    void FinalizePyEnv();
    PyExprEngine();
    ~PyExprEngine() override;

  private:
    static PyEnvConfig config_;
    std::unique_ptr<PyCodeParser> code_parser = nullptr;
    std::unique_ptr<PyCodeExecutor> code_executor = nullptr;
    bool manage_python_context_ = false;
};
} // namespace python
} // namespace xequation