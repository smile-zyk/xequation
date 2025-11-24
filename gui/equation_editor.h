#pragma once

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

#include "core/equation_group.h"
#include "core/equation_manager.h"

namespace xequation
{
namespace gui
{
class ContextSelectionWidget : public QWidget
{
    Q_OBJECT
  public:
    ContextSelectionWidget(const QMap<QString, QList<QString>> &data_map, QWidget *parent = nullptr);
    ~ContextSelectionWidget() {}
    QString GetSelectedVariable() const;
    void OnComboBoxChanged(const QString &text);
    void OnFilterTextChanged(const QString &text);

  private:
    void SetupUI();
    void SetupConnections();
    void UpdateListWidget(const QList<QString> &data_list);

  private:
    QMap<QString, QList<QString>> data_map_;
    QComboBox *context_combo_box_;
    QLineEdit *context_filter_edit_;
    QListWidget *context_list_widget_;
};

class EquationEditor : public QDialog
{
    Q_OBJECT
  public:
    enum class Mode
    {
      kInsert,
      kEdit
    };
    EquationEditor(const EquationManager *manager, QWidget *parent = nullptr);
    EquationEditor(const Equation* equation, QWidget* parent = nullptr);
    ~EquationEditor() {}
    void OnSuccess();

  signals:
    void AddEquationRequest(const QString& statement);
    void EditEquationRequest(const EquationGroupId& group_id, const QString& statement);

  private:
    void SetupUI();
    void SetupConnections();
    void OnContextButtonClicked();
    void OnInsertButtonClicked();
    void OnOkButtonClicked();
    void OnCancelButtonClicked();

  private:
    const EquationManager *manager_{};
    const Equation* equation_{};
    Mode mode_ = Mode::kInsert;
    QLabel *equation_name_label_{};
    QLineEdit *equation_name_edit_{};
    QLabel *expression_label_{};
    QLineEdit *expression_edit_{};
    QPushButton *context_button_{};
    ContextSelectionWidget *context_selection_widget_{};
    QPushButton *insert_button_{};
    QPushButton *ok_button_{};
    QPushButton *cancel_button_{};
};
} // namespace gui
} // namespace xequation