#pragma once

#include <QTableView>

namespace xequation
{
class EquationValue;

namespace rel_demo
{
class DataFrameTableModel;

// =========================================================================
// DataFrameTableView -- 用于显示 DataFrame 表格的 QTableView。
//
// 通过 SetEquationValue() 传入 EquationValue（暂时仅支持 REL 值，
// 内部经 DataFrameTableModel 转换为 DataFrame 表格），并支持
// Qt 的 fetchMore 懒加载：滚动到底部时自动请求下一批行。
// =========================================================================

class DataFrameTableView : public QTableView
{
    Q_OBJECT
  public:
    explicit DataFrameTableView(QWidget *parent = nullptr);
    ~DataFrameTableView() override;

    /// 显示一个 EquationValue 的 DataFrame 视图。
    /// 仅支持 REL 值（Measurement / DataArray）；其他类型清空表格。
    void SetEquationValue(const EquationValue &value);

    /// 清空表格。
    void Clear();

    DataFrameTableModel *table_model() const { return table_model_; }

  protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private:
    void SetupUI();
    void SetupConnections();
    void OnVerticalScrollbarValueChanged(int value);
    void FetchMoreIfNeeded();

  private:
    DataFrameTableModel *table_model_ = nullptr;
};

} // namespace rel_demo
} // namespace xequation
