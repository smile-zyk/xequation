#pragma once
#include "expr_common.h"
#include "value.h"
#include "variable_dependency_graph.h"
#include <string>

namespace xexprengine
{
class ExprEngine;

class ExprContext
{
  public:
    ExprContext(const std::string& name);

    Value GetValue(const Variable* var) const;
    Variable* GetVariable(const std::string &var_name) const;

    bool SetVariable(std::unique_ptr<Variable> var);
    bool SetVariables(const std::vector<std::unique_ptr<Variable>> &var_list);
    bool SetRawVariable(std::unique_ptr<RawVariable> var);
    bool SetRawVariable(const std::string& var_name, const Value& value);
    bool SetExprVariable(std::unique_ptr<ExprVariable> var);
    bool SetExprVariable(const std::string& var_name, const std::string& expression);

    bool RemoveVariable(const std::string &var_name);
    bool RemoveVariable(Variable* var);
    bool RemoveVariables(const std::vector<std::string> &var_name_list);
    bool RenameVariable(const std::string &old_name, const std::string &new_name);
    bool SetVariableDirty(const std::string &var_name, bool dirty);
    bool SetVariableDirty(Variable* var, bool dirty);

    bool IsVariableExist(const std::string &var_name) const;
    bool IsVariableDirty(const std::string &var_name) const;

    void Update();

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