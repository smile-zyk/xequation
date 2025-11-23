#pragma once

#include "core/equation_manager.h"
#include <QWidget>
#include <QMainWindow>
#include <memory>

#include "equation_manager_widget.h"
#include "equation_insert_editor.h"
#include "mock_equation_list_widget.h"

class QMenu;
class QAction;
class QTextEdit;
class QLabel;

class DemoWidget : public QMainWindow
{
    Q_OBJECT

public:
    explicit DemoWidget(QWidget *parent = nullptr);

private slots:
    void onOpen();
    void onInsertEquation();
    void onInsertMultiEquations();
    void onShowDependencyGraph();
    void onShowEquationManager();
    void onShowEquationInspector();

private:
    void createMenus();
    void createActions();
    
    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *viewMenu;
    
    QAction *openAction;
    QAction *exitAction;
    QAction *insertEquationAction;
    QAction *insertMultiEquationsAction;
    QAction *dependencyGraphAction;
    QAction *equationManagerAction;
    QAction *equationInspectorAction;

    xequation::gui::EquationManagerWidget* equation_manager_widget_;
    xequation::gui::EquationInsertEditor* equation_insert_editor_;
    MockEquationListWidget* mock_equation_list_widget_;
    std::unique_ptr<xequation::EquationManager> equation_manager_;
};
