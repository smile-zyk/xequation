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
#include <memory>

#include "equation_code_editor.h"
#include "equation_editor.h"
#include "equation_manager_tasks.h"
#include "equation_signals_qt_utils.h"
#include "python/python_qt_wrapper.h"

using namespace xequation;

DemoWidget::DemoWidget(QWidget *parent)
    : QMainWindow(parent),
      equation_browser_widget_(nullptr),
      variable_inspect_widget_(nullptr),
      expression_watch_widget_(nullptr),
      equation_editor_(nullptr),
      equation_code_editor_(nullptr),
      equation_manager_config_widget_(nullptr)
{
    // Initialize config option with default values
    config_option_.scale_factor = 100;
    config_option_.style_model = "Light";
    config_option_.auto_update = true;

    equation_manager_ = xequation::python::PythonEquationEngine::GetInstance().CreateEquationManager();
    mock_equation_list_widget_ = new MockEquationGroupListWidget(equation_manager_.get(), this);
    equation_browser_widget_ = new xequation::gui::EquationBrowserWidget(this);
    variable_inspect_widget_ = new xequation::gui::VariableInspectWidget(this);

    equation_completion_model_ = new xequation::gui::EquationCompletionModel(&equation_manager_->context(), this);

    expression_watch_widget_ = new xequation::gui::ExpressionWatchWidget(equation_completion_model_, this);

    dependency_graph_viewer_ = new xequation::gui::EquationDependencyGraphViewer(this);

    task_manager_ = new xequation::gui::ToastTaskManager(this, 1);

    // Create persistent editors and connect signals once
    equation_editor_ = new xequation::gui::EquationEditor(equation_completion_model_, this);
    equation_code_editor_ = new xequation::gui::EquationCodeEditor(this);
    equation_code_editor_->SetCompletionModel(equation_completion_model_);
    equation_manager_config_widget_ =
        new xequation::gui::EquationManagerConfigWidget(equation_manager_->engine_info(), this);

    // Connect editor signals once
    connect(
        equation_editor_, &xequation::gui::EquationEditor::AddEquationRequest, this,
        &DemoWidget::OnEquationEditorAddEquationRequest
    );
    connect(
        equation_editor_, &xequation::gui::EquationEditor::EditEquationRequest, this,
        &DemoWidget::OnEquationEditorEditEquationRequest
    );
    connect(
        equation_editor_, &xequation::gui::EquationEditor::UseCodeEditorRequest, this,
        &DemoWidget::OnEquationEditorUseCodeEditorRequest
    );
    connect(
        equation_code_editor_, &xequation::gui::EquationCodeEditor::AddEquationRequest, this,
        &DemoWidget::OnCodeEditorAddEquationRequest
    );
    connect(
        equation_code_editor_, &xequation::gui::EquationCodeEditor::EditEquationRequest, this,
        &DemoWidget::OnCodeEditorEditEquationRequest
    );
    connect(
        equation_code_editor_, &xequation::gui::EquationCodeEditor::StyleModeChanged, this,
        &DemoWidget::OnCodeEditorStyleModeChanged
    );
    connect(
        equation_code_editor_, &xequation::gui::EquationCodeEditor::ZoomChanged, this,
        &DemoWidget::OnCodeEditorZoomChanged
    );
    connect(
        equation_manager_config_widget_, &xequation::gui::EquationManagerConfigWidget::ConfigAccepted, this,
        &DemoWidget::OnEquationManagerConfigAccepted
    );

    // Apply initial config to code editor
    equation_code_editor_->blockSignals(true);
    equation_code_editor_->SetEditorZoomFactor(config_option_.scale_factor / 100.0);
    xequation::gui::CodeEditor::StyleMode initial_mode = (config_option_.style_model == "Dark")
                                                             ? xequation::gui::CodeEditor::StyleMode::kDark
                                                             : xequation::gui::CodeEditor::StyleMode::kLight;
    equation_code_editor_->SetEditorStyleMode(initial_mode);
    equation_code_editor_->blockSignals(false);

    // Load config to config widget as well
    equation_manager_config_widget_->SetConfigOption(config_option_);

    SetupUI();
    SetupConnections();
}

