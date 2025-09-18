#pragma once
#include "expr_common.h"
#include "expr_context.h"
#include <string>

namespace xexprengine
{
template<typename T>
class ExprEngine
{
  public:
    ExprEngine(const ExprEngine&) = delete;
    ExprEngine(ExprEngine&&) = delete;
    ExprEngine& operator=(const ExprEngine&) = delete;
    ExprEngine& operator=(ExprEngine&&) = delete;

    static T GetInstance()
    {
      static T instance;
      return instance;
    }

    virtual EvalResult Evaluate(const std::string &expr, const ExprContext *context = nullptr) = 0;
    virtual ParseResult Parse(const std::string &expr) = 0;
  protected:
    ExprEngine() = default;
    ~ExprEngine() = default;
  };
} // namespace xexprengine