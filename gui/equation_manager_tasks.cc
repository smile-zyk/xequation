#include "equation_manager_tasks.h"
#include "core/equation_common.h"
#include "python/python_qt_wrapper.h"
#include <QDir>
#include <QProcess>
#include <QThread>
#include <QFont>
#include <QFontDatabase>


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

EquationDependencyGraphGenerationTask::EquationDependencyGraphGenerationTask(
    const QString &title, EquationManager *manager
)
    : EquationManagerTask(title, manager)
{
    connect(this, &Task::Finished, this, [this](QUuid id) { emit DependencyGraphImageGenerated(image_path_); });
}

void EquationDependencyGraphGenerationTask::Execute()
{
    EquationManagerTask::Execute();
    SetProgress(0, "Starting dependency graph generation...");
    QDir temp_dir("temp");
    if (!temp_dir.exists())
    {
        temp_dir.mkpath(".");
    }
    SetProgress(10, "Generating dependency graph DOT file...");
    // Generate SVG using graphviz dot command
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString dot_file = QString("temp/%1_dependency.dot").arg(timestamp);
    QString svg_file = QString("temp/%1_dependency.svg").arg(timestamp);
    bool result = equation_manager()->WriteDependencyGraphToDotFile(dot_file.toStdString());
    if (!result)
    {
        SetProgress(0, "Failed to generate DOT file.");
        return;
    }

    // clear old temp files, keep only last 10 files
    QStringList dot_files = temp_dir.entryList(QStringList() << "*_dependency.dot", QDir::Files, QDir::Name);
    QStringList svg_files = temp_dir.entryList(QStringList() << "*_dependency.svg", QDir::Files, QDir::Name);
    QStringList png_files = temp_dir.entryList(QStringList() << "*_dependency.png", QDir::Files, QDir::Name);

    if (dot_files.size() > 10)
    {
        for (int i = 0; i < dot_files.size() - 10; ++i)
        {
            temp_dir.remove(dot_files[i]);
        }
    }

    if (svg_files.size() > 10)
    {
        for (int i = 0; i < svg_files.size() - 10; ++i)
        {
            temp_dir.remove(svg_files[i]);
        }
    }

    if (png_files.size() > 10)
    {
        for (int i = 0; i < png_files.size() - 10; ++i)
        {
            temp_dir.remove(png_files[i]);
        }
    }

    SetProgress(60, "Converting DOT file to SVG...");
    QProcess process;
    
    // Get fixed-width font for better readability
    QFont fixed_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    QString font_name = fixed_font.family();
    
    QStringList args;
    args << "-Tsvg"
         << "-Gfontname=" + font_name     // Set graph font
         << "-Nfontname=" + font_name     // Set node font
         << "-Efontname=" + font_name     // Set edge font
         << dot_file 
         << "-o" 
         << svg_file;
    
    process.start("dot", args);
    if (!process.waitForFinished(5000))
    {
        SetProgress(60, "Failed to generate dependency graph: Graphviz timeout or not installed.");
        return;
    }
    if (process.exitCode() != 0)
    {
        QString error = process.readAllStandardError();
        SetProgress(60, "Failed to generate dependency graph");
        return;
    }
    image_path_ = svg_file;
    SetProgress(100, "Successfully generated dependency graph");
}
} // namespace gui
} // namespace xequation