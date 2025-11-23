#include "demo_widget.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>


#ifdef slots
#undef slots
#endif
#include "python/python_equation_engine.h"
#define slots Q_SLOTS

using namespace xequation;

DemoWidget::DemoWidget(QWidget *parent) : QMainWindow(parent), equation_manager_widget_(nullptr)
{
    setWindowTitle("Qt Demo Widget - Equation Editor");
    setFixedSize(800, 600);

    equation_manager_ = xequation::python::PythonEquationEngine::GetInstance().CreateEquationManager();

    mock_equation_list_widget_ = new MockEquationListWidget(equation_manager_.get(), this);

    setCentralWidget(mock_equation_list_widget_);

    // Create menus and actions
    createActions();
    createMenus();

    auto id = equation_manager_->AddEquationGroup("from math import *");
    equation_manager_->UpdateEquationGroup(id);
}

void DemoWidget::createActions()
{
    // File menu actions
    openAction = new QAction("&Open", this);
    openAction->setShortcut(QKeySequence::Open);
    openAction->setStatusTip("Open an existing file");
    connect(openAction, &QAction::triggered, this, &DemoWidget::onOpen);

    exitAction = new QAction("E&xit", this);
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setStatusTip("Exit the application");
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    // Edit menu actions
    insertEquationAction = new QAction("Insert &Equation", this);
    insertEquationAction->setShortcut(QKeySequence("Ctrl+E"));
    insertEquationAction->setStatusTip("Insert a single equation");
    connect(insertEquationAction, &QAction::triggered, this, &DemoWidget::onInsertEquation);

    insertMultiEquationsAction = new QAction("Insert &Multi-Equations", this);
    insertMultiEquationsAction->setShortcut(QKeySequence("Ctrl+Shift+E"));
    insertMultiEquationsAction->setStatusTip("Insert multiple equations");
    connect(insertMultiEquationsAction, &QAction::triggered, this, &DemoWidget::onInsertMultiEquations);

    // View menu actions
    dependencyGraphAction = new QAction("&Dependency Graph", this);
    dependencyGraphAction->setShortcut(QKeySequence("Ctrl+G"));
    dependencyGraphAction->setStatusTip("Show equation dependency graph");
    connect(dependencyGraphAction, &QAction::triggered, this, &DemoWidget::onShowDependencyGraph);

    equationManagerAction = new QAction("Equation &Manager", this);
    equationManagerAction->setShortcut(QKeySequence("Ctrl+M"));
    equationManagerAction->setStatusTip("Manage equations");
    connect(equationManagerAction, &QAction::triggered, this, &DemoWidget::onShowEquationManager);

    equationInspectorAction = new QAction("Equation &Inspector", this);
    equationInspectorAction->setShortcut(QKeySequence("Ctrl+I"));
    equationInspectorAction->setStatusTip("Inspect equation properties");
    connect(equationInspectorAction, &QAction::triggered, this, &DemoWidget::onShowEquationInspector);
}

void DemoWidget::onOpen() {}

void DemoWidget::createMenus()
{
    // File menu
    fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(exitAction);

    // Edit menu
    editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(insertEquationAction);
    editMenu->addAction(insertMultiEquationsAction);

    // View menu
    viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(dependencyGraphAction);
    viewMenu->addAction(dependencyGraphAction);
    viewMenu->addAction(equationManagerAction);
    viewMenu->addAction(equationInspectorAction);
}

void DemoWidget::onInsertEquation()
{
    xequation::gui::EquationInsertEditor *editor =
        new xequation::gui::EquationInsertEditor(equation_manager_.get(), this);

    connect(editor, &xequation::gui::EquationInsertEditor::AddEquationRequest, [this, editor](const QString &statement) {
        try {
            auto id = equation_manager_->AddEquationGroup(statement.toStdString());
            equation_manager_->UpdateEquationGroup(id);
            editor->OnEquationAddSuccess();
        } 
        catch (const EquationException& e) 
        {
            QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        }
        catch (const DependencyCycleException& e)
        {
            QMessageBox::warning(this, "Warning", e.what(), QMessageBox::Ok);
        }
    });

    editor->exec();
}

void DemoWidget::onInsertMultiEquations()
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

void DemoWidget::onShowDependencyGraph()
{
    statusBar()->showMessage("Showing dependency graph", 2000);

    QMessageBox::information(
        this, "Dependency Graph",
        "Dependency Graph Feature\n\n"
        "This will display dependency relationships between equations.\n"
        "To be implemented: Visualize equation dependencies."
    );
}

void DemoWidget::onShowEquationManager()
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

void DemoWidget::onShowEquationInspector()
{
    statusBar()->showMessage("Opening equation inspector", 2000);

    QMessageBox::information(
        this, "Equation Inspector",
        "Equation Inspector Feature\n\n"
        "This will display detailed equation properties and information.\n"
        "To be implemented: Inspect equation variables, types, complexity, etc."
    );
}