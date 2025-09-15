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
    void SetValue(const std::string& var_name, const Value& value);
    void SetExpression(const std::string& var_name, const std::string& expression);
    void SetVariable(const std::string& var_name, std::unique_ptr<Variable> variable);

    // remove variable
    bool RemoveVariable(const std::string &var_name);
    bool RemoveVariables(const std::vector<std::string> &var_name_list);

    // rename variable
    bool RenameVariable(const std::string &old_name, const std::string &new_name);

    // check both graph node and variable exist
    bool IsVariableExist(const std::string &var_name) const;

    // clear graph and variable map
    void Reset();

    // use graph update all variable to context
    void Update();

    virtual Value GetContextValue(const std::string &var_name) const = 0;
    virtual bool IsContextValueExist(const std::string &var_name) const = 0;

    const DependencyGraph* graph()
    {
        return graph_.get();
    }

  protected:
    virtual void SetContextValue(const std::string &var_name, const Value &value) = 0;
    virtual bool RemoveContextValue(const std::string& var_name) = 0;
    virtual void ClearContextValue() = 0;
    
    void set_evaluate_callback(std::function<EvalResult(const std::string&,const ExprContext*)> callback)
    {
        evaluate_callback_ = callback;
    }

    void set_parse_callback(std::function<ParseResult(const std::string&)> callback)
    {
        parse_callback_ = callback;
    }

    // update variable to context
    bool UpdateVariable(const std::string& var_name);
    // call when exprvariable update expression
    bool UpdateVariableDependencies(const std::string &var_name);
    bool IsVariableDependencyEntire(const std::string &var_name, std::vector<std::string>& missing_dependencies) const;
  private:
    std::unique_ptr<DependencyGraph> graph_;
    std::unordered_map<std::string, std::unique_ptr<Variable>> variable_map_;
    std::function<EvalResult(const std::string&,const ExprContext*)> evaluate_callback_;
    std::function<ParseResult(const std::string&)> parse_callback_;
};
} // namespace xexprengine