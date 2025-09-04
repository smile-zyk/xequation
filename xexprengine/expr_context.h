#pragma once
#include "expr_common.h"
#include "value.h"
#include "variable_dependency_graph.h"

namespace xexprengine
{
class ExprEngine;

class ExprContext
{
  public:
    ExprContext() = default;

    Value GetValue(const Variable* var) const;
    Variable* GetVariable(const std::string &var_name) const;

    void SetVariable(const std::string &var_name, std::unique_ptr<Variable> var);
    void SetRawVariable(const std::string& var_name, std::unique_ptr<RawVariable> var);
    void SetRawVariable(const std::string& var_name, const Value& value);
    void SetExprVariable(const std::string& var_name, std::unique_ptr<ExprVariable> var);
    void SetExprVariable(const std::string& var_name, const std::string& expression);

    void RemoveVariable(const std::string &var_name);
    void RenameVariable(const std::string &old_name, const std::string &new_name);

    bool IsVariableExist(const std::string &var_name) const;

    void UpdateVariableGraph();

    EvalResult Evaluate(const std::string &expr) const;

    VariableDependencyGraph* graph()
    {
        return graph_.get();
    }

    const std::string &name() const
    {
        return name_;
    }
    
  private:
    std::unique_ptr<VariableDependencyGraph> graph_;
    std::string name_;
    ExprEngine *engine_ = nullptr;
};
} // namespace xexprengine