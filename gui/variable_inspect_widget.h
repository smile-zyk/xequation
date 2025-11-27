#pragma once
#include <QWidget>
#include <QtVariantPropertyManager>

#include "core/equation.h"
#include "core/equation_signals_manager.h"
#include "variable_property_browser.h"
#include "variable_property_manager.h"

namespace xequation
{
namespace gui {
class VariableInspectWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VariableInspectWidget(QWidget *parent = nullptr);
    virtual ~VariableInspectWidget() = default;
    
    void SetCurrentEquation(const Equation* equation);
    const Equation* current_equation() const { return current_equation_; }

    void OnEquationRemoving(const Equation* equation);
    void OnEquationUpdated(const Equation* equation, bitmask::bitmask<EquationUpdateFlag> change_type);
    void OnCurrentEquationChanged(const Equation* equation);

    void UpdateEquationProperty(const Value& value);

private:
    void SetupUI();
    void SetupConnections();

private:
    VariablePropertyBrowser *m_variable_property_browser;
    VariablePropertyManager *m_variable_property_manager;
    VariableProperty *m_variable_property{nullptr};
    const Equation* current_equation_{nullptr};
};
}
}