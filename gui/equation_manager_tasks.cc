#include "equation_manager_tasks.h"
#include "core/equation_common.h"
#include "python/python_qt_wrapper.h"
#include <QThread>

namespace xequation
{
namespace gui
{
void EquationManagerTask::Execute()
{
    if (equation_manager_->language() == "Python")
    {
        pybind11::gil_scoped_acquire acquire;
        PyThreadState *py_thread_state = PyThreadState_Get();
        internal_data_ = static_cast<void *>(py_thread_state);
    }
}

void EquationManagerTask::RequestCancel()
{
    Task::RequestCancel();
    if (equation_manager_->language() == "Python" && internal_data_)
    {
        pybind11::gil_scoped_acquire acquire;
        void *data = internal_data_;
        PyThreadState *py_thread_state = static_cast<PyThreadState *>(data);
        PyThreadState_SetAsyncExc(py_thread_state->thread_id, PyExc_KeyboardInterrupt);
    }
}

void EquationManagerTask::Cleanup()
{
    if (equation_manager_->language() == "Python")
    {
        internal_data_ = nullptr;
    }
}

void UpdateEquationGroupTask::Execute()
{
    EquationManagerTask::Execute();
    SetProgress(5, "Starting update of equation group...");
    auto manager = equation_manager();
    // get the equations in the group before updating
    auto equation_names = manager->GetEquationGroup(group_id_)->GetEquationNames();
    auto update_equation_names = manager->graph().TopologicalSort(equation_names);

    SetProgress(10, "Updating equations in the group...");

    for (size_t i = 0; i < update_equation_names.size(); ++i)
    {
        if (cancel_requested_.load())
        {
            manager->UpdateEquationStatus(update_equation_names[i], ResultStatus::kKeyBoardInterrupt);
            continue;
        }
        int progress = 10 + static_cast<int>(80.0 * i / update_equation_names.size());
        SetProgress(progress, "Updating equation: " + QString::fromStdString(update_equation_names[i]));
        manager->UpdateEquationWithoutPropagate(update_equation_names[i]);
        // release GIL for main thread to update UI
        QThread::msleep(200);
    }
    if (cancel_requested_.load())
    {
        return;
    }
    SetProgress(100, "Update completed.");
}

void UpdateManagerTask::Execute()
{
    EquationManagerTask::Execute();
    SetProgress(5, "Starting full update...");
    auto manager = equation_manager();
    auto update_equation_names = manager->graph().TopologicalSort();

    SetProgress(10, "Updating equations...");

    for (size_t i = 0; i < update_equation_names.size(); ++i)
    {
        if (cancel_requested_.load())
        {
            manager->UpdateEquationStatus(update_equation_names[i], ResultStatus::kKeyBoardInterrupt);
            continue;
        }
        int progress = 10 + static_cast<int>(80.0 * i / update_equation_names.size());
        SetProgress(progress, "Updating equation: " + QString::fromStdString(update_equation_names[i]));
        manager->UpdateEquationWithoutPropagate(update_equation_names[i]);
        // release GIL for main thread to update UI
        QThread::msleep(200);
    }
    if (cancel_requested_.load())
    {
        return;
    }
    SetProgress(100, "Full update completed.");
}

void UpdateEquationsTask::Execute()
{
    EquationManagerTask::Execute();
    SetProgress(5, "Starting update of equations...");
    auto manager = equation_manager();
    auto update_equation_names = manager->graph().TopologicalSort(update_equations_);

    SetProgress(10, "Updating equations...");

    for (size_t i = 0; i < update_equation_names.size(); ++i)
    {
        if (cancel_requested_.load())
        {
            manager->UpdateEquationStatus(update_equation_names[i], ResultStatus::kKeyBoardInterrupt);
            continue;
        }
        int progress = 10 + static_cast<int>(80.0 * i / update_equation_names.size());
        SetProgress(progress, "Updating equation: " + QString::fromStdString(update_equation_names[i]));
        manager->UpdateEquationWithoutPropagate(update_equation_names[i]);
        // release GIL for main thread to update UI
        QThread::msleep(200);
    }
    if (cancel_requested_.load())
    {
        return;
    }
    SetProgress(100, "Update completed.");
}

EvalExpressionTask::EvalExpressionTask(const QString &title, EquationManager *manager, const std::string &expression)
    : EquationManagerTask(title, manager), expression_(expression)
{
    connect(this, &Task::Finished, this, [this](QUuid id) { emit EvalCompleted(result_); });
}

void EvalExpressionTask::Execute()
{
    EquationManagerTask::Execute();
    SetProgress(5, "Starting evaluation of expression...");
    auto manager = equation_manager();

    SetProgress(10, "Evaluating expression...");
    result_ = manager->Eval(expression_);
    if (cancel_requested_.load())
    {
        return;
    }
    SetProgress(100, "Evaluation completed.");
}
} // namespace gui
} // namespace xequation