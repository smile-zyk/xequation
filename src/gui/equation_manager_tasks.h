#pragma once

#include "core/equation_common.h"
#include "core/equation_manager.h"
#include "task/task.h"

#include <QSize>

namespace xequation
{
namespace gui
{
class EquationManagerTask : public Task
{
    Q_OBJECT
  public:
    EquationManagerTask(const QString &title, EquationManager *manager) : Task(title), equation_manager_(manager) {}
    ~EquationManagerTask() override = default;

    virtual void Execute() override;
    virtual void RequestCancel() override;
    virtual void Cleanup() override;
    EquationManager *equation_manager() const
    {
        return equation_manager_;
    }

  private:
    EquationManager *equation_manager_;
};

class UpdateEquationGroupTask : public EquationManagerTask
{
    Q_OBJECT
  public:
    UpdateEquationGroupTask(const QString &title, EquationManager *manager, EquationGroupId group_id)
        : EquationManagerTask(title, manager), group_id_(group_id)
    {
    }
    ~UpdateEquationGroupTask() override = default;

    void Execute() override;

  private:
    EquationGroupId group_id_;
};

class UpdateManagerTask : public EquationManagerTask
{
    Q_OBJECT
  public:
    UpdateManagerTask(const QString &title, EquationManager *manager) : EquationManagerTask(title, manager) {}
    ~UpdateManagerTask() override = default;

    void Execute() override;
};

class UpdateEquationsTask : public EquationManagerTask
{
    Q_OBJECT
  public:
    UpdateEquationsTask(
        const QString &title, EquationManager *manager, const std::vector<std::string> &update_equations
    )
        : EquationManagerTask(title, manager), update_equations_(update_equations)
    {
    }
    ~UpdateEquationsTask() override = default;

    void Execute() override;

  private:
    std::vector<std::string> update_equations_;
};

class EvalExpressionTask : public EquationManagerTask
{
    Q_OBJECT
  public:
    EvalExpressionTask(const QString &title, EquationManager *manager, const std::string &expression);
    ~EvalExpressionTask() override = default;

    void Execute() override;

  signals:
    void EvalCompleted(InterpretResult result);

  private:
    std::string expression_;
    InterpretResult result_;
};

class ExecStatementTask : public EquationManagerTask
{
    Q_OBJECT
  public:
    ExecStatementTask(const QString &title, EquationManager *manager, const std::string &statement);
    ~ExecStatementTask() override = default;

    void Execute() override;

  signals:
    void ExecCompleted(InterpretResult result);

  private:
    std::string statement_;
    InterpretResult result_;
};

class EquationDependencyGraphGenerationTask : public EquationManagerTask
{
    Q_OBJECT
  public:
    EquationDependencyGraphGenerationTask(const QString &title, EquationManager *manager);
    ~EquationDependencyGraphGenerationTask() override = default;

    void Execute() override;

  signals:
    void DependencyGraphImageGenerated(const QString &image_path);

  private:
    QString image_path_;
};

} // namespace gui
} // namespace xequation