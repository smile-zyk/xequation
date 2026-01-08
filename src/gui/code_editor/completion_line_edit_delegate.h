#pragma once

#include <QStyledItemDelegate>
#include <QStringList>
#include <qabstractitemmodel.h>

namespace xequation
{
namespace gui
{
class CompletionLineEditDelegate : public QStyledItemDelegate
{
    Q_OBJECT

  public:
    explicit CompletionLineEditDelegate(QObject *parent = nullptr);

    void SetCompletionList(const QStringList &words);

    void SetCompletionModel(QAbstractItemModel* model);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

  private:
    QAbstractItemModel* completion_model_{nullptr};
};
} // namespace gui
} // namespace xequation
