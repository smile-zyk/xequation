#pragma once

#include <memory>

#include <QAbstractTableModel>

namespace xdataset
{
class DataFrame;
}

namespace xequation
{
class EquationValue;

namespace rel_demo
{

// =========================================================================
// DataFrameTableModel -- 将 EquationValue(REL) 的 DataFrame 视图展示为
// 二维表格的模型，支持 Qt 的 fetchMore 懒加载机制。
//
// 数据来源：
//   - SetEquationValue(): 仅支持 REL 值（Measurement / DataArray），
//     内部调用 rel::Value::data_frame() 构建 owned DataFrame；
//     其他类型的 EquationValue 会清空模型。
//   - SetDataFrame(): 直接注入已有的 DataFrame（move-only）。
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

    /// 设置要显示的 EquationValue。
    /// 仅支持 REL 值（Measurement / DataArray）；其他类型清空模型。
    void SetEquationValue(const EquationValue &value);

    /// 直接注入 DataFrame（move-only）。行按 kLoadBatchSize 分批加载。
    void SetDataFrame(std::unique_ptr<xdataset::DataFrame> frame);

    /// 清空模型，不显示任何数据。
    void Clear();

    bool HasDataFrame() const { return data_frame_ != nullptr; }

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
    std::unique_ptr<xdataset::DataFrame> data_frame_;
    std::size_t loaded_rows_ = 0;  // 已加载（对外暴露）的行数
};

} // namespace rel_demo
} // namespace xequation
