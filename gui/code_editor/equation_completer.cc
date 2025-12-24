#include "equation_completer.h"

namespace xequation {
namespace gui {

EquationCompleter::EquationCompleter(EquationLanguageModel* language_model, QObject* parent)
    : QCompleter(parent), language_model_(language_model)
{
    setModel(language_model_);
    setWrapAround(true);
}

EquationCompleter::~EquationCompleter()
{
}
}
}