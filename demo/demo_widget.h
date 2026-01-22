#pragma once

#include "core/equation.h"
#include "core/equation_group.h"
#include "core/equation_manager.h"
#include <QWidget>
#include <QMainWindow>
#include <memory>
#include <vector>
#include <quuid.h>

#include "equation_browser_widget.h"
#include "variable_inspect_widget.h"
#include "expression_watch_widget.h"
#include "mock_equation_group_list_widget.h"
#include "equation_completion_model.h"
#include "equation_dependency_graph_viewer.h"
#include "equation_editor.h"
#include "equation_code_editor.h"
#include "equation_manager_config_widget.h"
#include "task/toast_task_manager.h"
#include "message_widget.h"

namespace {
enum class EditorType
{
    Normal,
    Code
};
}

class QMenu;
class QAction;
class QTextEdit;
class QLabel;

class DemoWidget : public QMainWindow
{
    Q_OBJECT

public:
    explicit DemoWidget(QWidget *parent = nullptr);
    ~DemoWidget() override;
    
    // 为Python执行器设置输出处理函数
    void SetPythonOutputHandler();

private:
    void OnOpen();
    void OnInsertEquationRequest();
    void OnEditEquationGroupRequest(const xequation::EquationGroupId& id);
    void OnRemoveEquationGroupRequest(const xequation::EquationGroupId& id);
    void OnCopyEquationGroupRequest(const xequation::EquationGroupId& id);
    void OnUpdateEquationGroupRequest(const xequation::EquationGroupId& id);
    void OnAddEquationGroupToExpressionWatchRequest(const xequation::EquationGroupId& id);
    void OnEquationGroupSelected(const xequation::EquationGroupId& id);
    void OnEquationGroupsSelected(const std::vector<xequation::EquationGroupId>& ids);
    void OnEquationSelected(const xequation::Equation* equation);
    void OnShowDependencyGraph();
    void OnShowEquationManager();
    void OnShowEquationInspector();
    void OnShowExpressionWatch();
    void OnShowEquationManagerConfig();
    void OnEquationManagerConfigAccepted(const xequation::gui::EquationManagerConfigOption& option);
    void OnCodeEditorZoomChanged(double zoom_factor);
    void OnCodeEditorStyleModeChanged(xequation::gui::CodeEditor::StyleMode mode);
    void OnParseResultRequested(const QString& expression, xequation::ParseResult &result);
    void OnEvalResultAsyncRequested(const QUuid& id, const QString& expression);
    void OnEquationDependencyGraphImageRequested();

    bool AddEquationGroup(const std::string& statement);
    bool EditEquationGroup(const xequation::EquationGroupId& id, const std::string& statement);
    bool AddEquation(const QString& equation_name, const QString& expression);
    bool EditEquation(const xequation::EquationGroupId& group_id, const QString& equation_name, const QString& expression);
    bool RemoveEquationGroup(const xequation::EquationGroupId& id);
    void AsyncUpdateEquationGroup(const xequation::EquationGroupId& id);
    void AsyncUpdateManager();
    void AsyncUpdateEquationsAfterRemoveGroup(const std::vector<std::string>& equation_names);
    
    // Slots for persistent editors
    void OnEquationEditorAddEquationRequest(const QString &equation_name, const QString &expression);
    void OnEquationEditorUseCodeEditorRequest(const QString &initial_text);
    void OnEquationEditorEditEquationRequest(const xequation::EquationGroupId &group_id, const QString &equation_name, const QString &expression);
    void OnCodeEditorAddEquationRequest(const QString &statement);
    void OnCodeEditorEditEquationRequest(const xequation::EquationGroupId& group_id, const QString &statement);
    
private:
    void SetupUI();
    void SetupConnections();

    void CreateMenus();
    void CreateActions();
    void InitCompletionModel();
    
    QMenu *file_menu_;
    QMenu *edit_menu_;
    QMenu *view_menu_;
    
    QAction *open_action_;
    QAction *exit_action_;
    QAction *insert_equation_action_;
    QAction *update_all_action_;
    QAction *show_dependency_graph_action_;
    QAction *show_equation_manager_action_;
    QAction *show_variable_inspector_action_;
    QAction *show_expression_watch_action_;
    QAction *show_equation_manager_config_action_;

    std::unique_ptr<xequation::EquationManager> equation_manager_;
    xequation::gui::ToastTaskManager* task_manager_;
    
    std::unordered_map<xequation::EquationGroupId, EditorType> editor_type_map_;
    
    MockEquationGroupListWidget* mock_equation_list_widget_;
    xequation::gui::EquationBrowserWidget* equation_browser_widget_;
    xequation::gui::VariableInspectWidget* variable_inspect_widget_;
    xequation::gui::ExpressionWatchWidget* expression_watch_widget_;
    xequation::gui::EquationCompletionModel* equation_completion_model_;
    xequation::gui::EquationDependencyGraphViewer* dependency_graph_viewer_;
    
    xequation::gui::EquationEditor* equation_editor_;
    xequation::gui::EquationCodeEditor* equation_code_editor_;
    xequation::gui::EquationManagerConfigWidget* equation_manager_config_widget_;
    
    MessageWidget* message_widget_;
    
    xequation::gui::EquationManagerConfigOption config_option_;
};
