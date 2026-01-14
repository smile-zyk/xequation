#include "demo_widget.h"
#include "core/equation_common.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QStatusBar>
#include <QSvgWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrent/QtConcurrent>

#include "equation_editor.h"
#include "equation_group_editor.h"
#include "equation_manager_tasks.h"
#include "equation_signals_qt_utils.h"
#include "python/python_qt_wrapper.h"

using namespace xequation;

DemoWidget::DemoWidget(QWidget *parent)
    : QMainWindow(parent),
      equation_browser_widget_(nullptr),
      variable_inspect_widget_(nullptr),
      expression_watch_widget_(nullptr)
{
    equation_manager_ = xequation::python::PythonEquationEngine::GetInstance().CreateEquationManager();
    mock_equation_list_widget_ = new MockEquationGroupListWidget(equation_manager_.get(), this);
    equation_browser_widget_ = new xequation::gui::EquationBrowserWidget(this);
    variable_inspect_widget_ = new xequation::gui::VariableInspectWidget(this);

    equation_completion_model_ =
        new xequation::gui::EquationCompletionModel(QString::fromStdString(equation_manager_->language()), this);

    expression_watch_widget_ = new xequation::gui::ExpressionWatchWidget(equation_completion_model_, this);

    dependency_graph_viewer_ = new xequation::gui::EquationDependencyGraphViewer(this);

    task_manager_ = new xequation::gui::ToastTaskManager(this, 1);

    SetupUI();
    SetupConnections();
}

DemoWidget::~DemoWidget()
{
    delete task_manager_;
    task_manager_ = nullptr;
}

void DemoWidget::SetupUI()
{
    setWindowTitle("xequation demo");
    setFixedSize(800, 600);

    setCentralWidget(mock_equation_list_widget_);

    CreateActions();
    CreateMenus();
    InitCompletionModel();
}

