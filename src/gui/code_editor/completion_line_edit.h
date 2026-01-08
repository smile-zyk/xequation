#pragma once

#include <QAbstractItemModel>
#include <QCompleter>
#include <QKeyEvent>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QStringListModel>

namespace xequation
{
namespace gui
{
class CompletionLineEdit : public QLineEdit
{
    Q_OBJECT

  public:
    enum CompletionMode
    {
        kPrefixCompletion,
        kContainsCompletion
    };

    explicit CompletionLineEdit(QWidget *parent = nullptr);
    virtual ~CompletionLineEdit() = default;

    void SetCompletionModel(QAbstractItemModel *model);

    void SetCompletionList(const QStringList &words);

    void set_word_separators(const QString &separators)
    {
        word_separators_ = separators;
    }

    void set_minimum_prefix_length(int length)
    {
        min_prefix_length_ = length;
    }

    QCompleter *completer() const
    {
        return completer_;
    }

  protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool event(QEvent *event) override;

  private slots:
    void OnCompletionActivated(const QString &completion);

  private:
    void InitCompleter();

    int FindWordStart() const;

    QString GetCurrentWord() const;

    void UpdateCompleter();

    void HandleTabKey();

    QCompleter *completer_ = nullptr;
    QString word_separators_ = " .,;:!?()[]{}+-*/=<>\"'`~|\\@#$%^&";
    int min_prefix_length_ = 1;
};
} // namespace gui
} // namespace xequation