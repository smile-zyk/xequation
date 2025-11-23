#pragma once

#include "core/bitmask.hpp"
#include "core/equation_manager.h"
#include "core/equation_signals_manager.h"
#include <QWidget>
#include <QttreePropertyBrowser>
#include <QtvariantPropertyManager>

namespace xequation
{
class EquationManager;
namespace gui
{

class EquationManagerWidget : public QWidget
{
  public:
    EquationManagerWidget(const EquationManager *manager, QWidget *parent);
    ~EquationManagerWidget() = default;

  private:
    void SetupUI();
    void OnEquationAdded(const Equation *equation);
    void OnEquationRemoving(const Equation *equation);
    void OnEquationUpdated(const Equation *equation, bitmask::bitmask<EquationUpdateFlag> change_type);

  private:
    struct EquationPropertyItem
    {
        QtVariantProperty *name_property;
        QtVariantProperty *content_property;
        QtVariantProperty *type_property;
        QtVariantProperty *status_property;
        QtVariantProperty *message_property;
        QtVariantProperty *dependencies_group_property;
        QList<QtVariantProperty*> dependencies_properties;
        QtVariantProperty *dependents_group_property;
        QList<QtVariantProperty*> dependents_properties;
    };
    EquationPropertyItem CreatePropertyItem(const Equation *equation);
    void ClearPropertySubItems(QtVariantProperty *property);
  private:
    QtTreePropertyBrowser *property_browser_;
    QtVariantPropertyManager *property_manager_;
    const EquationManager *manager_;
    QMap<QString, EquationPropertyItem> equation_property_item_map_;

    xequation::ScopedConnection equation_added_connection_;
    xequation::ScopedConnection equation_removing_connection_;
    xequation::ScopedConnection equation_updated_connection_;
};
} // namespace gui
} // namespace xequation