#include "equation_manager_config_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

namespace xequation
{
namespace gui
{

EquationManagerConfigWidget::EquationManagerConfigWidget(QWidget *parent)
    : QWidget(parent)
{
    SetupUI();
    SetupConnections();
}

EquationManagerConfigWidget::~EquationManagerConfigWidget()
{
}

void EquationManagerConfigWidget::SetupUI()
{
    auto *main_layout = new QVBoxLayout(this);

    // Startup Script Group
    startup_script_groupbox_ = new QGroupBox("Startup Script", this);
    auto *startup_layout = new QVBoxLayout(startup_script_groupbox_);
    startup_script_label_ = new QLabel("Script to execute on startup:", startup_script_groupbox_);
    startup_script_editor_ = new CodeEditor(startup_script_groupbox_);
    startup_layout->addWidget(startup_script_label_);
    startup_layout->addWidget(startup_script_editor_);
    main_layout->addWidget(startup_script_groupbox_);

    // Code Editor Group
    code_editor_groupbox_ = new QGroupBox("Code Editor", this);
    auto *editor_form_layout = new QFormLayout(code_editor_groupbox_);
    
    style_model_label_ = new QLabel("Style Model:", code_editor_groupbox_);
    style_model_combobox_ = new QComboBox(code_editor_groupbox_);
    style_model_combobox_->addItems({"Dark", "Light", "Auto"});
    editor_form_layout->addRow(style_model_label_, style_model_combobox_);
    
    scale_factor_label_ = new QLabel("Scale Factor:", code_editor_groupbox_);
    scale_factor_spinbox_ = new QSpinBox(code_editor_groupbox_);
    scale_factor_spinbox_->setRange(50, 200);
    scale_factor_spinbox_->setSuffix("%");
    scale_factor_spinbox_->setValue(100);
    editor_form_layout->addRow(scale_factor_label_, scale_factor_spinbox_);
    
    main_layout->addWidget(code_editor_groupbox_);

    // Dependency Graph Group
    dependency_graph_groupbox_ = new QGroupBox("Dependency Graph", this);
    auto *graph_layout = new QVBoxLayout(dependency_graph_groupbox_);
    auto_update_checkbox_ = new QCheckBox("Auto Update", dependency_graph_groupbox_);
    auto_update_checkbox_->setChecked(true);
    graph_layout->addWidget(auto_update_checkbox_);
    main_layout->addWidget(dependency_graph_groupbox_);

    // Button Layout
    auto *button_layout = new QHBoxLayout();
    button_layout->addStretch();
    ok_button_ = new QPushButton("OK", this);
    cancel_button_ = new QPushButton("Cancel", this);
    button_layout->addWidget(ok_button_);
    button_layout->addWidget(cancel_button_);
    main_layout->addLayout(button_layout);

    setLayout(main_layout);
}

void EquationManagerConfigWidget::SetupConnections()
{
    connect(ok_button_, &QPushButton::clicked, this, &EquationManagerConfigWidget::OnOkButtonClicked);
    connect(cancel_button_, &QPushButton::clicked, this, &EquationManagerConfigWidget::OnCancelButtonClicked);
}

EquationManagerConfigOption EquationManagerConfigWidget::GetConfigOption() const
{
    EquationManagerConfigOption option;
    option.startup_script = startup_script_editor_->toPlainText();
    option.style_model = style_model_combobox_->currentText();
    option.scale_factor = scale_factor_spinbox_->value();
    option.auto_update = auto_update_checkbox_->isChecked();
    return option;
}

void EquationManagerConfigWidget::SetConfigOption(const EquationManagerConfigOption& option)
{
    startup_script_editor_->setPlainText(option.startup_script);
    
    int index = style_model_combobox_->findText(option.style_model);
    if (index >= 0)
    {
        style_model_combobox_->setCurrentIndex(index);
    }
    
    scale_factor_spinbox_->setValue(option.scale_factor);
    auto_update_checkbox_->setChecked(option.auto_update);
}

void EquationManagerConfigWidget::OnOkButtonClicked()
{
    EquationManagerConfigOption option = GetConfigOption();
    emit ConfigAccepted(option);
}

void EquationManagerConfigWidget::OnCancelButtonClicked()
{
    // 如果这个widget是在对话框中，应该关闭父对话框
    // 这里可以emit一个cancel信号，或者直接关闭父widget
    if (parentWidget())
    {
        parentWidget()->close();
    }
}

} // namespace gui
} // namespace xequation
