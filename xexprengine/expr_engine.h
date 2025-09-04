#pragma once
#include "expr_common.h"
#include "expr_context.h"
#include "value.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace xexprengine
{
class ExprEngine
{
  public:
    ExprEngine() = default;
    ~ExprEngine() = default;

    virtual EvalResult Evaluate(const std::string &expr, ExprContext *context = nullptr) = 0;
    virtual AnalyzeResult Analyze(const std::string &expr) = 0;

    void RegisterContext(const std::string &name, std::unique_ptr<ExprContext> context);
    void SetCurrentContext(ExprContext *context);
    void SetCurrentContext(const std::string &name);
    void RemoveContext(ExprContext *context);
    void RemoveContext(const std::string &name);

    ExprContext *GetCurrentContext() const;
    ExprContext *GetContext(const std::string &name) const;

  protected:
    virtual void SetVariable(const std::string &var_name, const Value &value, const ExprContext *context) = 0;
    virtual Value GetVariable(const std::string &var_name, const ExprContext *context) = 0;
    virtual void RemoveVariable(const std::string &var_name, const ExprContext *context) = 0;
    virtual void RenameVariable(const std::string &old_name, const std::string &new_name, const ExprContext *context) = 0;
    friend class ExprContext;

  private:
    std::unordered_map<std::string, std::unique_ptr<ExprContext>> context_map_;
    ExprContext *current_context_ = nullptr;
};
} // namespace xexprengine