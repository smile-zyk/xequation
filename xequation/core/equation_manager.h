#pragma once
#include <memory>
#include <string>

#include "dependency_graph.h"
#include "expr_common.h"
#include "expr_context.h"
#include "equation.h"

namespace xequation
{
class EquationManager
{
  public:
    EquationManager(std::unique_ptr<ExprContext> context, ExecCallback exec_callback, ParseCallback parse_callback, EvalCallback eval_callback = nullptr) noexcept;
    virtual ~EquationManager() noexcept = default;

    const Equation* GetEquation(const std::string &eqn_name) const;

    void SetEquation(const std::string &eqn_name, const std::string &expression);

    void RemoveEquation(const std::string &eqn_name) noexcept;

    void AddEquationStatement(const std::string &equation_code);

    void EditEquationStatement(const std::string& old_equation_code, const std::string& equation_code);

    void RemoveEquationStatement(const std::string &equation_code) noexcept;

    bool IsEquationExist(const std::string &eqn_name) const;

    std::vector<Equation> GetEquations(const std::string &equation_code) const;

    void Reset();

    void Update();

    void UpdateEquation(const std::string& eqn_name);

    const DependencyGraph *graph()
    {
        return graph_.get();
    }

    const ExprContext* context()
    {
        return context_.get();
    }

  private:
    EquationManager(const EquationManager &) = delete;
    EquationManager &operator=(const EquationManager &) = delete;

    EquationManager(EquationManager &&) noexcept = delete;
    EquationManager &operator=(EquationManager &&) noexcept = delete;

    void UpdateEquationInternal(const std::string &var_name);

    void AddNodeToGraph(const std::string& node_name, const std::vector<std::string>& dependencies);
    void RemoveNodeInGraph(const std::string& node_name);

  private:
    std::unique_ptr<DependencyGraph> graph_;
    std::unique_ptr<ExprContext> context_;
    std::unordered_map<std::string, Equation> equation_map_;
    ExecCallback exec_callback_;
    ParseCallback parse_callback_;
    EvalCallback eval_callback_;
};
} // namespace xequation