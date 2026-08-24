#pragma once

#include <QWidget>
#include <memory>

#include "core/equation.h"
#include "core/equation_manager.h"
#include "data_frame_table_view.h"

class QLineEdit;
class QListWidget;
class QPushButton;
class QLabel;

namespace xequation
{
namespace rel_demo
{

// =========================================================================
// RelDemoWidget -- 独立的 REL DataFrame 展示 demo。
//
// 功能：
//   - 顶部输入框：名称 + 表达式（如 `y = [1, 2, 3]`），点击 "Insert" 插入
//     单个 Equation 到 EquationManager（REL 引擎）。
//   - "Rename" 按钮：把当前选中方程（变量）重命名。会同时改写所有
//     依赖它的方程的表达式引用，并触发链式更新（依赖图 dirty 级联传播，
//     拓扑序重算）。
//   - 中部左侧：Equation 列表（QListWidget），展示已插入的方程名。
//   - 中部右侧：DataFrameTableView，选中列表项时显示该 Equation 的
//     DataFrame 表格（懒加载，fetchMore）。
// =========================================================================

class RelDemoWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit RelDemoWidget(QWidget *parent = nullptr);
    ~RelDemoWidget() override;

  private:
    void SetupUI();
    void SetupConnections();

    void OnInsertEquation();
    void OnRedefineEquation();
    void OnRenameEquation();
    void OnShowProperties();
    void OnEquationListSelectionChanged();
    void RefreshEquationList();
    void ShowEquation(const QString &equation_name);

    /// 当前选中列表项的方程名（列表项文本为 "name  [N row(s)]"）。
    QString CurrentSelectedEquationName() const;

    /// 按名称选中列表项（用于编辑/重命名后刷新选中态）。
    void SelectEquationByName(const QString &name);

    /// 解析 "name = expr" 形式的输入，返回 name（空表示解析失败）。
    static bool SplitStatement(const QString &statement, QString *name, QString *expr);

    /// 校验名字是否为合法标识符（字母/数字/下划线，不以数字开头）。
    static bool IsValidIdentifier(const QString &name);

    /// 在表达式 content 中做 token 级（词边界）替换 old -> new。
    static QString ReplaceIdentifierToken(const QString &content, const QString &old_name, const QString &new_name);

  private:
    std::unique_ptr<EquationManager> equation_manager_;

    QLineEdit *statement_edit_ = nullptr;
    QPushButton *insert_button_ = nullptr;
    QPushButton *redefine_button_ = nullptr;
    QPushButton *rename_button_ = nullptr;
    QPushButton *properties_button_ = nullptr;
    QLabel *status_label_ = nullptr;
    QListWidget *equation_list_ = nullptr;
    DataFrameTableView *data_frame_view_ = nullptr;
};

} // namespace rel_demo
} // namespace xequation