void DemoWidget::SetupConnections()
{
    connect(open_action_, &QAction::triggered, this, &DemoWidget::OnOpen);
    connect(exit_action_, &QAction::triggered, qApp, &QApplication::quit);
    connect(insert_equation_action_, &QAction::triggered, this, &DemoWidget::OnInsertEquationRequest);
    connect(update_all_action_, &QAction::triggered, this, &DemoWidget::AsyncUpdateManager);
    connect(show_dependency_graph_action_, &QAction::triggered, this, &DemoWidget::OnShowDependencyGraph);
    connect(show_equation_manager_action_, &QAction::triggered, this, &DemoWidget::OnShowEquationManager);
    connect(show_variable_inspector_action_, &QAction::triggered, this, &DemoWidget::OnShowEquationInspector);
    connect(show_expression_watch_action_, &QAction::triggered, this, &DemoWidget::OnShowExpressionWatch);
    connect(
        variable_inspect_widget_, &xequation::gui::VariableInspectWidget::AddExpressionToWatch,
        expression_watch_widget_, &xequation::gui::ExpressionWatchWidget::OnAddExpressionToWatch
    );

    connect(
        expression_watch_widget_, &xequation::gui::ExpressionWatchWidget::ParseResultRequested, this,
        &DemoWidget::OnParseResultRequested
    );

    connect(
        expression_watch_widget_, &xequation::gui::ExpressionWatchWidget::EvalResultAsyncRequested, this,
        &DemoWidget::OnEvalResultAsyncRequested
    );

    connect(
        dependency_graph_viewer_, &xequation::gui::EquationDependencyGraphViewer::DependencyGraphImageRequested, this,
        &DemoWidget::OnEquationDependencyGraphImageRequested
    );

    connect(
        mock_equation_list_widget_, &MockEquationGroupListWidget::EditEquationGroupRequested, this,
        &DemoWidget::OnEditEquationGroupRequest
    );

    connect(
        mock_equation_list_widget_, &MockEquationGroupListWidget::RemoveEquationGroupRequested, this,
        &DemoWidget::OnRemoveEquationGroupRequest
    );

    connect(
        mock_equation_list_widget_, &MockEquationGroupListWidget::CopyEquationGroupRequested, this,
        &DemoWidget::OnCopyEquationGroupRequest
    );

    connect(
        mock_equation_list_widget_, &MockEquationGroupListWidget::EquationGroupSelected, this,
        &DemoWidget::OnEquationGroupSelected
    );

    connect(
        mock_equation_list_widget_, &MockEquationGroupListWidget::UpdateEquationGroupRequested, this,
        &DemoWidget::OnUpdateEquationGroupRequest
    );

    connect(
        mock_equation_list_widget_, &MockEquationGroupListWidget::AddEquationGroupToExpressionWatchRequested, this,
        &DemoWidget::OnAddEquationGroupToExpressionWatchRequest
    );

    connect(
        equation_browser_widget_, &xequation::gui::EquationBrowserWidget::EquationSelected, this,
        &DemoWidget::OnEquationSelected
    );

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationAdded>(
        &equation_manager_->signals_manager(), equation_browser_widget_,
        &xequation::gui::EquationBrowserWidget::OnEquationAdded
    );

    xequation::gui::ConnectEquationSignalDirect<EquationEvent::kEquationRemoving>(
        &equation_manager_->signals_manager(), equation_browser_widget_,
        &xequation::gui::EquationBrowserWidget::OnEquationRemoving
    );

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationUpdated>(
        &equation_manager_->signals_manager(), equation_browser_widget_,
        &xequation::gui::EquationBrowserWidget::OnEquationUpdated
    );

    xequation::gui::ConnectEquationSignalDirect<EquationEvent::kEquationRemoving>(
        &equation_manager_->signals_manager(), variable_inspect_widget_,
        &xequation::gui::VariableInspectWidget::OnEquationRemoving
    );

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationUpdated>(
        &equation_manager_->signals_manager(), variable_inspect_widget_,
        &xequation::gui::VariableInspectWidget::OnEquationUpdated
    );

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationRemoved>(
        &equation_manager_->signals_manager(), expression_watch_widget_,
        &xequation::gui::ExpressionWatchWidget::OnEquationRemoved
    );

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationUpdated>(
        &equation_manager_->signals_manager(), expression_watch_widget_,
        &xequation::gui::ExpressionWatchWidget::OnEquationUpdated
    );

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationAdded>(
        &equation_manager_->signals_manager(), equation_completion_model_,
        &xequation::gui::EquationCompletionModel::OnEquationAdded
    );

    xequation::gui::ConnectEquationSignalDirect<EquationEvent::kEquationRemoving>(
        &equation_manager_->signals_manager(), equation_completion_model_,
        &xequation::gui::EquationCompletionModel::OnEquationRemoving
    );

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationGroupAdded>(
        &equation_manager_->signals_manager(), dependency_graph_viewer_,
        &xequation::gui::EquationDependencyGraphViewer::OnEquationGroupAdded
    );

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationGroupUpdated>(
        &equation_manager_->signals_manager(), dependency_graph_viewer_,
        &xequation::gui::EquationDependencyGraphViewer::OnEquationGroupUpdated
    );

    xequation::gui::ConnectEquationSignalDirect<EquationEvent::kEquationGroupRemoving>(
        &equation_manager_->signals_manager(), dependency_graph_viewer_,
        &xequation::gui::EquationDependencyGraphViewer::OnEquationGroupRemoving
    );
}

