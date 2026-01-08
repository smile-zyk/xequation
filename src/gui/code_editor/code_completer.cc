#include "code_completer.h"

namespace xequation {
namespace gui {

CodeCompleter::CodeCompleter(QObject* parent)
    : QCompleter(parent)
{
    setWrapAround(true);
}

CodeCompleter::~CodeCompleter()
{
}
}
}