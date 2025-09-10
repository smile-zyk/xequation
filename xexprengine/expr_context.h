#pragma once
#include "expr_common.h"
#include "value.h"
#include "dependency_graph.h"
#include "variable.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace xexprengine
{
class ExprEngine;

class ExprContext
{
  public:
    ExprContext(const std::string& name);

    Value GetValue(const Variable* var) const;
    Variable* GetVariable(const std::string &var_name) const;

    bool AddVariable(std::unique_ptr<Variable> var);
    bool AddVariables(std::vector<std::unique_ptr<Variable>> var_list);

    bool SetVariable(const std::string& var_name, const Value& value);
    bool SetVariable(const std::string& var_name, const std::string& expression);

    bool RemoveVariable(const std::string &var_name);
    bool RemoveVariable(Variable* var);
    bool RemoveVariables(const std::vector<std::string> &var_name_list);

    bool RenameVariable(const std::string &old_name, const std::string &new_name);
    
    bool SetVariableDirty(const std::string &var_name, bool dirty);
    bool SetVariableDirty(Variable* var, bool dirty);

    bool IsVariableExist(const std::string &var_name) const;

    void Reset();
  
    void Update();

    void ParseVariableDependency(Variable* var);
    
    EvalResult Evaluate(const std::string &expr) const;

    ParseResult Parse(const std::string &expr) const;

    DependencyGraph* graph()
    {
        return graph_.get();
    }

    const std::string &name() const
    {
        return name_;
    }

  private:
    std::unique_ptr<DependencyGraph> graph_;
    std::unordered_map<std::string, std::unique_ptr<Variable>> variable_map_;
    std::string name_;
    ExprEngine *engine_ = nullptr;
};
} // namespace xexprengine