#include "equation_highlighter.h"
#include <QHighlightBlockRule>
#include <QHighlightRule>

namespace xequation
{
namespace gui
{
class PythonEquationHighlighter : public EquationHighlighter
{
    Q_OBJECT
public:
    explicit PythonEquationHighlighter(EquationLanguageModel* model, QTextDocument* document = nullptr);
protected:
    void highlightBlock(const QString& text) override;
    void ApplyRules(const QString& text, const QVector<QHighlightRule>& rules);
protected:
    QVector<QHighlightRule> pre_model_rules_;
    QVector<QHighlightRule> post_model_rules_;
    QVector<QHighlightBlockRule> block_rules_;
};
}
}