#include "code_editor.h"

#include <QSyntaxStyle>

#include "code_completer.h"
#include "code_highlighter.h"

namespace xequation
{
namespace gui
{
    QMap<CodeEditor::StyleMode, QString> CodeEditor::language_style_file_map_ = {
        {CodeEditor::StyleMode::kLight, ":/styles/light_style.xml"},
        {CodeEditor::StyleMode::kDark, ":/styles/dark_style.xml"}
    };

    CodeEditor::CodeEditor(const QString& language, QWidget* parent)
        : QCodeEditor(parent), style_mode_(StyleMode::kLight)
    {
        Q_INIT_RESOURCE(code_editor_resource);
        
        language_model_ = new LanguageModel(language, this);

        CodeCompleter* completer = new CodeCompleter(language_model_, this);
        setCompleter(completer);

        CodeHighlighter* highlighter = CodeHighlighter::Create(language_model_, document());
        setHighlighter(highlighter);

        SetStyleMode(style_mode_);
    }

    CodeEditor::~CodeEditor()
    {
    }

    void CodeEditor::SetStyleMode(StyleMode mode)
    {
        if(style_map_.find(mode) == style_map_.end())
        {
            QSyntaxStyle* style = new QSyntaxStyle(this);
            QString style_file = language_style_file_map_[mode];
            QFile fl(style_file);
            if (fl.open(QIODevice::ReadOnly))
            {
                if (style->load(fl.readAll()))
                {
                    style_map_[mode] = style;
                }
                else
                {
                    delete style;
                    style_map_[mode] = QSyntaxStyle::defaultStyle();
                }
            }
            else
            {
                style_map_[mode] = QSyntaxStyle::defaultStyle();
            }
        }
        setSyntaxStyle(style_map_[mode]);
        style_mode_ = mode;
    }
}
}