#pragma once

#include <QDialog>
#include <QToolBar>
#include <qcompleter.h>

#include "code_editor/code_editor.h"
#include "code_editor/code_highlighter.h"
#include "equation_completion_model.h"

namespace xequation 
{
namespace gui
{
class EquationCodeEditor : public QDialog
{
    Q_OBJECT
public:
    explicit EquationCodeEditor(EquationCompletionModel* language_model, QWidget *parent = nullptr);
    ~EquationCodeEditor();

    void SetEquationGroup(const EquationGroup* group);
    QString GetText() const;
    void SetText(const QString &text);
    void ClearText();
    void SetEditorZoomFactor(double zoom_factor);
    void SetEditorStyleMode(CodeEditor::StyleMode mode);
    
signals:
    void AddEquationRequest(const QString &text);
    void EditEquationRequest(const EquationGroupId& group_id, const QString &statement);
    void StyleModeChanged(CodeEditor::StyleMode mode);
    void ZoomChanged(double zoom_factor);
protected:
    void SetupUI();
    void SetupConnections();
    void done(int result) override;
    void keyPressEvent(QKeyEvent *event) override;

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
    QString language_name_{};
    EquationCompletionFilterModel* equation_completion_filter_model_{};
    
    const EquationGroup* group_{};  // Store group for edit mode

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