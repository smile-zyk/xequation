#include "completion_line_edit.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QDebug>
#include <QScrollBar>
#include <qglobal.h>
#include <qnamespace.h>

namespace xequation
{
namespace gui
{
CompletionLineEdit::CompletionLineEdit(QWidget *parent) : QLineEdit(parent)
{
    InitCompleter();
}

void CompletionLineEdit::SetCompletionModel(QAbstractItemModel *model)
{
    auto old_model = completer_->model();
    if (old_model)
    {
        delete old_model;
    }
    completer_->setModel(model);
}

void CompletionLineEdit::SetCompletionList(const QStringList &words)
{
    QStringListModel *model = new QStringListModel(words, completer_);
    completer_->setModel(model);
}

void CompletionLineEdit::keyPressEvent(QKeyEvent *event)
{
    QLineEdit::keyPressEvent(event);
    if (!event->text().isEmpty())
    {
        UpdateCompleter();
    }
}

void CompletionLineEdit::focusOutEvent(QFocusEvent *event)
{
    if (completer_->popup())
    {
        completer_->popup()->hide();
    }
    QLineEdit::focusOutEvent(event);
}

bool CompletionLineEdit::event(QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *key_event = static_cast<QKeyEvent *>(event);
        if (key_event->key() == Qt::Key_A && key_event->modifiers() == Qt::ControlModifier)
        {
            if (completer_->popup())
            {
                completer_->popup()->hide();
            }
            return QLineEdit::event(event);
        }
        if (key_event->key() == Qt::Key_Tab && completer_->popup()->isVisible())
        {
            HandleTabKey();
            return true;
        }
    }

    return QLineEdit::event(event);
}

void CompletionLineEdit::OnCompletionActivated(const QString &completion)
{
    int start = FindWordStart();
    int cursor_pos = cursorPosition();

    QString text = this->text();
    setText(text.left(start) + completion + text.mid(cursor_pos));
    setCursorPosition(start + completion.length());
}

void CompletionLineEdit::InitCompleter()
{
    completer_ = new QCompleter(this);
    completer_->setCompletionMode(QCompleter::PopupCompletion);
    completer_->setCaseSensitivity(Qt::CaseSensitive);
    completer_->setWidget(this);

    QStringListModel *model = new QStringListModel(this);
    completer_->setModel(model);

    connect(
        completer_, QOverload<const QString &>::of(&QCompleter::activated), this,
        &CompletionLineEdit::OnCompletionActivated
    );
}

int CompletionLineEdit::FindWordStart() const
{
    QString text = this->text();
    int cursor_pos = cursorPosition();
    int start = cursor_pos - 1;

    while (start >= 0)
    {
        QChar c = text.at(start);
        bool is_separator = word_separators_.contains(c);

        if (!is_separator && (c.isLetterOrNumber() || c == '_'))
        {
            start--;
        }
        else
        {
            break;
        }
    }

    return start + 1;
}

QString CompletionLineEdit::GetCurrentWord() const
{
    int start = FindWordStart();
    int cursor_pos = cursorPosition();
    return this->text().mid(start, cursor_pos - start);
}

void CompletionLineEdit::UpdateCompleter()
{
    if (!completer_ || !completer_->model())
        return;

    QString word = GetCurrentWord();

    if (word.length() < min_prefix_length_)
    {
        if (completer_->popup())
            completer_->popup()->hide();
        return;
    }

    completer_->setCompletionPrefix(word);

    if (completer_->completionCount() > 0)
    {
        QRect cursor_rect = cursorRect();
        cursor_rect.setWidth(
            completer_->popup()->sizeHintForColumn(0) + completer_->popup()->verticalScrollBar()->sizeHint().width()
        );
        completer_->complete(cursor_rect);
        completer_->popup()->setCurrentIndex(completer_->completionModel()->index(0, 0));
    }
    else if (completer_->popup())
    {
        completer_->popup()->hide();
    }
}

void CompletionLineEdit::HandleTabKey()
{
    QString completion = completer_->popup()->currentIndex().data(Qt::EditRole).toString();

    emit completer_->activated(completion);

    if (completer_->popup())
    {
        completer_->popup()->hide();
    }
}

} // namespace gui
} // namespace xequation