#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "dependency_graph.h"
#include "expr_common.h"
#include "value.h"
#include "variable.h"

namespace xexprengine
{
class ExprContext
{
  public:
    ExprContext() noexcept;
    ~ExprContext() noexcept = default;

    ExprContext(const ExprContext &) = delete;
    ExprContext &operator=(const ExprContext &) = delete;

    ExprContext(ExprContext &&) noexcept = default;
    ExprContext &operator=(ExprContext &&) noexcept = default;

    Variable *GetVariable(const std::string &var_name) const;

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

    virtual Value GetContextValue(const std::string &var_name) const = 0;
    virtual bool IsContextValueExist(const std::string &var_name) const = 0;
    virtual std::unordered_set<std::string> GetContextExistVariables() const = 0;

    const DependencyGraph *graph()
    {
        return graph_.get();
    }

  protected:
    virtual void SetContextValue(const std::string &var_name, const Value &value) = 0;
    virtual bool RemoveContextValue(const std::string &var_name) = 0;
    virtual void ClearContextValue() = 0;
    
    bool UpdateVariable(const std::string &var_name);
    void UpdateVariableParseStatus(Variable* var);
    bool AddVariableToGraph(const Variable* var);
    bool RemoveVariableToGraph(const std::string& var_name) noexcept;
    bool AddVariableToMap(std::unique_ptr<Variable> var);

    // graph helper functions
    bool CheckNodeDependenciesComplete(const std::string& node_name, std::vector<std::string>& missing_dependencies) const;
    bool UpdateNodeDependencies(const std::string& node_name, const std::unordered_set<std::string>& node_dependencies);

    void set_evaluate_callback(std::function<EvalResult(const std::string &)> callback)
    {
        evaluate_callback_ = callback;
    }
    void set_parse_callback(std::function<ParseResult(const std::string &)> callback)
    {
        parse_callback_ = callback;
    }

    private:
    std::unique_ptr<DependencyGraph> graph_;
    std::unordered_map<std::string, std::unique_ptr<Variable>> variable_map_;
    std::unordered_map<std::string, ParseResult> parse_cached_map_;
    std::function<EvalResult(const std::string &)> evaluate_callback_;
    std::function<ParseResult(const std::string &)> parse_callback_;
};
} // namespace xexprengine