void DemoWidget::CreateActions()
{
    // File menu actions
    open_action_ = new QAction("&Open", this);
    open_action_->setShortcut(QKeySequence::Open);
    open_action_->setStatusTip("Open an existing file");

    exit_action_ = new QAction("E&xit", this);
    exit_action_->setShortcut(QKeySequence::Quit);
    exit_action_->setStatusTip("Exit the application");

    // Edit menu actions
    insert_equation_action_ = new QAction("Insert Equation", this);
    insert_equation_action_->setStatusTip("Insert a new equation (can switch to group mode)");

    update_all_action_ = new QAction("Update All Equations", this);
    update_all_action_->setStatusTip("Update all equations in the manager");

    // View menu actions
    show_dependency_graph_action_ = new QAction("Dependency Graph", this);
    show_dependency_graph_action_->setStatusTip("Show equation dependency graph");

    show_equation_manager_action_ = new QAction("Equation Browser", this);
    show_equation_manager_action_->setStatusTip("Browser equations");

    show_variable_inspector_action_ = new QAction("Variable Inspector", this);
    show_variable_inspector_action_->setStatusTip("Inspect variables");

    show_expression_watch_action_ = new QAction("Expression Watch", this);
    show_expression_watch_action_->setStatusTip("Watch expressions");
}

void DemoWidget::InitCompletionModel()
{
    if (!equation_completion_model_)
    {
        return;
    }
    if (equation_completion_model_->language_name() == "Python")
    {
        std::set<std::string> all_builtin_names = equation_manager_->context().GetBuiltinNames();
        for (const auto &name : all_builtin_names)
        {
            QString word = QString::fromStdString(name);
            equation_completion_model_->AddCompletionItem(word, "Builtin", word);
        }
    }
}

void DemoWidget::OnOpen() {}

void DemoWidget::CreateMenus()
{
    // File menu
    file_menu_ = menuBar()->addMenu("&File");
    file_menu_->addAction(exit_action_);

    // Edit menu
    edit_menu_ = menuBar()->addMenu("&Edit");
    edit_menu_->addAction(insert_equation_action_);
    edit_menu_->addAction(update_all_action_);

    // View menu
    view_menu_ = menuBar()->addMenu("&View");
    view_menu_->addAction(show_dependency_graph_action_);
    view_menu_->addAction(show_dependency_graph_action_);
    view_menu_->addAction(show_equation_manager_action_);
    view_menu_->addAction(show_variable_inspector_action_);
    view_menu_->addAction(show_expression_watch_action_);
}

void DemoWidget::OnInsertEquationRequest()
{
    xequation::gui::EquationEditor *editor = new xequation::gui::EquationEditor(equation_completion_model_, this);

    connect(
        editor, &xequation::gui::EquationEditor::AddEquationRequest,
        [this, editor](const QString &equation_name, const QString &expression) {
            if (AddEquation(equation_name, expression))
            {
                editor->accept();
            }
        }
    );
    
    // Handle switch to group editor for new equation
    connect(
        editor, &xequation::gui::EquationEditor::SwitchToGroupEditorRequest,
        [this](const QString &initial_text) {
            // Use QTimer to ensure EquationEditor closes first
            QTimer::singleShot(0, [this, initial_text]() {
                xequation::gui::EquationGroupEditor *group_editor =
                    new xequation::gui::EquationGroupEditor(equation_completion_model_, this, "Insert Equation Group");
                group_editor->setAttribute(Qt::WA_DeleteOnClose);
                
                if (!initial_text.isEmpty())
                {
                    group_editor->SetText(initial_text);
                }
                
                connect(
                    group_editor, &xequation::gui::EquationGroupEditor::TextSubmitted,
                    [this, group_editor](const QString &statement) {
                        if (AddEquationGroup(statement.toStdString()))
                        {
                            group_editor->accept();
                        }
                    }
                );
                
                group_editor->exec();
            });
        }
    );

    editor->exec();
}

