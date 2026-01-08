#include "completer_delegate.h"
#include "completer_line_edit.h"
#include <qchar.h>

namespace xequation
{
namespace gui
{
CompleterDelegate::CompleterDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

void CompleterDelegate::SetCompletionList(const QStringList &words)
{
    if(completion_model_)
    {
        delete completion_model_;
    }
    QStringListModel *model = new QStringListModel(words, this);
    completion_model_ = model;
}

void CompleterDelegate::SetCompletionModel(QAbstractItemModel* model)
{
    if(completion_model_)
    {
        delete completion_model_;
    }
    completion_model_ = model;
}

QWidget *CompleterDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto editor = new CompleterLineEdit(parent);
    editor->SetCompletionModel(completion_model_);
    return editor;
}

void CompleterDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto line_edit = qobject_cast<CompleterLineEdit *>(editor);
    if (line_edit)
    {
        line_edit->setText(index.data(Qt::EditRole).toString());
    }
}

void CompleterDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    auto line_edit = qobject_cast<CompleterLineEdit *>(editor);
    if (line_edit)
    {
        model->setData(index, line_edit->text(), Qt::EditRole);
    }
}

} // namespace gui
} // namespace xequation
