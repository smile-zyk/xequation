#pragma once
#include <memory>
#include <string>
#include <map>

#include "dependency_graph.h"
#include "expr_common.h"
#include "expr_context.h"
#include "variable.h"

namespace xexprengine
{
class VariableManager
{
  public:
    VariableManager(std::unique_ptr<ExprContext> context, EvalCallback eval_callback = nullptr, ParseCallback parse_callback = nullptr) noexcept;
    virtual ~VariableManager() noexcept = default;

    // get variable
    const Variable *GetVariable(const std::string &var_name) const;
    Variable *GetVariable(const std::string &var_name);

    // add variable
    bool AddVariable(std::unique_ptr<Variable> var);
    bool AddVariables(std::vector<std::unique_ptr<Variable>> var_list);

    // set variable : if variable not exist , add variable, otherwise overwrite variable
    void SetValue(const std::string &var_name, const Value &value);
    void SetExpression(const std::string &var_name, const std::string &expression);
    bool SetVariable(const std::string &var_name, std::unique_ptr<Variable> variable);

    // remove variable
    bool RemoveVariable(const std::string &var_name) noexcept;
    bool RemoveVariables(const std::vector<std::string> &var_name_list) noexcept;

    // rename variable
    bool RenameVariable(const std::string &old_name, const std::string &new_name);

    // check both graph node and variable exist
    bool IsVariableExist(const std::string &var_name) const;

    // clear graph and variable map
    void Reset();

    // use graph update all variable to context
    void Update();

    // use graph update single varible and its dependents to context
    bool UpdateVariable(const std::string& var_name);

    const DependencyGraph *graph()
    {
        return graph_.get();
    }

    const ExprContext* context()
    {
        return context_.get();
    }

  private:
    VariableManager(const VariableManager &) = delete;
    VariableManager &operator=(const VariableManager &) = delete;

    VariableManager(VariableManager &&) noexcept = delete;
    VariableManager &operator=(VariableManager &&) noexcept = delete;

    // update value to context and update statue to variable
    bool UpdateVariableInternal(const std::string &var_name);
    
    // update status when add variable to variable_map
    void UpdateVariableStatus(Variable* var);

    // for batch update
    bool AddVariableToGraph(const Variable* var);
    bool RemoveVariableToGraph(const std::string& var_name) noexcept;

    // graph helper functions
    bool CheckNodeDependenciesComplete(const std::string& node_name, std::vector<std::string>& missing_dependencies) const;
    bool UpdateNodeDependencies(const std::string& node_name, const std::unordered_set<std::string>& node_dependencies);

    // for remove obsolete variable in context
    void RemoveObsoleteVariablesInContext();

  private:
    std::unique_ptr<DependencyGraph> graph_;
    std::unique_ptr<ExprContext> context_;
    std::unordered_map<std::string, std::unique_ptr<Variable>> variable_map_;
    EvalCallback evaluate_callback_;
    ParseCallback parse_callback_;
};
} // namespace xexprengine