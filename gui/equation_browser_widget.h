#pragma once

#include "core/bitmask.hpp"
#include "core/equation_manager.h"
#include "core/equation_signals_manager.h"
#include <QList>
#include <QMap>
#include <QWidget>
#include <QtTreePropertyBrowser>
#include <QtVariantPropertyManager>

namespace xequation
{
class EquationManager;
namespace gui
{

class EquationBrowserWidget : public QWidget
{
    Q_OBJECT

  public:
    EquationBrowserWidget(const EquationManager *manager, QWidget *parent);
    ~EquationBrowserWidget() = default;

    void SetCurrentEquation(const Equation *equation, bool expand = true);
    const Equation *GetCurrentEquation() const;

  signals:
    void EquationSelected(const Equation *equation);

  protected:
    void SetupUI();
    void SetupConnections();
    void OnEquationAdded(const Equation *equation);
    void OnEquationRemoving(const Equation *equation);
    void OnEquationUpdated(const Equation *equation, bitmask::bitmask<EquationUpdateFlag> change_type);

    void OnBrowserItemChanged(QtBrowserItem *item);

    struct EquationPropertyItem
    {
        QtVariantProperty *name_property{nullptr};
        QtVariantProperty *content_property{nullptr};
        QtVariantProperty *type_property{nullptr};
        QtVariantProperty *status_property{nullptr};
        QtVariantProperty *message_property{nullptr};
        QtVariantProperty *dependencies_group_property{nullptr};
        QList<QtVariantProperty *> dependencies_properties;
        QtVariantProperty *dependents_group_property{nullptr};
        QList<QtVariantProperty *> dependents_properties;

        QList<QtVariantProperty *> GetMainProperties() const
        {
            return {content_property, type_property, status_property, message_property};
        }

        QList<QtVariantProperty *> GetGroupProperties() const
        {
            QList<QtVariantProperty *> groups;
            if (dependencies_group_property)
                groups.append(dependencies_group_property);
            if (dependents_group_property)
                groups.append(dependents_group_property);
            return groups;
        }

        void ClearDependencyLists()
        {
            dependencies_properties.clear();
            dependents_properties.clear();
        }
    };

  private:
    EquationPropertyItem CreatePropertyItem(const Equation *equation);
    void ClearPropertySubItems(QtVariantProperty *property);
    void UpdateDependencies(EquationPropertyItem &item, const Equation *equation);
    void UpdateDependents(EquationPropertyItem &item, const Equation *equation);
    void UpdateEquationList(
        QtVariantProperty *group_property, QList<QtVariantProperty *> &properties_list,
        const DependencyGraph::NodeNameSet &equation_names, const EquationManager *manager
    );
    QtVariantProperty *CreateBasicProperty(const QString &name, const std::string &value);
    void AddSubProperties(EquationPropertyItem &item);

  private:
    QtTreePropertyBrowser *property_browser_{nullptr};
    QtVariantPropertyManager *property_manager_{nullptr};
    const EquationManager *manager_{nullptr};
    QMap<const Equation *, EquationPropertyItem> equation_property_item_map_;
    QMap<QtProperty *, const Equation *> property_equation_map_;

    xequation::ScopedConnection equation_added_connection_;
    xequation::ScopedConnection equation_removing_connection_;
    xequation::ScopedConnection equation_updated_connection_;
};

} // namespace gui
} // namespace xequation