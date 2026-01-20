#include "python_completion_model.h"

namespace xequation
{
namespace gui
{
PythonCompletionModel::PythonCompletionModel(QObject *parent)
    : CompletionModel(parent)
{
    InitPythonKeywordAndType();
}

void PythonCompletionModel::InitPythonKeywordAndType()
{
    // keywords
    AddCompletionItem("None", "keyword", "Keyword");
    AddCompletionItem("True", "keyword", "Keyword");
    AddCompletionItem("False", "keyword", "Keyword");
    AddCompletionItem("and", "keyword", "Keyword");
    AddCompletionItem("as", "keyword", "Keyword");
    AddCompletionItem("assert", "keyword", "Keyword");
    AddCompletionItem("break", "keyword", "Keyword");
    AddCompletionItem("class", "keyword", "Keyword");
    AddCompletionItem("continue", "keyword", "Keyword");
    AddCompletionItem("def", "keyword", "Keyword");
    AddCompletionItem("del", "keyword", "Keyword");
    AddCompletionItem("elif", "keyword", "Keyword");
    AddCompletionItem("else", "keyword", "Keyword");
    AddCompletionItem("except", "keyword", "Keyword");
    AddCompletionItem("finally", "keyword", "Keyword");
    AddCompletionItem("for", "keyword", "Keyword");
    AddCompletionItem("from", "keyword", "Keyword");
    AddCompletionItem("global", "keyword", "Keyword");
    AddCompletionItem("if", "keyword", "Keyword");
    AddCompletionItem("import", "keyword", "Keyword");
    AddCompletionItem("in", "keyword", "Keyword");
    AddCompletionItem("is", "keyword", "Keyword");
    AddCompletionItem("lambda", "keyword", "Keyword");
    AddCompletionItem("nonlocal", "keyword", "Keyword");
    AddCompletionItem("not", "keyword", "Keyword");
    AddCompletionItem("or", "keyword", "Keyword");
    AddCompletionItem("pass", "keyword", "Keyword");
    AddCompletionItem("raise", "keyword", "Keyword");
    AddCompletionItem("return", "keyword", "Keyword");
    AddCompletionItem("try", "keyword", "Keyword");
    AddCompletionItem("while", "keyword", "Keyword");
    AddCompletionItem("with", "keyword", "Keyword");
    AddCompletionItem("yield", "keyword", "Keyword");

    // built-in types
    AddCompletionItem("bool", "type", "Type");
    AddCompletionItem("int", "type", "Type");
    AddCompletionItem("float", "type", "Type");
    AddCompletionItem("complex", "type", "Type");
    AddCompletionItem("str", "type", "Type");
    AddCompletionItem("bytes", "type", "Type");
    AddCompletionItem("bytearray", "type", "Type");
    AddCompletionItem("memoryview", "type", "Type");
    AddCompletionItem("list", "type", "Type");
    AddCompletionItem("tuple", "type", "Type");
    AddCompletionItem("range", "type", "Type");
    AddCompletionItem("set", "type", "Type");
    AddCompletionItem("frozenset", "type", "Type");
    AddCompletionItem("dict", "type", "Type");
    AddCompletionItem("type", "type", "Type");
    AddCompletionItem("object", "type", "Type");
}
}
}