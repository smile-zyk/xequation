#pragma once

#include <QCompleter>

#include "equation_language_model.h"

namespace xequation {
namespace gui {
class EquationCompleter : public QCompleter
{
public:
    EquationCompleter(EquationLanguageModel* language_model, QObject* parent = nullptr);
    ~EquationCompleter();

private:
    EquationLanguageModel* language_model_;
};
}
}