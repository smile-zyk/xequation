#pragma once

#include "code_editor/completion_model.h"

namespace xequation
{
namespace gui
{
    class PythonCompletionModel : public CompletionModel
    {
        Q_OBJECT
      public:
        explicit PythonCompletionModel(QObject *parent = nullptr);
        ~PythonCompletionModel() override = default;
      private:
        void InitPythonKeywordAndType();
    };
}
}