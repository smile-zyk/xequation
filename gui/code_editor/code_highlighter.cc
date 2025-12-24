#include "code_highlighter.h"
#include "python_highlighter.h"
#include <QSyntaxStyle>

namespace xequation
{
namespace gui
{
CodeHighlighter* CodeHighlighter::Create(LanguageModel* model, QTextDocument* document)
{
    if (model->language_name() == "Python")
    {
        return new PythonHighlighter(model, document);
    }
    return new CodeHighlighter(model, document);
}

CodeHighlighter::CodeHighlighter(LanguageModel* model, QTextDocument* document)
    : QStyleSyntaxHighlighter(document), model_(model)
{
}

CodeHighlighter::~CodeHighlighter()
{
}

void CodeHighlighter::highlightBlock(const QString& text)
{
    int row = model_->rowCount();
    for(int i = 0; i < row; ++i)
    {
        auto index = model_->index(i, 0);
        QString word = model_->data(index, LanguageModel::kWordRole).toString();
        QString category = model_->data(index, LanguageModel::kCategoryRole).toString();
        QRegularExpression pattern("\\b" + QRegularExpression::escape(word) + "\\b");
        auto matchIterator = pattern.globalMatch(text);

        while (matchIterator.hasNext())
        {
            auto match = matchIterator.next();

            setFormat(
                match.capturedStart(),
                match.capturedLength(),
                syntaxStyle()->getFormat(category)
            );
        }
    }
}
}
}