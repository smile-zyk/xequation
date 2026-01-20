#include "code_highlighter.h"
#include "python/python_highlighter.h"
#include "completion_model.h"

#include <QSyntaxStyle>

namespace xequation
{
namespace gui
{
CodeHighlighter* CodeHighlighter::Create(const QString& language_name, QTextDocument* document)
{
    CodeHighlighter* result = nullptr;
    if (language_name == "Python")
    {
        result = new PythonHighlighter(document);
    }
    else 
    {
        result = new CodeHighlighter(document);
    }
    return result;
}

CodeHighlighter::CodeHighlighter(QTextDocument* document)
    : QStyleSyntaxHighlighter(document), model_(nullptr)
{
}

void CodeHighlighter::SetModel(QAbstractItemModel* model)
{
    model_ = model;
}

CodeHighlighter::~CodeHighlighter()
{
}

void CodeHighlighter::highlightBlock(const QString& text)
{
    if (!model_)
    {
        return;
    }

    int row = model_->rowCount();
    for(int i = 0; i < row; ++i)
    {
        auto index = model_->index(i, 0);
        QString word = model_->data(index, CompletionModel::kWordRole).toString();
        QString category = model_->data(index, CompletionModel::kHighlightCategoryRole).toString();
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