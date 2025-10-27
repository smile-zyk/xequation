#pragma once
#include "core/expr_common.h"
#include "core/expr_engine.h"
#include "py_symbol_extractor.h"
#include "py_restricted_evaluator.h"
#include <memory>
#include <string>

namespace xexprengine
{
class PyExprEngine : public ExprEngine<PyExprEngine>
{
  public:
    struct PyEnvConfig
    {
        std::string py_home;
        std::vector<std::string> lib_path_list;
    };
    static void SetPyEnvConfig(const PyEnvConfig& config);
    EvalResult Evaluate(const std::string &expr, const ExprContext *context = nullptr) override;
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
    std::unique_ptr<PySymbolExtractor> symbol_extractor_ = nullptr;
    std::unique_ptr<PyRestrictedEvaluator> restricted_evaluator_ = nullptr;
    bool manage_python_context_ = false;
};
} // namespace xexprengine