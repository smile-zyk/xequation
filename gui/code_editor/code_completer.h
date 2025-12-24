#pragma once

#include <QCompleter>

#include "language_model.h"

namespace xequation {
namespace gui {
class CodeCompleter : public QCompleter
{
public:
    CodeCompleter(LanguageModel* language_model, QObject* parent = nullptr);
    ~CodeCompleter();

private:
    LanguageModel* language_model_;
};
}
}