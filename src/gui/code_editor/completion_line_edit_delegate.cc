#include "completion_line_edit_delegate.h"
#include "completion_line_edit.h"
#include <qchar.h>

namespace xequation
{
namespace gui
{
CompletionLineEditDelegate::CompletionLineEditDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

void CompletionLineEditDelegate::SetCompletionList(const QStringList &words)
{
    if(completion_model_)
    {
        delete completion_model_;
    }
    QStringListModel *model = new QStringListModel(words, this);
    completion_model_ = model;
}

void CompletionLineEditDelegate::SetCompletionModel(QAbstractItemModel* model)
{
    if(completion_model_)
    {
        delete completion_model_;
    }
    completion_model_ = model;
}

QWidget *CompletionLineEditDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto editor = new CompletionLineEdit(parent);
    editor->SetCompletionModel(completion_model_);
    return editor;
}

void CompletionLineEditDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto line_edit = qobject_cast<CompletionLineEdit *>(editor);
    if (line_edit)
    {
        line_edit->setText(index.data(Qt::EditRole).toString());
    }
}

void CompletionLineEditDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    auto line_edit = qobject_cast<CompletionLineEdit *>(editor);
    if (line_edit)
    {
        model->setData(index, line_edit->text(), Qt::EditRole);
    }
}

} // namespace gui
} // namespace xequation