void DemoWidget::OnEditEquationGroupRequest(const xequation::EquationGroupId &id)
{
    auto group = equation_manager_->GetEquationGroup(id);
    if (!group)
    {
        return;
    }
    
    // Decide which editor to open based on editor_type_map_
    auto it = editor_type_map_.find(id);
    EditorType editor_type = (it != editor_type_map_.end()) ? it->second : EditorType::SingleEquation;
    
    if (editor_type == EditorType::SingleEquation)
    {
        // Open EquationEditor
        xequation::gui::EquationEditor *editor = new xequation::gui::EquationEditor(equation_completion_model_, this);
        editor->SetEquationGroup(group);
        
        connect(
            editor, &xequation::gui::EquationEditor::EditEquationRequest,
            [this, editor](const EquationGroupId &group_id, const QString &equation_name, const QString &expression) {
                if (EditEquation(group_id, equation_name, expression))
                {
                    editor->accept();
                }
            }
        );
        
        // Handle switch to group editor
        connect(
            editor, &xequation::gui::EquationEditor::SwitchToGroupEditorRequest,
            [this, id](const QString &initial_text) {
                // Use QTimer to ensure EquationEditor closes first
                QTimer::singleShot(0, [this, id, initial_text]() {
                    xequation::gui::EquationGroupEditor *group_editor =
                        new xequation::gui::EquationGroupEditor(equation_completion_model_, this, "Edit Equation Group");
                    group_editor->setAttribute(Qt::WA_DeleteOnClose);
                    group_editor->SetEquationGroup(equation_manager_->GetEquationGroup(id));
                    
                    if (!initial_text.isEmpty())
                    {
                        group_editor->SetText(initial_text);
                    }
                    
                    connect(
                        group_editor, &xequation::gui::EquationGroupEditor::TextSubmitted,
                        [this, group_editor, id](const QString &statement) {
                            if (EditEquationGroup(id, statement.toStdString()))
                            {
                                // Mark as converted to group editor
                                editor_type_map_[id] = EditorType::Group;
                                group_editor->accept();
                            }
                        }
                    );
                    
                    group_editor->exec();
                });
            }
        );

        editor->exec();
    }
    else  // EditorType::Group
    {
        // Open EquationGroupEditor
        xequation::gui::EquationGroupEditor *editor =
            new xequation::gui::EquationGroupEditor(equation_completion_model_, this, "Edit Equation Group");
        editor->setAttribute(Qt::WA_DeleteOnClose);
        editor->SetEquationGroup(group);
        
        connect(
            editor, &xequation::gui::EquationGroupEditor::TextSubmitted,
            [this, editor, id](const QString &statement) {
                if (EditEquationGroup(id, statement.toStdString()))
                {
                    editor->accept();
                }
            }
        );

        editor->exec();
    }
}

void DemoWidget::OnRemoveEquationGroupRequest(const xequation::EquationGroupId &id)
{
    if (RemoveEquationGroup(id))
    {
        editor_type_map_.erase(id);
    }
}

void DemoWidget::OnCopyEquationGroupRequest(const xequation::EquationGroupId &id)
{
    QApplication::clipboard()->setText(QString::fromStdString(equation_manager_->GetEquationGroup(id)->statement()));
}

void DemoWidget::OnUpdateEquationGroupRequest(const xequation::EquationGroupId &id)
{
    AsyncUpdateEquationGroup(id);
}

void DemoWidget::OnAddEquationGroupToExpressionWatchRequest(const xequation::EquationGroupId &id)
{
    const EquationGroup *group = equation_manager_->GetEquationGroup(id);
    if (!group)
    {
        return;
    }

    for (const auto &equation_name : group->GetEquationNames())
    {
        expression_watch_widget_->OnAddExpressionToWatch(QString::fromStdString(equation_name));
    }
}

