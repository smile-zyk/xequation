#pragma once
#include "core/expr_context.h"
#include <pybind11/pybind11.h>

namespace xexprengine
{
class PyExprContext : public ExprContext
{
  public:
    PyExprContext();
    ~PyExprContext() = default;
    Value GetContextValue(const std::string &var_name) const override;
    bool IsContextValueExist(const std::string &var_name) const override;
    std::unordered_set<std::string> GetContextExistVariables() const override;
  private:
    void SetContextValue(const std::string &var_name, const Value &value) override;
    bool RemoveContextValue(const std::string &var_name) override;
    void ClearContextValue() override;
    pybind11::dict context_dict_;
};
} // namespace xexprengine