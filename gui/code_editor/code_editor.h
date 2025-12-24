#pragma once
#include "language_model.h"
#include <QCodeEditor>

namespace xequation 
{
namespace gui 
{
    class CodeEditor : public QCodeEditor 
    {
        Q_OBJECT
    public:
        enum class StyleMode
        {
            kLight,
            kDark
        };
        CodeEditor(const QString& language, QWidget* parent = nullptr);
        ~CodeEditor() override;
        void SetStyleMode(StyleMode mode);
    private:
        static QMap<StyleMode, QString> language_style_file_map_;
        LanguageModel* language_model_;
        StyleMode style_mode_;
        QMap<StyleMode, QSyntaxStyle*> style_map_;
    };
}
}