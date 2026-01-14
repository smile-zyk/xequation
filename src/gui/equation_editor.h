#pragma once

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>
#include <qcompleter.h>

#include "core/equation_group.h"
#include "code_editor/completion_line_edit.h"
#include "equation_completion_model.h"

namespace xequation
{
namespace gui
{
class ContextSelectionWidget : public QWidget
{
    Q_OBJECT
  public:
    ContextSelectionWidget(EquationCompletionFilterModel* model, QWidget *parent = nullptr);
    ~ContextSelectionWidget() {}
    QString GetSelectedVariable() const;
    void OnComboBoxChanged(const QString &text);
    void OnFilterTextChanged(const QString &text);

  signals:
    void VariableDoubleClicked(const QString& variable);

  protected:
    void SetupUI();
    void SetupConnections();

  private:
    void OnListViewDoubleClicked(const QModelIndex &index);

    EquationCompletionFilterModel* model_{};
    QComboBox *context_combo_box_;
    QLineEdit *context_filter_edit_;
    QListView *context_list_view_;
};

class EquationEditor : public QDialog
{
    Q_OBJECT
  public:
    EquationEditor(EquationCompletionModel* language_model, QWidget *parent = nullptr);
    ~EquationEditor() {}
    void SetEquationGroup(const EquationGroup* group);

  signals:
    void AddEquationRequest(const QString& equation_name, const QString& expression);
    void EditEquationRequest(const EquationGroupId& group_id, const QString& equation_name, const QString& expression);
    void VariableInsertRequested(const QString& variable);
    void SwitchToGroupEditorRequest(const QString& initial_text);

  private:
    void SetupUI();
    void SetupConnections();
    void OnContextButtonClicked();
    void OnInsertButtonClicked();
    void OnVariableDoubleClicked(const QString& variable);
    void OnSwitchToGroupEditorClicked();
    void OnOkButtonClicked();

  private:
    const EquationGroup* group_{};
    EquationCompletionFilterModel* context_selection_filter_model_{};
    EquationCompletionFilterModel* completion_filter_model_{};
    QLabel *equation_name_label_{};
    QLineEdit *equation_name_edit_{};
    QLabel *expression_label_{};
    CompletionLineEdit *expression_edit_{};
    QPushButton *context_button_{};
    ContextSelectionWidget *context_selection_widget_{};
    QPushButton *insert_button_{};    QPushButton *switch_to_group_button_;    QPushButton *ok_button_{};
    QPushButton *cancel_button_{};
};
} // namespace gui
} // namespace xequation