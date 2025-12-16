#pragma once

#include "core/equation.h"
#include "core/equation_group.h"
#include "core/equation_manager.h"
#include <QWidget>
#include <QMainWindow>
#include <memory>
#include <unordered_set>

#include "equation_browser_widget.h"
#include "mock_equation_group_list_widget.h"
#include "value_model_view/value_tree_view.h"

class QMenu;
class QAction;
class QTextEdit;
class QLabel;

class DemoWidget : public QMainWindow
{
    Q_OBJECT

public:
    explicit DemoWidget(QWidget *parent = nullptr);

private:
    void OnOpen();
    void OnInsertEquationRequest();
    void OnEditEquationGroupRequest(const xequation::EquationGroupId& id);
    void OnRemoveEquationGroupRequest(const xequation::EquationGroupId& id);
    void OnCopyEquationGroupRequest(const xequation::EquationGroupId& id);
    void OnEquationGroupSelected(const xequation::EquationGroupId& id);
    void OnEquationSelected(const xequation::Equation* equation);
    void OnInsertEquationGroupRequest();
    void OnShowDependencyGraph();
    void OnShowEquationManager();
    void OnShowEquationInspector();

    bool AddEquationGroup(const std::string& statement, xequation::EquationGroupId& id);
    bool EditEquationGroup(const xequation::EquationGroupId& id, const std::string& statement);
    bool RemoveEquationGroup(const xequation::EquationGroupId& id);
    
private:
    void SetupUI();
    void SetupConnections();

    void CreateMenus();
    void CreateActions();
    
    QMenu *file_menu_;
    QMenu *edit_menu_;
    QMenu *view_menu_;
    
    QAction *open_action_;
    QAction *exit_action_;
    QAction *insert_equation_action_;
    QAction *insert_equation_group_action_;
    QAction *show_dependency_graph_action_;
    QAction *show_equation_manager_action_;
    QAction *show_variable_inspector_action_;
    QAction *show_variable_monitor_action_;

    std::unordered_set<xequation::EquationGroupId> single_equation_set_;
    std::unordered_set<xequation::EquationGroupId> equation_group_set_;
    xequation::gui::EquationBrowserWidget* equation_browser_widget_;
    xequation::gui::ValueTreeView* variable_tree_;
    MockEquationGroupListWidget* mock_equation_list_widget_;
    std::unique_ptr<xequation::EquationManager> equation_manager_;
};
