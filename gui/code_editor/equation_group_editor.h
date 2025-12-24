#pragma once
#include "equation_completer.h"
#include "equation_highlighter.h"
#include "equation_language_model.h"
#include "internal/QSyntaxStyle.hpp"
#include <QCodeEditor>
#include <qchar.h>

namespace xequation 
{
namespace gui 
{
    class EquationGroupEditor : public QCodeEditor 
    {
        Q_OBJECT
    public:
        enum class StyleMode
        {
            kLight,
            kDark
        };
        EquationGroupEditor(const QString& language, QWidget* parent = nullptr);
        ~EquationGroupEditor() override;
        void SetStyleMode(StyleMode mode);
    private:
        static QMap<StyleMode, QString> language_style_file_map_;
        EquationLanguageModel* language_model_;
        StyleMode style_mode_;
        QMap<StyleMode, QSyntaxStyle*> style_map_;
    };
}
}