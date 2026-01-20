#pragma once

#include <QAbstractItemModel>
#include <QStyleSyntaxHighlighter>

namespace xequation
{
namespace gui
{
class CodeHighlighter : public QStyleSyntaxHighlighter
{
    Q_OBJECT
  public:
    ~CodeHighlighter() override;
    void SetModel(QAbstractItemModel* model);
  protected:
    explicit CodeHighlighter(QTextDocument* document = nullptr);
    virtual void highlightBlock(const QString& text) override;
  private:
    QAbstractItemModel* model_;
};
} // namespace gui
} // namespace xequation