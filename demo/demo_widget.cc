#include "demo_widget.h"
#include "debugger/variable_model.h"
#include "equation_editor.h"

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
#include <algorithm>
#include <utility>

#include "python/python_qt_wrapper.h"
#include "text_editor_dialog.h"

using namespace xequation;

DemoWidget::DemoWidget(QWidget *parent)
    : QMainWindow(parent), equation_browser_widget_(nullptr), variable_tree_(nullptr)
{
    equation_manager_ = xequation::python::PythonEquationEngine::GetInstance().CreateEquationManager();
    mock_equation_list_widget_ = new MockEquationGroupListWidget(equation_manager_.get(), this);
    equation_browser_widget_ = new xequation::gui::EquationBrowserWidget(equation_manager_.get(), this);

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

    equation_manager_->signals_manager().Connect<EquationEvent::kEquationUpdated>(
        [this](const Equation *equation, bitmask::bitmask<EquationUpdateFlag> change_type) {

        }
    );

    equation_manager_->signals_manager().Connect<EquationEvent::kEquationRemoving>([this](const Equation *equation) {});
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
    show_equation_manager_action_->setStatusTip("browser equations");

    show_variable_inspector_action_ = new QAction("Variable &Inspector", this);
    show_variable_inspector_action_->setStatusTip("Inspect variable");

    show_variable_monitor_action_ = new QAction("Variable Monitor", this);
    show_variable_monitor_action_->setStatusTip("Monitor variable");
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
    view_menu_->addAction(show_variable_monitor_action_);
}

void DemoWidget::OnInsertEquationRequest()
{
    xequation::gui::EquationEditor *editor = new xequation::gui::EquationEditor(equation_manager_.get(), this);

    connect(editor, &xequation::gui::EquationEditor::AddEquationRequest, [this, editor](const QString &statement) {
        xequation::EquationGroupId id;
        if (AddEquationGroup(statement.toStdString(), id))
        {
            single_equation_set_.insert(id);
            editor->OnSuccess();
        }
    });

    editor->exec();
}

void DemoWidget::OnEditEquationGroupRequest(const xequation::EquationGroupId &id)
{
    if (single_equation_set_.count(id) != 0)
    {
        auto group = equation_manager_->GetEquationGroup(id);
        xequation::gui::EquationEditor *editor = new xequation::gui::EquationEditor(group, this);

        connect(
            editor, &xequation::gui::EquationEditor::EditEquationRequest,
            [this, editor](const EquationGroupId &group_id, const QString &statement) {
                if (EditEquationGroup(group_id, statement.toStdString()))
                {
                    editor->OnSuccess();
                }
            }
        );

        editor->exec();
    }

    if (equation_group_set_.count(id) != 0)
    {
        TextEditorDialog *editor = new TextEditorDialog(this, "Edit Equation Group");
        editor->setText(QString::fromStdString(equation_manager_->GetEquationGroup(id)->statement()));
        connect(editor, &TextEditorDialog::textSubmitted, [this, editor, id](const QString &statement) {
            if (EditEquationGroup(id, statement.toStdString()))
            {
                editor->accept();
            }
        });

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

bool DemoWidget::AddEquationGroup(const std::string &statement, xequation::EquationGroupId &id)
{
    try
    {
        id = equation_manager_->AddEquationGroup(statement);
        equation_manager_->UpdateEquationGroup(id);
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
        equation_manager_->UpdateEquationGroup(id);
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

void DemoWidget::OnEquationGroupSelected(const xequation::EquationGroupId &id)
{
    if (equation_browser_widget_)
    {
        equation_browser_widget_->blockSignals(true);
    }

    if (equation_manager_->IsEquationGroupExist(id))
    {
        const auto &group = equation_manager_->GetEquationGroup(id);
        auto equation_names = group->GetEquationNames();
        if (equation_names.size() >= 1)
        {
            const auto &equation = group->GetEquation(equation_names[0]);

            if (equation_browser_widget_)
            {
                equation_browser_widget_->SetCurrentEquation(equation, false);
            }
        }
    }

    if (equation_browser_widget_)
    {
        equation_browser_widget_->blockSignals(false);
    }
}

void DemoWidget::OnEquationSelected(const xequation::Equation *equation)
{
    if (mock_equation_list_widget_)
    {
        mock_equation_list_widget_->blockSignals(true);
    }

    if (equation_manager_->IsEquationExist(equation->name()))
    {
        if (mock_equation_list_widget_)
        {
            const auto &group_id = equation->group_id();
            mock_equation_list_widget_->SetCurrentEquationGroup(group_id);
        }
    }

    if (mock_equation_list_widget_)
    {
        mock_equation_list_widget_->blockSignals(false);
    }
}

void DemoWidget::OnInsertEquationGroupRequest()
{
    TextEditorDialog *editor = new TextEditorDialog(this, "Insert Equation Group");

    connect(editor, &TextEditorDialog::textSubmitted, [this, editor](const QString &statement) {
        xequation::EquationGroupId id;
        if (AddEquationGroup(statement.toStdString(), id))
        {
            equation_group_set_.insert(id);
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
    if (equation_browser_widget_ == nullptr)
    {
        equation_browser_widget_ = new xequation::gui::EquationBrowserWidget(equation_manager_.get(), this);
        equation_browser_widget_->show();
    }
    else
    {
        equation_browser_widget_->show();
        equation_browser_widget_->raise();
        equation_browser_widget_->activateWindow();
    }

    statusBar()->showMessage("Opening equation manager", 2000);
}

void DemoWidget::OnShowEquationInspector()
{
    if (variable_tree_ == nullptr)
    {
        variable_tree_ = new xequation::gui::VariableView(this);
        variable_tree_->setWindowFlags(
            Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint
        );
        variable_tree_->setWindowTitle("Variable Inspector");
    }

    variable_tree_->show();
    variable_tree_->raise();
    variable_tree_->activateWindow();

    xequation::gui::VariableModel *model = variable_tree_->variable_model();

    xequation::gui::Variable::UniquePtr var1 = xequation::gui::Variable::Create("var1", "10", "int");
    auto var_ptr1 = var1.get();
    model->AddRootVariable(std::move(var1));

    // test child variables
    xequation::gui::Variable::UniquePtr var3 = xequation::gui::Variable::Create("var3", "30", "int");
    var_ptr1->AddChild(std::move(var3));

    var_ptr1->set_value("test change value");

    for (int i = 0; i < 5; ++i)
    {
        QString name = QString("child_%1").arg(i);
        xequation::gui::Variable::UniquePtr var = xequation::gui::Variable::Create(name, "value", "type");
        var_ptr1->AddChild(std::move(var));
    }
    statusBar()->showMessage("Opening variable inspector", 2000);
}