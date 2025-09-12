#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "expr_common.h"
#include "value.h"
#include "dependency_graph.h"
#include "variable.h"

namespace xexprengine
{
class ExprEngine;

class ExprContext
{
  public:
    ExprContext();
    ~ExprContext() = default;

    ExprContext(const ExprContext &) = delete;
    ExprContext &operator=(const ExprContext &) = delete;

    ExprContext(ExprContext &&) = default;
    ExprContext &operator=(ExprContext &&) = default;

    Variable* GetVariable(const std::string &var_name) const;

    // add variable
    bool AddVariable(std::unique_ptr<Variable> var);
    bool AddVariables(std::vector<std::unique_ptr<Variable>> var_list);

    // set variable : if variable not exist , add variable, otherwise overwrite variable
    void SetVariable(const std::string& var_name, const Value& value);
    void SetVariable(const std::string& var_name, const std::string& expression);
    void SetVariable(const std::string& var_name, std::unique_ptr<Variable> variable);

    // remove variable
    bool RemoveVariable(const std::string &var_name);
    bool RemoveVariables(const std::vector<std::string> &var_name_list);

    // rename variable
    bool RenameVariable(const std::string &old_name, const std::string &new_name);

    // call when exprvariable update expression
    bool UpdateVariableDependencies(const std::string &var_name);

    // check both graph node and variable exist
    bool IsVariableExist(const std::string &var_name) const;

    // clear graph and variable map
    void Reset();

    // update variable to context
    bool UpdateVariable(const std::string& var_name);

    // use graph update all variable to context
    void Update();

    // interface to set/get real context
    virtual Value GetContextValue(const std::string &var_name) const = 0;
    virtual void SetContextValue(const std::string &var_name, const Value &value) = 0;
    virtual bool RemoveContextValue(const std::string& var_name) = 0;

    const DependencyGraph* graph()
    {
        return graph_.get();
    }

  private:
    std::unique_ptr<DependencyGraph> graph_;
    std::unordered_map<std::string, std::unique_ptr<Variable>> variable_map_;
    std::function<EvalResult(const std::string&,const ExprContext*)> evaluate_callback_;
    std::function<ParseResult(const std::string&)> parse_callback_;
};
} // namespace xexprengine