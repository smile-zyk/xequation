#pragma once
#include <exception>
#include <memory>
#include <string>

#include "dependency_graph.h"
#include "equation.h"
#include "equation_common.h"
#include "equation_context.h"


namespace xequation
{

class DuplicateEquationNameError : public std::exception
{
  private:
    std::string exist_equation_name_;
    std::string exist_equation_content_;

  public:
    DuplicateEquationNameError(const std::string &eqn_name, const std::string eqn_content)
        : exist_equation_name_(eqn_name), exist_equation_content_(eqn_content)
    {
    }

    const char *what() const noexcept override
    {
        static std::string res;
        res = exist_equation_name_ + " is exist! exist equation content: " + exist_equation_content_;
        return res.c_str();
    }

    const std::string &exist_equation_name() const
    {
        return exist_equation_name_;
    }
    const std::string &exist_equation_content() const
    {
        return exist_equation_content_;
    }
};

class EquationManager
{
  public:
    EquationManager(
        std::unique_ptr<EquationContext> context, ExecCallback exec_callback, ParseCallback parse_callback,
        EvalCallback eval_callback = nullptr
    ) noexcept;
    virtual ~EquationManager() noexcept = default;

    const Equation *GetEquation(const std::string &eqn_name) const;

    void AddEquation(const std::string &eqn_name, const std::string &expression);

    void EditEquation(const std::string &old_eqn_name, const std::string& new_eqn_name, const std::string &new_eqn_expr);

    void RemoveEquation(const std::string &eqn_name);

    void AddMultipleEquations(const std::string& eqn_code);

    void EditMultipleEquations(const std::string& old_eqn_code, const std::string& new_eqn_code);

    void RemoveMultipleEquations(const std::string& eqn_code);

    bool IsEquationExist(const std::string &eqn_name) const;

    EvalResult Eval(const std::string& expression) const;

    void Reset();

    void Update();

    void UpdateEquation(const std::string &eqn_name);

    void UpdateMultipleEquations(const std::string &eqn_code);

    const DependencyGraph *graph()
    {
        return graph_.get();
    }

    const EquationContext *context()
    {
        return context_.get();
    }

  private:
    EquationManager(const EquationManager &) = delete;
    EquationManager &operator=(const EquationManager &) = delete;

    EquationManager(EquationManager &&) noexcept = delete;
    EquationManager &operator=(EquationManager &&) noexcept = delete;

    void AddEquationStatement(const std::string &eqn_code);
    void EditEquationStatement(const std::string &old_eqn_code, const std::string &equation_code);
    void RemoveEquationStatement(const std::string &eqn_code);

    void UpdateEquationInternal(const std::string &var_name);

    void AddNodeToGraph(const std::string &node_name, const std::vector<std::string> &dependencies);
    void RemoveNodeInGraph(const std::string &node_name);

  private:
    std::unique_ptr<DependencyGraph> graph_;
    std::unique_ptr<EquationContext> context_;
    std::unordered_map<std::string, Equation> equation_map_;
    std::unordered_set<std::string> single_variable_equation_name_set_;
    std::unordered_set<std::string> multiple_variable_equation_code_set_;
    ExecCallback exec_callback_;
    ParseCallback parse_callback_;
    EvalCallback eval_callback_;
};
} // namespace xequation