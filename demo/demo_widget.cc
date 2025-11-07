#include "demo_widget.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QTextEdit>
#include <QLabel>
#include <QStatusBar>
#include <QMessageBox>
#include <QApplication>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>
#include <qaction.h>

DemoWidget::DemoWidget(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Qt Demo Widget - Equation Editor");
    setMinimumSize(800, 600);
    
    // Create central widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Create layout
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    
    // Create menus and actions
    createActions();
    createMenus();
    
    statusBar()->showMessage("Application started", 3000);
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
    
    equationInspectorAction = new QAction("Equation &Inspector", this);
    equationInspectorAction->setShortcut(QKeySequence("Ctrl+I"));
    equationInspectorAction->setStatusTip("Inspect equation properties");
    connect(equationInspectorAction, &QAction::triggered, this, &DemoWidget::onShowEquationInspector);

    equationResultInspectorAction = new QAction("Equation Result Inspector", this);
    equationResultInspectorAction->setStatusTip("Inspect equation result properties");
    connect(equationResultInspectorAction, &QAction::triggered, this, &DemoWidget::onShowEquationResultInspector);
}

void DemoWidget::onOpen()
{

}

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
    viewMenu->addAction(equationInspectorAction);
}

void DemoWidget::onInsertEquation()
{
    QString equation = "\\[ E = mc^2 \\]";
    statusBar()->showMessage("Single equation inserted", 2000);
    
    QMessageBox::information(this, "Insert Equation", 
        "Single equation inserted into document.\nExample: " + equation);
}

void DemoWidget::onInsertMultiEquations()
{
    QString equations = 
        "\\begin{align}\n"
        "  F &= ma \\\\\n"
        "  v &= u + at \\\\\n"
        "  s &= ut + \\frac{1}{2}at^2\n"
        "\\end{align}";
    
    statusBar()->showMessage("Multiple equations inserted", 2000);
    
    QMessageBox::information(this, "Insert Multi-Equations", 
        "Multiple equations inserted into document.\nContains related equation set.");
}

void DemoWidget::onShowDependencyGraph()
{
    statusBar()->showMessage("Showing dependency graph", 2000);
    
    QMessageBox::information(this, "Dependency Graph", 
        "Dependency Graph Feature\n\n"
        "This will display dependency relationships between equations.\n"
        "To be implemented: Visualize equation dependencies.");
}

void DemoWidget::onShowEquationInspector()
{
    statusBar()->showMessage("Opening equation inspector", 2000);
    
    QMessageBox::information(this, "Equation Inspector", 
        "Equation Inspector Feature\n\n"
        "This will display detailed equation properties and information.\n"
        "To be implemented: Inspect equation variables, types, complexity, etc.");
}