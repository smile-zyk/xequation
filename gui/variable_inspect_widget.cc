#include "variable_inspect_widget.h"
#include "python/converters/basic_python_converter.h"
#include "python/converters/python_property_converter.h"

#include <QVBoxLayout>

namespace xequation
{
namespace gui
{
namespace python
{
REGISTER_PYTHON_PROPERTY_CONVERTER(BasicPythonPropertyConverter,  0);
REGISTER_PYTHON_PROPERTY_CONVERTER(ListPropertyConverter, 1);
REGISTER_PYTHON_PROPERTY_CONVERTER(DefaultPythonPropertyConverter, 100);
} // namespace python

VariableInspectWidget::VariableInspectWidget(QWidget *parent)
    : QWidget(parent),
      m_variable_property_browser(new VariablePropertyBrowser(this)),
      m_variable_property_manager(new VariablePropertyManager(this))
{
    SetupUI();
    SetupConnections();
}

void VariableInspectWidget::SetupUI()
{
    setWindowTitle("Variable Inspector");
    setWindowFlags(
        Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint
    );

    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->addWidget(m_variable_property_browser);
    setLayout(main_layout);

    setMinimumSize(800, 600);
    m_variable_property_browser->setHeaderSectionResizeRatio(0, 2);
    m_variable_property_browser->setHeaderSectionResizeRatio(1, 2);
    m_variable_property_browser->setHeaderSectionResizeRatio(2, 1);
}

void VariableInspectWidget::SetupConnections() {}

void VariableInspectWidget::OnCurrentEquationChanged(const Equation *equation)
{
    SetCurrentEquation(equation);
}

void DeleteVariableProperty(VariableProperty *property)
{
    if (property)
    {
        QList<QtProperty*> subProperties = property->subProperties();
        for (QtProperty* subProperty : subProperties)
        {
            DeleteVariableProperty(static_cast<VariableProperty*>(subProperty));
        }

        property->subProperties().clear();
        
        delete property;
    }
    property = nullptr;
}

void VariableInspectWidget::SetCurrentEquation(const Equation *equation)
{
    current_equation_ = equation;
    if (current_equation_)
    {
        Value value = current_equation_->GetValue();
        UpdateEquationProperty(value);
    }
    else
    {
        DeleteVariableProperty(m_variable_property);
        m_variable_property_browser->clear();
    }
}

void VariableInspectWidget::OnEquationRemoving(const Equation *equation)
{
    if (equation == current_equation_)
    {
        SetCurrentEquation(nullptr);
    }
}

void VariableInspectWidget::OnEquationUpdated(
    const Equation *equation, bitmask::bitmask<EquationUpdateFlag> change_type
)
{
    if (equation == current_equation_ && change_type & EquationUpdateFlag::kValue)
    {
        UpdateEquationProperty(equation->GetValue());
    }
}

void VariableInspectWidget::UpdateEquationProperty(const Value &value)
{
    if (value.IsNull())
    {
        DeleteVariableProperty(m_variable_property);
        m_variable_property =
            m_variable_property_manager->addProperty(QString::fromStdString(current_equation_->name()));
        m_variable_property_manager->setValue(
            m_variable_property, "undefined identifier '" + QString::fromStdString(current_equation_->name()) + "'"
        );
        m_variable_property_browser->clear();
        m_variable_property_browser->addProperty(m_variable_property);
    }

    if (value.Type() == typeid(pybind11::object))
    {
        pybind11::object obj = value.Cast<pybind11::object>();
        VariableProperty *property = python::CreatePythonProperty(
            m_variable_property_manager, QString::fromStdString(current_equation_->name()), obj
        );
        if (property)
        {
            DeleteVariableProperty(m_variable_property);
            m_variable_property = property;
            m_variable_property = property;
            m_variable_property_browser->clear();
            m_variable_property_browser->addProperty(m_variable_property);
        }
    }
}

} // namespace gui
} // namespace xequation