#include "demo_widget.h"
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
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

#ifdef slots
#undef slots
#endif
#include "python/python_equation_engine.h"
#define slots Q_SLOTS

using namespace xequation;

DemoWidget::DemoWidget(QWidget *parent) : QMainWindow(parent), equation_manager_widget_(nullptr)
{
    SetupUI();
    SetupConnections();

    auto id = equation_manager_->AddEquationGroup("from math import *");
    equation_manager_->UpdateEquationGroup(id);
}

void DemoWidget::SetupUI()
{
    setWindowTitle("xequation demo");
    setFixedSize(800, 600);

    equation_manager_ = xequation::python::PythonEquationEngine::GetInstance().CreateEquationManager();

    mock_equation_list_widget_ = new MockEquationGroupListWidget(equation_manager_.get(), this);

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
    connect(show_equation_inspector_action_, &QAction::triggered, this, &DemoWidget::OnShowEquationInspector);

    connect(
        mock_equation_list_widget_, &MockEquationGroupListWidget::OnEditEquationGroup, this,
        &DemoWidget::OnEditEquationGroupRequest
    );

    connect(
        mock_equation_list_widget_, &MockEquationGroupListWidget::OnRemoveEquationGroup, this,
        &DemoWidget::OnRemoveEquationGroupRequest
    );

    connect(
        mock_equation_list_widget_, &MockEquationGroupListWidget::OnCopyEquationGroup,
        [this](const xequation::EquationGroupId &id) {
            QApplication::clipboard()->setText(
                QString::fromStdString(equation_manager_->GetEquationGroup(id)->statement())
            );
        }
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
    insert_equation_action_ = new QAction("Insert &Equation", this);
    insert_equation_action_->setShortcut(QKeySequence("Ctrl+E"));
    insert_equation_action_->setStatusTip("Insert a single equation");

    insert_equation_group_action_ = new QAction("Insert Equation Group", this);
    insert_equation_group_action_->setShortcut(QKeySequence("Ctrl+Shift+E"));
    insert_equation_group_action_->setStatusTip("Insert multiple equations");

    // View menu actions
    show_dependency_graph_action_ = new QAction("&Dependency Graph", this);
    show_dependency_graph_action_->setShortcut(QKeySequence("Ctrl+G"));
    show_dependency_graph_action_->setStatusTip("Show equation dependency graph");

    show_equation_manager_action_ = new QAction("Equation &Manager", this);
    show_equation_manager_action_->setShortcut(QKeySequence("Ctrl+M"));
    show_equation_manager_action_->setStatusTip("Manage equations");

    show_equation_inspector_action_ = new QAction("Equation &Inspector", this);
    show_equation_inspector_action_->setShortcut(QKeySequence("Ctrl+I"));
    show_equation_inspector_action_->setStatusTip("Inspect equation properties");
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
    view_menu_->addAction(show_equation_inspector_action_);
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
        auto equation_names = group->GetEquationNames();
        if (equation_names.size() != 1)
        {
            return;
        }
        auto equation = group->GetEquation(equation_names[0]);
        xequation::gui::EquationEditor *editor = new xequation::gui::EquationEditor(equation, this);

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
}

void DemoWidget::OnRemoveEquationGroupRequest(const xequation::EquationGroupId &id)
{
    if (RemoveEquationGroup(id))
    {
        if (single_equation_set_.count(id) != 0)
        {
            single_equation_set_.erase(id);
        }
    }
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

void DemoWidget::OnInsertEquationGroupRequest()
{
    QString equations = "\\begin{align}\n"
                        "  F &= ma \\\\\n"
                        "  v &= u + at \\\\\n"
                        "  s &= ut + \\frac{1}{2}at^2\n"
                        "\\end{align}";

    statusBar()->showMessage("Multiple equations inserted", 2000);

    QMessageBox::information(
        this, "Insert Multi-Equations", "Multiple equations inserted into document.\nContains related equation set."
    );
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
    if (equation_manager_widget_ == nullptr)
    {
        equation_manager_widget_ = new xequation::gui::EquationManagerWidget(equation_manager_.get(), this);
        equation_manager_widget_->show();
    }
    else
    {
        equation_manager_widget_->show();
        equation_manager_widget_->raise();
        equation_manager_widget_->activateWindow();
    }

    statusBar()->showMessage("Opening equation manager", 2000);
}

void DemoWidget::OnShowEquationInspector()
{
    statusBar()->showMessage("Opening equation inspector", 2000);

    QMessageBox::information(
        this, "Equation Inspector",
        "Equation Inspector Feature\n\n"
        "This will display detailed equation properties and information.\n"
        "To be implemented: Inspect equation variables, types, complexity, etc."
    );
}