#pragma once

#include "value_item.h"
#include <QAbstractItemModel>

namespace xequation 
{
namespace gui 
{
class ValueModel : public QAbstractItemModel
{
    Q_OBJECT
  public:
    explicit ValueModel(QObject *parent = nullptr);
    ~ValueModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    ValueItem* GetValueItemFromIndex(const QModelIndex &index) const;
    QModelIndex GetIndexFromValueItem(ValueItem *item) const;

  private:
    QVector<ValueItem*> root_items_;
};
}
}