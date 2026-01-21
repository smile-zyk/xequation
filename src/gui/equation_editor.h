#pragma once

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>
#include <qcompleter.h>
#include <qidentityproxymodel.h>
#include <qsortfilterproxymodel.h>
#include <qvariant.h>

#include "code_editor/completion_line_edit.h"
#include "core/equation_group.h"
#include "equation_completion_model.h"


namespace xequation
{
namespace gui
{

class ContextSelectionWidget : public QWidget
{
    Q_OBJECT
  public:
    ContextSelectionWidget(EquationCompletionModel *model, QWidget *parent = nullptr);
    ~ContextSelectionWidget() {}
    QString GetSelectedVariable() const;
    void OnComboBoxChanged(const QString &text);
    void OnFilterTextChanged(const QString &text);
    void ResetState();        // Reset all state
    void ResetFilters();      // Reset filter text and combo box
    void RefreshCategories(); // Reload categories from model

  signals:
    void VariableDoubleClicked(const QString &variable);

  protected:
    void SetupUI();
    void SetupConnections();

  private:
    class ContextFilterProxyModel : public QSortFilterProxyModel
    {
      public:
        ContextFilterProxyModel(EquationCompletionModel *model, QObject *parent);
        QVector<QString> GetAllCategories() const;
        void SetFilterCategory(const QString &category);
        void SetFilterText(const QString &text);

      protected:
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

      private:
        QString filter_category_;
        QString filter_text_;
    };

    void OnListViewDoubleClicked(const QModelIndex &index);

    ContextFilterProxyModel *proxy_model_{};
    EquationCompletionModel *model_{};
    QComboBox *context_combo_box_;
    QLineEdit *context_filter_edit_;
    QListView *context_list_view_;
};

class EquationEditor : public QDialog
{
    Q_OBJECT
  public:
    EquationEditor(EquationCompletionModel *language_model, QWidget *parent = nullptr);
    ~EquationEditor() {}
    void SetEquationGroup(const EquationGroup *group);
    void ClearText();
    void ResetState(); // Reset all state except text fields
    const EquationGroup *equation_group() const
    {
        return group_;
    }
  signals:
    void AddEquationRequest(const QString &equation_name, const QString &expression);
    void EditEquationRequest(const EquationGroupId &group_id, const QString &equation_name, const QString &expression);
    void VariableInsertRequested(const QString &variable);
    void UseCodeEditorRequest(const QString &initial_text);

  private:
    void SetupUI();
    void SetupConnections();
    void OnContextButtonClicked();
    void OnInsertButtonClicked();
    void OnVariableDoubleClicked(const QString &variable);
    void OnSwitchToGroupEditorClicked();
    void OnOkButtonClicked();
    void done(int result) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

  private:
    const EquationGroup *group_{};
    EquationCompletionModel *completion_model_{};
    QLabel *equation_name_label_{};
    QLineEdit *equation_name_edit_{};
    QLabel *expression_label_{};
    CompletionLineEdit *expression_edit_{};
    QPushButton *context_button_{};
    ContextSelectionWidget *context_selection_widget_{};
    QPushButton *insert_button_{};
    QPushButton *switch_to_group_button_;
    QPushButton *ok_button_{};
    QPushButton *cancel_button_{};
};
} // namespace gui
} // namespace xequation