DemoWidget::~DemoWidget()
{
    delete task_manager_;
    task_manager_ = nullptr;
    // Editors are deleted automatically as children of this widget
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
    connect(show_equation_manager_config_action_, &QAction::triggered, this, &DemoWidget::OnShowEquationManagerConfig);
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

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationUpdated>(
        &equation_manager_->signals_manager(), equation_completion_model_,
        &xequation::gui::EquationCompletionModel::OnEquationUpdated
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

    show_equation_manager_config_action_ = new QAction("Equation Manager Config", this);
    show_equation_manager_config_action_->setStatusTip("Configure equation manager settings");
}

void DemoWidget::InitCompletionModel()
{
    if (!equation_completion_model_)
    {
        return;
    }
    if (QString::fromStdString(equation_manager_->engine_info().name) == "Python")
    {
        std::vector<std::string> all_builtin_names = equation_manager_->context().GetBuiltinNames();
        for (const auto &name : all_builtin_names)
        {
            QString word = QString::fromStdString(name);
            QString type = QString::fromStdString(equation_manager_->context().GetSymbolType(name));
            QString category = QString::fromStdString(equation_manager_->context().GetTypeCategory(type.toStdString()));
            equation_completion_model_->AddCompletionItem(word, type, category);
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
    view_menu_->addAction(show_equation_manager_config_action_);
}

void DemoWidget::OnInsertEquationRequest()
{
    equation_editor_->exec();
}

void DemoWidget::OnEditEquationGroupRequest(const xequation::EquationGroupId &id)
{
    auto group = equation_manager_->GetEquationGroup(id);
    if (!group)
    {
        return;
    }

    auto it = editor_type_map_.find(id);
    EditorType editor_type = (it != editor_type_map_.end()) ? it->second : EditorType::Normal;

    if (editor_type == EditorType::Normal)
    {
        equation_editor_->SetEquationGroup(group);
        equation_editor_->exec();
    }
    else if (editor_type == EditorType::Code)
    {
        equation_code_editor_->SetEquationGroup(group);
        equation_code_editor_->exec();
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
        editor_type_map_[id] = EditorType::Code;
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
        editor_type_map_[id] = EditorType::Code;
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
        editor_type_map_[id] = EditorType::Normal;
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
        editor_type_map_[group_id] = EditorType::Normal;
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

void DemoWidget::OnShowEquationManagerConfig()
{
    equation_manager_config_widget_->SetConfigOption(config_option_);
    equation_manager_config_widget_->show();
    equation_manager_config_widget_->raise();
    equation_manager_config_widget_->activateWindow();
}

void DemoWidget::OnEquationManagerConfigAccepted(const xequation::gui::EquationManagerConfigOption &option)
{
    // Apply configuration to global code editor
    if (equation_code_editor_)
    {
        double zoom_factor = option.scale_factor / 100.0;
        xequation::gui::CodeEditor::StyleMode style_mode = (option.style_model == "Dark")
                                                               ? xequation::gui::CodeEditor::StyleMode::kDark
                                                               : xequation::gui::CodeEditor::StyleMode::kLight;
        equation_code_editor_->SetEditorZoomFactor(zoom_factor);
        equation_code_editor_->SetEditorStyleMode(style_mode);
    }

    if (option.startup_script != config_option_.startup_script)
    {
        equation_manager_->ResetContext();

        auto startup_task = std::unique_ptr<gui::ExecStatementTask>(new gui::ExecStatementTask(
            "Execute Startup Script", equation_manager_.get(), option.startup_script.toStdString()
        ));
        new gui::ExecStatementTask(
            "Execute Startup Script", equation_manager_.get(), option.startup_script.toStdString()
        );

        auto update_task = std::unique_ptr<gui::UpdateManagerTask>(
            new gui::UpdateManagerTask("Update All Equation Groups", equation_manager_.get())
        );

        connect(startup_task.get(), &gui::ExecStatementTask::ExecCompleted, this, [this](InterpretResult result) {
            if (result.status == ResultStatus::kSuccess)
            {
                auto symbol_names = equation_manager_->context().GetSymbolNames();
                for (const auto &name : symbol_names)
                {
                    QString word = QString::fromStdString(name);
                    QString type = QString::fromStdString(equation_manager_->context().GetSymbolType(name));
                    QString category =
                        QString::fromStdString(equation_manager_->context().GetTypeCategory(type.toStdString()));
                    equation_completion_model_->AddCompletionItem(word, type, category);
                }
            }
        });

        task_manager_->EnqueueTask(std::move(startup_task));
        task_manager_->EnqueueTask(std::move(update_task));
    }

    config_option_ = option;
}

void DemoWidget::OnCodeEditorZoomChanged(double zoom_factor)
{
    // Update config option when zoom changes
    config_option_.scale_factor = static_cast<int>(zoom_factor * 100);

    // Also apply to global code editor to keep them in sync
    if (equation_code_editor_)
    {
        equation_code_editor_->blockSignals(true);
        equation_code_editor_->SetEditorZoomFactor(zoom_factor);
        equation_code_editor_->blockSignals(false);
    }
}

void DemoWidget::OnCodeEditorStyleModeChanged(xequation::gui::CodeEditor::StyleMode mode)
{
    // Update config option when style mode changes
    config_option_.style_model = (mode == xequation::gui::CodeEditor::StyleMode::kDark) ? "Dark" : "Light";

    // Also apply to global code editor to keep them in sync
    if (equation_code_editor_)
    {
        equation_code_editor_->SetEditorStyleMode(mode);
    }
}

void DemoWidget::OnParseResultRequested(const QString &expression, xequation::ParseResult &result)
{
    result = equation_manager_->Parse(expression.toStdString(), xequation::ParseMode::kExpression);
}

void DemoWidget::OnEvalResultAsyncRequested(const QUuid &id, const QString &expression)
{
    auto task = std::unique_ptr<gui::EvalExpressionTask>(
        new gui::EvalExpressionTask("Evaluate Expression", equation_manager_.get(), expression.toStdString())
    );

    connect(task.get(), &gui::EvalExpressionTask::EvalCompleted, this, [this, id](InterpretResult result) {
        expression_watch_widget_->OnEvalResultSubmitted(id, result);
    });

    task_manager_->EnqueueTask(std::move(task));
}

void DemoWidget::OnEquationDependencyGraphImageRequested()
{
    if (config_option_.auto_update == false)
    {
        return;
    }

    auto task = std::unique_ptr<gui::EquationDependencyGraphGenerationTask>(
        new gui::EquationDependencyGraphGenerationTask("Generate Dependency Graph", equation_manager_.get())
    );

    connect(
        task.get(), &gui::EquationDependencyGraphGenerationTask::DependencyGraphImageGenerated, this,
        [this](const QString &image_path) { dependency_graph_viewer_->OnDependencyGraphImageGenerated(image_path); }
    );

    task_manager_->EnqueueTask(std::move(task));
}

void DemoWidget::OnEquationEditorAddEquationRequest(const QString &equation_name, const QString &expression)
{
    if (AddEquation(equation_name, expression))
    {
        equation_editor_->accept();
    }
}

void DemoWidget::OnEquationEditorEditEquationRequest(
    const xequation::EquationGroupId &group_id, const QString &equation_name, const QString &expression
)
{
    if (EditEquation(group_id, equation_name, expression))
    {
        equation_editor_->accept();
    }
}

void DemoWidget::OnEquationEditorUseCodeEditorRequest(const QString &initial_text)
{
    auto group = equation_editor_->equation_group();

    equation_editor_->reject();

    equation_code_editor_->SetEquationGroup(group);

    if (!initial_text.isEmpty())
    {
        equation_code_editor_->SetText(initial_text);
    }

    equation_code_editor_->exec();
}

void DemoWidget::OnCodeEditorAddEquationRequest(const QString &statement)
{
    if (AddEquationGroup(statement.toStdString()))
    {
        equation_code_editor_->accept();
    }
}

void DemoWidget::OnCodeEditorEditEquationRequest(const xequation::EquationGroupId &group_id, const QString &statement)
{
    if (EditEquationGroup(group_id, statement.toStdString()))
    {
        equation_code_editor_->accept();
    }
}