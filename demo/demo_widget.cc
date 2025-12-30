#include "demo_widget.h"
#include "core/equation_common.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

#include "equation_editor.h"
#include "equation_group_editor.h"
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

    auto eval_expr_handle = [equation_manager_ptr = equation_manager_.get()](const QString &expression) {
        return equation_manager_ptr->Eval(expression.toStdString());
    };

    auto parse_expr_handle = [equation_manager_ptr = equation_manager_.get()](const QString &expression) {
        return equation_manager_ptr->Parse(expression.toStdString(), ParseMode::kExpression);
    };

    expression_watch_widget_ = new xequation::gui::ExpressionWatchWidget(parse_expr_handle, eval_expr_handle, this);

    SetupUI();
    SetupConnections();
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
    connect(insert_equation_group_action_, &QAction::triggered, this, &DemoWidget::OnInsertEquationGroupRequest);
    connect(show_dependency_graph_action_, &QAction::triggered, this, &DemoWidget::OnShowDependencyGraph);
    connect(show_equation_manager_action_, &QAction::triggered, this, &DemoWidget::OnShowEquationManager);
    connect(show_variable_inspector_action_, &QAction::triggered, this, &DemoWidget::OnShowEquationInspector);
    connect(show_expression_watch_action_, &QAction::triggered, this, &DemoWidget::OnShowExpressionWatch);
    connect(
        variable_inspect_widget_, &xequation::gui::VariableInspectWidget::AddExpressionToWatch,
        expression_watch_widget_, &xequation::gui::ExpressionWatchWidget::OnAddExpressionToWatch
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
        equation_browser_widget_, &xequation::gui::EquationBrowserWidget::EquationSelected, this,
        &DemoWidget::OnEquationSelected
    );

    // xequation::gui::EquationBrowserWidget* equation_browser_widget_;
    // xequation::gui::VariableInspectWidget* variable_inspect_widget_;
    // xequation::gui::ExpressionWatchWidget* expression_watch_widget_;
    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationAdded>(
        &equation_manager_->signals_manager(), equation_browser_widget_,
        &xequation::gui::EquationBrowserWidget::OnEquationAdded
    );

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationRemoving>(
        &equation_manager_->signals_manager(), equation_browser_widget_,
        &xequation::gui::EquationBrowserWidget::OnEquationRemoving
    );

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationUpdated>(
        &equation_manager_->signals_manager(), equation_browser_widget_,
        &xequation::gui::EquationBrowserWidget::OnEquationUpdated
    );

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationRemoving>(
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

    xequation::gui::ConnectEquationSignal<EquationEvent::kEquationRemoving>(
        &equation_manager_->signals_manager(), equation_completion_model_,
        &xequation::gui::EquationCompletionModel::OnEquationRemoving
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
    insert_equation_action_->setStatusTip("Insert a single equation");

    insert_equation_group_action_ = new QAction("Insert Equation Group", this);
    insert_equation_group_action_->setStatusTip("Insert multiple equations");

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
    edit_menu_->addAction(insert_equation_group_action_);

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

    connect(editor, &xequation::gui::EquationEditor::AddEquationRequest, [this, editor](const QString &equation_name, const QString &expression) {
        xequation::EquationGroupId id;
        if (AddEquation(equation_name, expression))
        {
            editor->accept();
        }
    });

    editor->exec();
}

void DemoWidget::OnEditEquationGroupRequest(const xequation::EquationGroupId &id)
{
    if (single_equation_set_.count(id) != 0)
    {
        auto group = equation_manager_->GetEquationGroup(id);
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

        editor->exec();
    }

    if (equation_group_set_.count(id) != 0)
    {
        xequation::gui::EquationGroupEditor *editor =
            new xequation::gui::EquationGroupEditor(equation_completion_model_, this, "Edit Equation Group");
        editor->setAttribute(Qt::WA_DeleteOnClose);
        editor->SetEquationGroup(equation_manager_->GetEquationGroup(id));
        connect(
            editor, &xequation::gui::EquationGroupEditor::TextSubmitted, [this, editor, id](const QString &statement) {
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
        if (single_equation_set_.count(id) != 0)
        {
            single_equation_set_.erase(id);
        }

        if (equation_group_set_.count(id) != 0)
        {
            equation_group_set_.erase(id);
        }
    }
}

void DemoWidget::OnCopyEquationGroupRequest(const xequation::EquationGroupId &id)
{
    QApplication::clipboard()->setText(QString::fromStdString(equation_manager_->GetEquationGroup(id)->statement()));
}

bool DemoWidget::AddEquationGroup(const std::string &statement)
{
    try
    {
        auto id = equation_manager_->AddEquationGroup(statement);
        equation_group_set_.insert(id);
        // Delay the async update to ensure GIL is fully released
        QTimer::singleShot(0, [this, id]() {
            AsyncUpdateEquationGroup(id);
        });
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
    try
    {
        equation_manager_->EditEquationGroup(id, statement);
        // Delay the async update to ensure GIL is fully released
        QTimer::singleShot(0, [this, id]() {
            AsyncUpdateEquationGroup(id);
        });
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
    try
    {
        auto id = equation_manager_->AddSingleEquation(equation_name.toStdString(), expression.toStdString());
        single_equation_set_.insert(id);
        // Delay the async update to ensure GIL is fully released
        QTimer::singleShot(0, [this, id]() {
            AsyncUpdateEquationGroup(id);
        });
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

bool DemoWidget::EditEquation(const xequation::EquationGroupId &group_id, const QString &equation_name, const QString &expression)
{
    try
    {
        equation_manager_->EditSingleEquation(
            group_id, equation_name.toStdString(), expression.toStdString()
        );
        // Delay the async update to ensure GIL is fully released
        QTimer::singleShot(0, [this, group_id]() {
            AsyncUpdateEquationGroup(group_id);
        });
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
    try
    {
        auto equation_names = equation_manager_->GetEquationGroup(id)->GetEquationNames();
        auto update_equation_names = equation_manager_->graph().TopologicalSort(equation_names);
        equation_manager_->RemoveEquationGroup(id);
        for (const auto &equation_name : update_equation_names)
        {
            if (std::find(equation_names.begin(), equation_names.end(), equation_name) == equation_names.end())
            {
                equation_manager_->UpdateSingleEquation(equation_name);
            }
        }
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
    // Use QFuture to properly manage the async task
    // This ensures GIL is properly released before starting the background thread
    QtConcurrent::run([this, id]() {
        try
        {
            // UpdateEquationGroup will acquire GIL internally as needed
            equation_manager_->UpdateEquationGroup(id);
        }
        catch (const std::exception &e)
        {
            std::string error_msg = e.what();
            qDebug() << "Error updating equation group:" << QString::fromStdString(error_msg);
        }
    });
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
            const auto &equation = group->GetEquation(equation_names[0]);
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

void DemoWidget::OnInsertEquationGroupRequest()
{
    xequation::gui::EquationGroupEditor *editor =
        new xequation::gui::EquationGroupEditor(equation_completion_model_, this, "Insert Equation Group");
    editor->setAttribute(Qt::WA_DeleteOnClose);
    connect(editor, &xequation::gui::EquationGroupEditor::TextSubmitted, [this, editor](const QString &statement) {
        xequation::EquationGroupId id;
        if (AddEquationGroup(statement.toStdString()))
        {
            editor->accept();
        }
    });

    editor->exec();
}

void DemoWidget::OnShowDependencyGraph()
{
    statusBar()->showMessage("Showing dependency graph", 2000);

    QMessageBox::information(
        this, "Dependency Graph",
        "Dependency Graph Feature\n\n"
        "This will display dependency relationships between equations.\n"
        "To be implemented: Visualize equation dependencies."
    );
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