#include "python_highlighter.h"
#include <QSyntaxStyle>
#include <qglobal.h>

namespace xequation
{
namespace gui
{
PythonHighlighter::PythonHighlighter(QTextDocument *document) : CodeHighlighter(document)
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
    block_rules_.append({QRegularExpression("(''')"), QRegularExpression("(''')"), "String"});
    block_rules_.append({QRegularExpression("(\"\"\")"), QRegularExpression("(\"\"\")"), "String"});
}

void PythonHighlighter::ApplyRules(const QString &text, const QVector<QHighlightRule> &rules)
{
    for (const auto &rule : rules)
    {
        QRegularExpression pattern = rule.pattern;
        auto matchIterator = pattern.globalMatch(text);

        while (matchIterator.hasNext())
        {
            auto match = matchIterator.next();
            setFormat(match.capturedStart(1), match.capturedLength(1), syntaxStyle()->getFormat(rule.formatName));
        }
    }
}
void PythonHighlighter::highlightBlock(const QString &text)
{
    // Apply pre-model rules
    ApplyRules(text, pre_model_rules_);

    // Apply model-based rules
    CodeHighlighter::highlightBlock(text);

    // Apply post-model rules
    ApplyRules(text, post_model_rules_);

    setCurrentBlockState(0);
    int startIndex = 0;
    int highlightRuleId = previousBlockState();

    if (highlightRuleId >= 1 && highlightRuleId <= block_rules_.size())
    {
        const auto &blockRules = block_rules_.at(highlightRuleId - 1);
        auto match = blockRules.endPattern.match(text);

        if (match.capturedStart() == -1)
        {
            setCurrentBlockState(highlightRuleId);
            setFormat(0, text.length(), syntaxStyle()->getFormat(blockRules.formatName));
            return;
        }

        int matchLength = match.capturedStart() + match.capturedLength();
        setFormat(0, matchLength, syntaxStyle()->getFormat(blockRules.formatName));
        startIndex = matchLength;
        highlightRuleId = 0;
    }

    while (startIndex < text.length())
    {
        int nextStartIndex = -1;
        int nextRuleId = 0;

        for (int i = 0; i < block_rules_.size(); ++i)
        {
            auto match = block_rules_.at(i).startPattern.match(text, startIndex);
            if (match.hasMatch())
            {
                int matchStart = match.capturedStart();
                if (nextStartIndex == -1 || matchStart < nextStartIndex)
                {
                    nextStartIndex = matchStart;
                    nextRuleId = i + 1;
                }
            }
        }

        if (nextStartIndex == -1)
            break;

        const auto &blockRules = block_rules_.at(nextRuleId - 1);
        auto startMatch = blockRules.startPattern.match(text, nextStartIndex);
        int afterStart = startMatch.capturedStart() + startMatch.capturedLength();

        auto endMatch = blockRules.endPattern.match(text, afterStart);

        if (endMatch.hasMatch())
        {
            int blockEnd = endMatch.capturedStart() + endMatch.capturedLength();
            setFormat(
                startMatch.capturedStart(), blockEnd - startMatch.capturedStart(),
                syntaxStyle()->getFormat(blockRules.formatName)
            );
            startIndex = blockEnd;
        }
        else
        {
            setCurrentBlockState(nextRuleId);
            setFormat(
                startMatch.capturedStart(), text.length() - startMatch.capturedStart(),
                syntaxStyle()->getFormat(blockRules.formatName)
            );
            break;
        }
    }
}
} // namespace gui
} // namespace xequation