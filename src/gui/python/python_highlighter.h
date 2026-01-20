#include "code_editor/code_highlighter.h"
#include <QHighlightBlockRule>
#include <QHighlightRule>

namespace xequation
{
namespace gui
{
class PythonHighlighter : public CodeHighlighter
{
    Q_OBJECT
public:
    static CodeHighlighter* Create(QTextDocument* document = nullptr);
protected:
    explicit PythonHighlighter(QTextDocument* document = nullptr);
    void highlightBlock(const QString& text) override;
    void ApplyRules(const QString& text, const QVector<QHighlightRule>& rules);
protected:
    QVector<QHighlightRule> pre_model_rules_;
    QVector<QHighlightRule> post_model_rules_;
    QVector<QHighlightBlockRule> block_rules_;
};
}
}