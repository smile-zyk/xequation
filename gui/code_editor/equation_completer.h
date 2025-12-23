#pragma once

#include <QCompleter>
#include <core/equation.h>

namespace xequation {
namespace gui {

class EquationCompleter : QCompleter
{
public:
    EquationCompleter(QObject* parent = nullptr);
    ~EquationCompleter();
};
}
}