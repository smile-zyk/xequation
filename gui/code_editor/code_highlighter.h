#pragma once

#include "language_model.h"
#include <QStyleSyntaxHighlighter>

namespace xequation
{
namespace gui
{
class CodeHighlighter : public QStyleSyntaxHighlighter
{
    Q_OBJECT
  public:
    static CodeHighlighter* Create(LanguageModel* model, QTextDocument* document = nullptr);
    ~CodeHighlighter() override;
  protected:
    explicit CodeHighlighter(LanguageModel* model, QTextDocument* document = nullptr);
    virtual void highlightBlock(const QString& text) override;
  private:
    LanguageModel* model_;
};
} // namespace gui
} // namespace xequation