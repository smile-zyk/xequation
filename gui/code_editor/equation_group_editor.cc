#include "equation_group_editor.h"
#include <QDebug>

namespace xequation
{
namespace gui
{
    QMap<EquationGroupEditor::StyleMode, QString> EquationGroupEditor::language_style_file_map_ = {
        {EquationGroupEditor::StyleMode::kLight, "C:\\dev\\PyExprEngine\\gui\\code_editor\\resources\\styles\\light_style.xml"},
        {EquationGroupEditor::StyleMode::kDark, "C:\\dev\\PyExprEngine\\gui\\code_editor\\resources\\styles\\dark_style.xml"}
    };

    EquationGroupEditor::EquationGroupEditor(const QString& language, QWidget* parent)
        : QCodeEditor(parent), style_mode_(StyleMode::kLight)
    {
        language_model_ = new EquationLanguageModel(language, this);

        EquationCompleter* completer = new EquationCompleter(language_model_, this);
        setCompleter(completer);

        EquationHighlighter* highlighter = EquationHighlighter::Create(language_model_, document());
        setHighlighter(highlighter);

        SetStyleMode(style_mode_);
    }

    EquationGroupEditor::~EquationGroupEditor()
    {
    }

    void EquationGroupEditor::SetStyleMode(StyleMode mode)
    {
        if(style_map_.find(mode) == style_map_.end())
        {
            QSyntaxStyle* style = new QSyntaxStyle(this);
            QString style_file = language_style_file_map_[mode];
            qDebug() << "Loading style file:" << style_file;
            Q_INIT_RESOURCE(code_editor_resource);
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