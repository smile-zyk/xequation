#pragma once
#include "expr_common.h"
#include "expr_context.h"
#include <string>

namespace xexprengine
{
class ExprEngine
{
  public:
    ExprEngine() = default;
    ~ExprEngine() = default;

    virtual EvalResult Evaluate(const std::string &expr, const ExprContext *context = nullptr) = 0;
    virtual ParseResult Parse(const std::string &expr) = 0;
};
} // namespace xexprengine