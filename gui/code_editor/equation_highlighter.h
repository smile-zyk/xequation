#pragma once

#include "equation_language_model.h"
#include <QStyleSyntaxHighlighter>

namespace xequation
{
namespace gui
{
class EquationHighlighter : public QStyleSyntaxHighlighter
{
    Q_OBJECT
  public:
    static EquationHighlighter* Create(EquationLanguageModel* model, QTextDocument* document = nullptr);
    ~EquationHighlighter() override;
  protected:
    explicit EquationHighlighter(EquationLanguageModel* model, QTextDocument* document = nullptr);
    virtual void highlightBlock(const QString& text) override;
  private:
    EquationLanguageModel* model_;
};
} // namespace gui
} // namespace xequation