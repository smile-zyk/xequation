#pragma once

#include <cstddef>
#include <string>

#include <QAbstractTableModel>

#include "core/equation_signals_manager.h"
#include "core/equation_value.h"

namespace xdataset
{
class DataFrame;
}

namespace xequation
{
class Equation;

namespace rel_demo
{

// =========================================================================
// DataFrameTableModel -- 将 EquationValue(REL) 的 DataFrame 视图展示为
// 二维表格的模型，支持 Qt 的 fetchMore 懒加载机制。
//
// 数据来源：
//   - SetEquation(): 仅支持 REL 值（Measurement / DataArray）。内部调用
//     Equation::GetValue() 取到 EquationValue，并【持有其副本】作为
//     DataFrame 的稳定 owner —— 因为 rel::Value::data_frame() 返回的是
//     由底层 DataArray 拥有的 stable reference（见 REL value.h 契约），
//     调用方必须保证该 Value 在使用 frame 期间存活。
//     其他类型（或空指针）会清空模型。
//
// 懒加载：
//   rowCount() 返回"已加载"的行数（而非 DataFrame 总行数）；
//   canFetchMore() 在已加载行数 < DataFrame 总行数时返回 true；
//   fetchMore() 每次追加 kLoadBatchSize 行，配合 QTableView 滚动
//   到底部时自动触发（Qt 内置机制 + 视图层补充触发）。
// =========================================================================

class DataFrameTableModel : public QAbstractTableModel
{
    Q_OBJECT
  public:
    /// 每次 fetchMore 追加的行数（对应 xdataset 内部 chunk 的整数倍）。
    static constexpr int kLoadBatchSize = 256;

    explicit DataFrameTableModel(QObject *parent = nullptr);
    ~DataFrameTableModel() override;

    /// 设置要显示的 Equation。
    /// 仅支持 REL 值（Measurement / DataArray）；其他类型清空模型。
    /// 注意：内部复制 Equation::GetValue() 的结果并持有该副本，
    /// 作为 DataFrame 的 owner；不持有 Equation 指针。
    void SetEquation(const Equation *equation);

    /// 清空模型，不显示任何数据。
    void Clear();

    bool HasDataFrame() const;

    /// DataFrame 总行数（0 表示无数据）。
    std::size_t total_row_count() const;

    // ---- QAbstractItemModel --------------------------------------------

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(
        int section, Qt::Orientation orientation, int role = Qt::DisplayRole
    ) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

  private:
    /// 从 EquationManager 的 kEquationRemoving 信号处接收回调（删除前触发）。
    /// 若被删除的方程正是当前显示的方程，则立即清空模型。
    void OnEquationRemoving(const Equation *equation);

    /// 绑定 / 重新绑定到 host(equation) 所属的 EquationManager 的
    /// kEquationRemoving 信号，并记录当前方程名。切换方程时先断开旧连接。
    void BindToManager(const Equation *equation);

    /// 当前持有的 EquationValue（作为 DataFrame 的稳定 owner，null 表示无数据）。
    EquationValue equation_value_;
    /// 指向 equation_value_.AsRel().data_frame() 的缓存引用（仅当有 REL 值时有效）。
    const xdataset::DataFrame *data_frame_ = nullptr;
    std::size_t loaded_rows_ = 0;  // 已加载（对外暴露）的行数

    /// 当前显示的方程名（用于在 kEquationRemoving 中比对）。
    std::string equation_name_;
    /// 与当前 manager 的 kEquationRemoving 信号的连接（model 析构时自动断开）。
    ScopedConnection removing_connection_;
};

} // namespace rel_demo
} // namespace xequation
