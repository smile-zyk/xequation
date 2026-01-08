#pragma once

#include <QCompleter>

namespace xequation {
namespace gui {
class CodeCompleter : public QCompleter
{
public:
    CodeCompleter(QObject* parent = nullptr);
    ~CodeCompleter();
};
}
}