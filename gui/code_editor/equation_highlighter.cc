#include "equation_highlighter.h"
#include "python_equation_highlighter.h"
#include <QSyntaxStyle>

namespace xequation
{
namespace gui
{
EquationHighlighter* EquationHighlighter::Create(EquationLanguageModel* model, QTextDocument* document)
{
    if (model->language_name() == "Python")
    {
        return new PythonEquationHighlighter(model, document);
    }
    return new EquationHighlighter(model, document);
}

EquationHighlighter::EquationHighlighter(EquationLanguageModel* model, QTextDocument* document)
    : QStyleSyntaxHighlighter(document), model_(model)
{
}

EquationHighlighter::~EquationHighlighter()
{
}

void EquationHighlighter::highlightBlock(const QString& text)
{
    int row = model_->rowCount();
    for(int i = 0; i < row; ++i)
    {
        auto index = model_->index(i, 0);
        QString word = model_->data(index, EquationLanguageModel::kWordRole).toString();
        QString category = model_->data(index, EquationLanguageModel::kCategoryRole).toString();
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