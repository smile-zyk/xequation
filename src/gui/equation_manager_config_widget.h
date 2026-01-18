#pragma once

#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QPushButton>

#include "code_editor/code_editor.h"

namespace xequation
{
namespace gui
{

struct EquationManagerConfigOption
{
    QString startup_script;
    QString style_model;
    int scale_factor;
    bool auto_update;
};
class EquationManagerConfigWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit EquationManagerConfigWidget(QWidget *parent = nullptr);
    ~EquationManagerConfigWidget() override;
    
    EquationManagerConfigOption GetConfigOption() const;
    void SetConfigOption(const EquationManagerConfigOption& option);

signals:
    void ConfigAccepted(const EquationManagerConfigOption& option);
    
protected:
    void SetupUI();
    void SetupConnections();

private slots:
    void OnOkButtonClicked();
    void OnCancelButtonClicked();

private:
    QGroupBox* startup_script_groupbox_;
    QLabel* startup_script_label_;
    CodeEditor* startup_script_editor_;
    QGroupBox* code_editor_groupbox_;
    QLabel* style_model_label_;
    QComboBox* style_model_combobox_;
    QLabel* scale_factor_label_;
    QSpinBox* scale_factor_spinbox_;
    QGroupBox* dependency_graph_groupbox_;
    QCheckBox* auto_update_checkbox_;
    QPushButton* ok_button_;
    QPushButton* cancel_button_;
};
} // namespace gui
} // namespace xequation