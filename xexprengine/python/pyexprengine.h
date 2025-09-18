#pragma once
#include "core/expr_engine.h"

namespace xexprengine
{
class PyExprEngine : public ExprEngine<PyExprEngine>
{
  public:
    EvalResult Evaluate(const std::string &expr, const ExprContext *context = nullptr) override;
    ParseResult Parse(const std::string &expr) override;
  private:
    friend class ExprEngine<PyExprEngine>;
    PyExprEngine();
};
} // namespace xexprengine