bool DemoWidget::AddEquationGroup(const std::string &statement)
{
    if (!task_manager_->IsIdle())
    {
        QMessageBox::warning(
            this, "Operation Locked",
            "Cannot add equation group while updating another equation group. Please wait for the current operation to "
            "complete.",
            QMessageBox::Ok
        );
        return false;
    }

    try
    {
        auto id = equation_manager_->AddEquationGroup(statement);
        editor_type_map_[id] = EditorType::Group;
        // Delay the async update to ensure GIL is fully released
        QTimer::singleShot(0, [this, id]() { AsyncUpdateEquationGroup(id); });
        return true;
    }
    catch (const EquationException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
    catch (const ParseException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
    catch (const DependencyCycleException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
}
bool DemoWidget::EditEquationGroup(const xequation::EquationGroupId &id, const std::string &statement)
{
    if (!task_manager_->IsIdle())
    {
        QMessageBox::warning(
            this, "Operation Locked",
            "Cannot edit equation group while updating another equation group. Please wait for the current operation "
            "to complete.",
            QMessageBox::Ok
        );
        return false;
    }

    try
    {
        equation_manager_->EditEquationGroup(id, statement);
        // Delay the async update to ensure GIL is fully released
        QTimer::singleShot(0, [this, id]() { AsyncUpdateEquationGroup(id); });
        return true;
    }
    catch (const EquationException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
    catch (const ParseException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
    catch (const DependencyCycleException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
}

bool DemoWidget::AddEquation(const QString &equation_name, const QString &expression)
{
    if (!task_manager_->IsIdle())
    {
        QMessageBox::warning(
            this, "Operation Locked",
            "Cannot add equation while updating an equation group. Please wait for the current operation to complete.",
            QMessageBox::Ok
        );
        return false;
    }

    try
    {
        auto id = equation_manager_->AddEquation(equation_name.toStdString(), expression.toStdString());
        editor_type_map_[id] = EditorType::SingleEquation;
        // Delay the async update to ensure GIL is fully released
        QTimer::singleShot(0, [this, id]() { AsyncUpdateEquationGroup(id); });
        return true;
    }
    catch (const EquationException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
    catch (const ParseException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
    catch (const DependencyCycleException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
}

bool DemoWidget::EditEquation(
    const xequation::EquationGroupId &group_id, const QString &equation_name, const QString &expression
)
{
    if (!task_manager_->IsIdle())
    {
        QMessageBox::warning(
            this, "Operation Locked",
            "Cannot edit equation while updating an equation group. Please wait for the current operation to complete.",
            QMessageBox::Ok
        );
        return false;
    }

    try
    {
        equation_manager_->EditSingleEquation(group_id, equation_name.toStdString(), expression.toStdString());
        // Delay the async update to ensure GIL is fully released
        QTimer::singleShot(0, [this, group_id]() { AsyncUpdateEquationGroup(group_id); });
        return true;
    }
    catch (const EquationException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
    catch (const ParseException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
    catch (const DependencyCycleException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
}

bool DemoWidget::RemoveEquationGroup(const xequation::EquationGroupId &id)
{
    if (!task_manager_->IsIdle())
    {
        QMessageBox::warning(
            this, "Operation Locked",
            "Cannot remove equation group while updating another equation group. Please wait for the current operation "
            "to complete.",
            QMessageBox::Ok
        );
        return false;
    }

    try
    {
        auto equation_names = equation_manager_->GetEquationGroup(id)->GetEquationNames();
        auto update_equation_names = equation_manager_->graph().TopologicalSort(equation_names);
        equation_manager_->RemoveEquationGroup(id);
        AsyncUpdateEquationsAfterRemoveGroup(update_equation_names);
        return true;
    }
    catch (const EquationException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
    catch (const DependencyCycleException &e)
    {
        QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        return false;
    }
}

void DemoWidget::AsyncUpdateEquationGroup(const xequation::EquationGroupId &id)
{
    auto task = std::unique_ptr<xequation::gui::UpdateEquationGroupTask>(
        new xequation::gui::UpdateEquationGroupTask("Update Equation Group", equation_manager_.get(), id)
    );

    task_manager_->EnqueueTask(std::move(task));
}

void DemoWidget::AsyncUpdateManager()
{
    auto task = std::unique_ptr<xequation::gui::UpdateManagerTask>(
        new xequation::gui::UpdateManagerTask("Update All Equation Groups", equation_manager_.get())
    );

    task_manager_->EnqueueTask(std::move(task));
}

void DemoWidget::AsyncUpdateEquationsAfterRemoveGroup(const std::vector<std::string> &equation_names)
{
    auto task = std::unique_ptr<xequation::gui::UpdateEquationsTask>(new xequation::gui::UpdateEquationsTask(
        "Update Equations After Remove Group", equation_manager_.get(), equation_names
    ));

    task_manager_->EnqueueTask(std::move(task));
}

void DemoWidget::OnEquationGroupSelected(const xequation::EquationGroupId &id)
{
    equation_browser_widget_->blockSignals(true);
    variable_inspect_widget_->blockSignals(true);

    if (equation_manager_->IsEquationGroupExist(id))
    {
        const auto &group = equation_manager_->GetEquationGroup(id);
        auto equation_names = group->GetEquationNames();
        if (equation_names.size() >= 1)
        {
            const auto &equation = group->GetEquation(equation_names.back());
            variable_inspect_widget_->SetCurrentEquation(equation);
            equation_browser_widget_->SetCurrentEquation(equation, false);
        }
    }

    equation_browser_widget_->blockSignals(false);
    variable_inspect_widget_->blockSignals(false);
}

void DemoWidget::OnEquationSelected(const xequation::Equation *equation)
{
    mock_equation_list_widget_->blockSignals(true);
    variable_inspect_widget_->blockSignals(true);

    if (equation_manager_->IsEquationExist(equation->name()))
    {
        variable_inspect_widget_->SetCurrentEquation(equation);
        const auto &group_id = equation->group_id();
        mock_equation_list_widget_->SetCurrentEquationGroup(group_id);
    }

    mock_equation_list_widget_->blockSignals(false);
    variable_inspect_widget_->blockSignals(false);
}

void DemoWidget::OnShowDependencyGraph()
{
    dependency_graph_viewer_->show();
    dependency_graph_viewer_->raise();
    dependency_graph_viewer_->activateWindow();
}

void DemoWidget::OnShowEquationManager()
{

    equation_browser_widget_->show();
    equation_browser_widget_->raise();
    equation_browser_widget_->activateWindow();
}
void DemoWidget::OnShowEquationInspector()
{
    variable_inspect_widget_->show();
    variable_inspect_widget_->raise();
    variable_inspect_widget_->activateWindow();
}

void DemoWidget::OnShowExpressionWatch()
{
    expression_watch_widget_->show();
    expression_watch_widget_->raise();
    expression_watch_widget_->activateWindow();
}

void DemoWidget::OnParseResultRequested(const QString &expression, xequation::ParseResult &result)
{
    result = equation_manager_->Parse(expression.toStdString(), xequation::ParseMode::kExpression);
}

void DemoWidget::OnEvalResultAsyncRequested(const QUuid& id, const QString& expression)
{
    gui::EvalExpressionTask *task =
        new gui::EvalExpressionTask("Evaluate Expression", equation_manager_.get(), expression.toStdString());

    connect(task, &gui::EvalExpressionTask::EvalCompleted, this, [this, id](InterpretResult result) {
        expression_watch_widget_->OnEvalResultSubmitted(id, result);
    });

    task_manager_->EnqueueTask(std::unique_ptr<gui::EvalExpressionTask>(task));
}

void DemoWidget::OnEquationDependencyGraphImageRequested()
{
    gui::EquationDependencyGraphGenerationTask *task = new gui::EquationDependencyGraphGenerationTask(
        "Generate Dependency Graph", equation_manager_.get()
    );

    connect(
        task, &gui::EquationDependencyGraphGenerationTask::DependencyGraphImageGenerated, this,
        [this](const QString &image_path) { dependency_graph_viewer_->OnDependencyGraphImageGenerated(image_path); }
    );

    task_manager_->EnqueueTask(std::unique_ptr<gui::EquationDependencyGraphGenerationTask>(task));
}