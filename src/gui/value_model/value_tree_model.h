#pragma once

#include "value_item.h"
#include <QAbstractItemModel>

namespace xequation 
{
namespace gui 
{
class ValueTreeModel : public QAbstractItemModel
{
    Q_OBJECT
  public:
    constexpr static int kLoadBatchSize = 50;
    explicit ValueTreeModel(QObject *parent = nullptr);
    ~ValueTreeModel() override = default;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;
    ValueItem* GetValueItemFromIndex(const QModelIndex &index) const;
    QModelIndex GetIndexFromValueItem(ValueItem *item) const;

    void AddRootItem(ValueItem* item);
    void RemoveRootItem(ValueItem* item);
    void SetRootItems(const QVector<ValueItem*>& items);
    size_t GetRootItemCount() const { return root_items_.size(); }
    ValueItem* GetRootItemAt(size_t index) const;
    void Clear();

  protected:

    QVector<ValueItem*> root_items_;
};
}
}