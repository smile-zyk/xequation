#include "python_equation_highlighter.h"
#include <QSyntaxStyle>
#include <QDebug>
#include <qglobal.h>

namespace xequation
{
namespace gui
{
PythonEquationHighlighter::PythonEquationHighlighter(EquationLanguageModel *model, QTextDocument *document)
    : EquationHighlighter(model, document)
{
    // Initialize pre-model rules
    // Variables
    pre_model_rules_.append({QRegularExpression(R"((\b[a-zA-Z_][a-zA-Z0-9_]*\b))"), "Variable"});

    // Functions
    pre_model_rules_.append({QRegularExpression(R"((\b[a-zA-Z_][a-zA-Z0-9_]*\b)\()"), "Function"});

    // Class Define
    pre_model_rules_.append({QRegularExpression(R"(\bclass\b\s+([A-Za-z_][A-Za-z0-9_]*))"), "Class"});

    // Function Define
    pre_model_rules_.append({QRegularExpression(R"(\bdef\b\s+([A-Za-z_][A-Za-z0-9_]*))"), "Function"});

    // Initialize post-model rules
    // Numbers
    post_model_rules_.append({QRegularExpression(R"((\b(0b|0x){0,1}[\d.']+\b))"), "Number"});

    // Strings
    post_model_rules_.append({QRegularExpression(R"(("[^\n"]*"))"), "String"});
    post_model_rules_.append({QRegularExpression(R"(('[^\n"]*'))"), "String"});

    // Single line comment
    post_model_rules_.append({QRegularExpression("(#[^\n]*)"), "Comment"});

    // Multiline string
    block_rules_.append({
         QRegularExpression("(''')"),
         QRegularExpression("(''')"),
         "Comment"
     });
    block_rules_.append({
         QRegularExpression("(\"\"\")"),
         QRegularExpression("(\"\"\")"),
         "Comment"
     });
}
void PythonEquationHighlighter::ApplyRules(const QString &text, const QVector<QHighlightRule> &rules)
{
    for (const auto &rule : rules)
    {
        qDebug() << "Applying rule for format:" << rule.formatName;
        qDebug() << "Pattern:" << rule.pattern.pattern();
        qDebug() << "Text:" << text;

        QRegularExpression pattern = rule.pattern;
        auto matchIterator = pattern.globalMatch(text);

        while (matchIterator.hasNext())
        {
            auto match = matchIterator.next();
            qDebug() << "Matched text:" << match.captured(1) << "at position" << match.capturedStart(1);
            setFormat(match.capturedStart(1), match.capturedLength(1), syntaxStyle()->getFormat(rule.formatName));
        }
    }
}
void PythonEquationHighlighter::highlightBlock(const QString &text)
{
    // Apply pre-model rules
    ApplyRules(text, pre_model_rules_);

    // Apply model-based rules
    EquationHighlighter::highlightBlock(text);

    // Apply post-model rules
    ApplyRules(text, post_model_rules_);

    setCurrentBlockState(0);
    int startIndex = 0;
    int highlightRuleId = previousBlockState();
    if (highlightRuleId < 1 || highlightRuleId > block_rules_.size()) {
        for(int i = 0; i < block_rules_.size(); ++i) {
            startIndex = text.indexOf(block_rules_.at(i).startPattern);
            qDebug() << "Block rule" << i << "start pattern:" << block_rules_.at(i).startPattern.pattern() << "found at index:" << startIndex;
            if (startIndex >= 0) {
                highlightRuleId = i + 1;
                break;
            }
        }
    }

    while (startIndex >= 0)
    {
        const auto &blockRules = block_rules_.at(highlightRuleId - 1);
        auto match = blockRules.endPattern.match(text, startIndex+1); // Should be + length of start pattern

        int endIndex = match.capturedStart();
        int matchLength = 0;

        if (endIndex == -1)
        {
            setCurrentBlockState(highlightRuleId);
            matchLength = text.length() - startIndex;
        }
        else
        {
            matchLength = endIndex - startIndex + match.capturedLength();
        }
        qDebug() << "Applying block rule format:" << blockRules.formatName << "from index" << startIndex << "with length" << matchLength;
        setFormat(
            startIndex,
            matchLength,
            syntaxStyle()->getFormat(blockRules.formatName)
        );
        startIndex = text.indexOf(blockRules.startPattern, startIndex + matchLength);
    }
}
} // namespace gui
} // namespace xequation