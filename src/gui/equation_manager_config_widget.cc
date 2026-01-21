#include "equation_manager_config_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include "python/python_highlighter.h"
#include "python/python_completion_model.h"

namespace xequation
{
namespace gui
{

EquationManagerConfigWidget::EquationManagerConfigWidget(xequation::EquationEngineInfo engine_info, QWidget *parent)
    : QWidget(parent), engine_info_(engine_info)
{
    SetupUI();
    SetupConnections();
}

EquationManagerConfigWidget::~EquationManagerConfigWidget()
{
}

void EquationManagerConfigWidget::SetupUI()
{
    setWindowFlag(Qt::Window);
    setWindowTitle("Equation Manager Config");

    auto *main_layout = new QVBoxLayout(this);

    // Startup Script Group
    startup_script_groupbox_ = new QGroupBox("Startup Script", this);
    auto *startup_layout = new QVBoxLayout(startup_script_groupbox_);
    startup_script_editor_ = new CodeEditor(QString::fromStdString(engine_info_.name), startup_script_groupbox_);
    startup_layout->addWidget(startup_script_editor_);
    main_layout->addWidget(startup_script_groupbox_);

    if(engine_info_.name == "Python")
    {
        startup_script_completion_model_ = new PythonCompletionModel(this);
        startup_script_editor_->completer()->setModel(startup_script_completion_model_);
        startup_script_highlighter_ = PythonHighlighter::Create(startup_script_editor_->document());
        startup_script_highlighter_->SetModel(startup_script_completion_model_);
        startup_script_editor_->setHighlighter(startup_script_highlighter_);
    }

    // Code Editor Group
    code_editor_groupbox_ = new QGroupBox("Code Editor", this);
    auto *editor_form_layout = new QFormLayout(code_editor_groupbox_);
    
    style_model_label_ = new QLabel("Style Model:", code_editor_groupbox_);
    style_model_combobox_ = new QComboBox(code_editor_groupbox_);
    style_model_combobox_->addItems({"Light", "Dark"});
    style_model_combobox_->setCurrentIndex(0);  // Default to Light
    editor_form_layout->addRow(style_model_label_, style_model_combobox_);
    
    scale_factor_label_ = new QLabel("Scale Factor:", code_editor_groupbox_);
    scale_factor_spinbox_ = new QSpinBox(code_editor_groupbox_);
    int min_zoom_percent = static_cast<int>(CodeEditor::GetMinZoom() * 100);
    int max_zoom_percent = static_cast<int>(CodeEditor::GetMaxZoom() * 100);
    scale_factor_spinbox_->setRange(min_zoom_percent, max_zoom_percent);
    scale_factor_spinbox_->setSuffix("%");
    scale_factor_spinbox_->setValue(100);
    scale_factor_spinbox_->setSingleStep(10);
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

    setMinimumSize(800, 600);
}

void EquationManagerConfigWidget::SetupConnections()
{
    connect(ok_button_, &QPushButton::clicked, this, &EquationManagerConfigWidget::OnOkButtonClicked);
    connect(cancel_button_, &QPushButton::clicked, this, &EquationManagerConfigWidget::OnCancelButtonClicked);
    
    // Connect startup script editor signals
    if (startup_script_editor_)
    {
        connect(startup_script_editor_, &CodeEditor::ZoomChanged, this, [this](double zoom) {
            // Update spinbox when editor zoom changes (but don't trigger the spinbox's value changed signal)
            scale_factor_spinbox_->blockSignals(true);
            scale_factor_spinbox_->setValue(static_cast<int>(zoom * 100));
            scale_factor_spinbox_->blockSignals(false);
        });
    }
    
    // Connect code editor settings to startup script editor
    connect(style_model_combobox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        QString style = style_model_combobox_->currentText();
        CodeEditor::StyleMode mode = (style == "Dark") ? CodeEditor::StyleMode::kDark : CodeEditor::StyleMode::kLight;
        startup_script_editor_->SetStyleMode(mode);
    });
    
    connect(scale_factor_spinbox_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        // Convert percentage to zoom factor (100% = 1.0)
        double zoom = value / 100.0;
        startup_script_editor_->SetZoomFactor(zoom);
    });
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
    close();
}

void EquationManagerConfigWidget::OnCancelButtonClicked()
{
    close();
}

} // namespace gui
} // namespace xequation
