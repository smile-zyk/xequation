#include "code_completer.h"

namespace xequation {
namespace gui {

CodeCompleter::CodeCompleter(LanguageModel* language_model, QObject* parent)
    : QCompleter(parent), language_model_(language_model)
{
    setModel(language_model_);
    setWrapAround(true);
}

CodeCompleter::~CodeCompleter()
{
}
}
}