#pragma once

#include <QDialog>
#include <QToolBar>

#include "code_editor/code_completer.h"
#include "code_editor/code_editor.h"
#include "code_editor/code_highlighter.h"
#include "equation_completion_model.h"

namespace xequation 
{
namespace gui 
{
class EquationGroupEditor : public QDialog
{
    Q_OBJECT
public:
    explicit EquationGroupEditor(EquationCompletionModel* language_model, QWidget *parent = nullptr, const QString &title = "text editor");
    ~EquationGroupEditor();

    void SetEquationGroup(const EquationGroup* group);
    QString GetText() const;
    void SetText(const QString &text);
    void ClearText();
signals:
    void TextSubmitted(const QString &text);
protected:
    void SetupUI();
    void SetupConnections();

    void OnOpen();
    void OnSave();
    void OnReset();
    void OnUndo();
    void OnRedo();
    void OnSwitchMode();
    void OnOkClicked();
    void OnCancelClicked();

private:

    QToolBar *tool_bar_{};

    CodeEditor *editor_{};
    CodeHighlighter* editor_highlighter_{};
    CodeCompleter* editor_completer_{};
    QString language_name_{};
    EquationCompletionFilterModel* equation_completion_filter_model_{};

    QPushButton *ok_button_{};
    QPushButton *cancel_button_{};
    // actions
    QAction *open_action_{};
    QAction *save_action_{};
    QAction *reset_action_{};
    QAction *undo_action_{};
    QAction *redo_action_{};
    QAction *switch_mode_action_{};
};
}
}