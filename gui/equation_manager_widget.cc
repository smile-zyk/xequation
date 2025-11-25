#include "equation_manager_widget.h"
#include "core/equation.h"
#include "core/equation_group.h"
#include "core/equation_signals_manager.h"
#include <QLayout>
#include <qvariant.h>
#include <string>

namespace xequation
{
namespace gui
{

EquationManagerWidget::EquationManagerWidget(const EquationManager *manager, QWidget *parent)
    : QWidget(parent), manager_(manager)
{
    SetupUI();

    equation_added_connection_ =
        manager_->signals_manager().ConnectScoped<EquationEvent::kEquationAdded>([this](const Equation *equation) {
            OnEquationAdded(equation);
        });

    equation_removing_connection_ =
        manager_->signals_manager().ConnectScoped<EquationEvent::kEquationRemoving>([this](const Equation *equation) {
            OnEquationRemoving(equation);
        });

    equation_updated_connection_ = manager_->signals_manager().ConnectScoped<EquationEvent::kEquationUpdated>(
        [this](const Equation *equation, bitmask::bitmask<EquationUpdateFlag> change_type) {
            OnEquationUpdated(equation, change_type);
        }
    );
}

void EquationManagerWidget::OnEquationAdded(const Equation *equation)
{
    EquationPropertyItem item = CreatePropertyItem(equation);
    property_browser_->addProperty(item.name_property);
    equation_property_item_map_.insert(QString::fromStdString(equation->name()), item);
}

void EquationManagerWidget::OnEquationRemoving(const Equation *equation)
{
    QString eq_name = QString::fromStdString(equation->name());
    if (equation_property_item_map_.contains(eq_name))
    {
        EquationPropertyItem item = equation_property_item_map_.value(eq_name);
        property_browser_->removeProperty(item.name_property);
        equation_property_item_map_.remove(eq_name);
        ClearPropertySubItems(item.dependencies_group_property);
        ClearPropertySubItems(item.dependents_group_property);
        ClearPropertySubItems(item.name_property);
        delete item.name_property;
    }
}

void EquationManagerWidget::OnEquationUpdated(
    const Equation *equation, bitmask::bitmask<EquationUpdateFlag> change_type
)
{
    if (!equation_property_item_map_.contains(QString::fromStdString(equation->name())))
    {
        return;
    }

    EquationPropertyItem &item = equation_property_item_map_[QString::fromStdString(equation->name())];
    if (change_type & EquationUpdateFlag::kContent)
    {
        item.content_property->setValue(QString::fromStdString(equation->content()));
    }
    if (change_type & EquationUpdateFlag::kType)
    {
        item.type_property->setValue(QString::fromStdString(Equation::TypeToString(equation->type())));
    }
    if (change_type & EquationUpdateFlag::kStatus)
    {
        item.status_property->setValue(QString::fromStdString(Equation::StatusToString(equation->status())));
    }
    if (change_type & EquationUpdateFlag::kMessage)
    {
        item.message_property->setValue(QString::fromStdString(equation->message()));
    }
    if (change_type & EquationUpdateFlag::kDependencies)
    {
        ClearPropertySubItems(item.dependencies_group_property);
        std::string dependencies_label = "[";
        for (const auto &dep : equation->GetDependencies())
        {
            if (dependencies_label.size() > 1)
            {
                dependencies_label += ", ";
            }
            QtVariantProperty *dep_property = static_cast<QtVariantProperty *>(
                property_manager_->addProperty(QVariant::String, QString::fromStdString(dep))
            );
            dep_property->setVisible(true);
            if (equation->manager()->IsEquationExist(dep))
            {
                std::string content = equation->manager()->GetEquation(dep)->content();
                dep_property->setValue(QString::fromStdString(content));
            }
            item.dependencies_group_property->addSubProperty(dep_property);
            item.dependencies_properties.append(dep_property);
            dependencies_label += dep;
        }
        dependencies_label += "]";
        item.dependencies_group_property->setValue(QString::fromStdString(dependencies_label));
    }

    if (change_type & EquationUpdateFlag::kDependents)
    {
        ClearPropertySubItems(item.dependents_group_property);
        std::string dependents_label = "[";
        for (const auto &dep : equation->GetDependents())
        {
            if (dependents_label.size() > 1)
            {
                dependents_label += ", ";
            }
            QtVariantProperty *dep_property = static_cast<QtVariantProperty *>(
                property_manager_->addProperty(QVariant::String, QString::fromStdString(dep))
            );
            dep_property->setVisible(true);
            if (equation->manager()->IsEquationExist(dep))
            {
                std::string content = equation->manager()->GetEquation(dep)->content();
                dep_property->setValue(QString::fromStdString(content));
            }
            item.dependents_group_property->addSubProperty(dep_property);
            item.dependents_properties.append(dep_property);
            dependents_label += dep;
        }
        dependents_label += "]";
        item.dependents_group_property->setValue(QString::fromStdString(dependents_label));
    }
}

EquationManagerWidget::EquationPropertyItem EquationManagerWidget::CreatePropertyItem(const Equation *equation)
{
    EquationPropertyItem item;
    item.name_property = static_cast<QtVariantProperty *>(property_manager_->addProperty(
        QtVariantPropertyManager::groupTypeId(), QString::fromStdString(equation->name())
    ));

    item.content_property =
        static_cast<QtVariantProperty *>(property_manager_->addProperty(QVariant::String, "Content"));
    item.type_property = static_cast<QtVariantProperty *>(property_manager_->addProperty(QVariant::String, "Type"));
    item.status_property = static_cast<QtVariantProperty *>(property_manager_->addProperty(QVariant::String, "Status"));
    item.message_property =
        static_cast<QtVariantProperty *>(property_manager_->addProperty(QVariant::String, "Message"));
    item.dependencies_group_property = static_cast<QtVariantProperty *>(
        property_manager_->addProperty(QVariant::String, "Dependencies")
    );
    item.dependents_group_property = static_cast<QtVariantProperty *>(
        property_manager_->addProperty(QVariant::String, "Dependents")
    );

    item.name_property->setVisible(true);
    item.content_property->setVisible(true);
    item.type_property->setVisible(true);
    item.status_property->setVisible(true);
    item.message_property->setVisible(true);
    item.dependencies_group_property->setVisible(true);
    item.dependents_group_property->setVisible(true);

    item.content_property->setValue(QString::fromStdString(equation->content()));
    item.type_property->setValue(QString::fromStdString(Equation::TypeToString(equation->type())));
    item.status_property->setValue(QString::fromStdString(Equation::StatusToString(equation->status())));
    item.message_property->setValue(QString::fromStdString(equation->message()));

    std::string dependencies_label = "[";
    for (const auto &dep : equation->GetDependencies())
    {
        if (dependencies_label.size() > 1)
        {
            dependencies_label += ", ";
        }
        QtVariantProperty *dep_property = static_cast<QtVariantProperty *>(
            property_manager_->addProperty(QVariant::String, QString::fromStdString(dep))
        );
        dep_property->setVisible(true);
        if (equation->manager()->IsEquationExist(dep))
        {
            std::string content = equation->manager()->GetEquation(dep)->content();
            dep_property->setValue(QString::fromStdString(content));
        }
        item.dependencies_group_property->addSubProperty(dep_property);
        item.dependencies_properties.append(dep_property);
        dependencies_label += dep;
    }
    dependencies_label += "]";
    item.dependencies_group_property->setValue(QString::fromStdString(dependencies_label));

    std::string dependents_label = "[";
    for (const auto &dep : equation->GetDependents())
    {
        if (dependents_label.size() > 1)
        {
            dependents_label += ", ";
        }
        QtVariantProperty *dep_property = static_cast<QtVariantProperty *>(
            property_manager_->addProperty(QVariant::String, QString::fromStdString(dep))
        );
        dep_property->setVisible(true);
        if (equation->manager()->IsEquationExist(dep))
        {
            std::string content = equation->manager()->GetEquation(dep)->content();
            dep_property->setValue(QString::fromStdString(content));
        }
        item.dependents_group_property->addSubProperty(dep_property);
        item.dependents_properties.append(dep_property);
        dependents_label += dep;
    }
    dependents_label += "]";
    item.dependents_group_property->setValue(QString::fromStdString(dependents_label));

    item.name_property->addSubProperty(item.content_property);
    item.name_property->addSubProperty(item.type_property);
    item.name_property->addSubProperty(item.status_property);
    item.name_property->addSubProperty(item.message_property);
    item.name_property->addSubProperty(item.dependencies_group_property);
    item.name_property->addSubProperty(item.dependents_group_property);

    return item;
}

void EquationManagerWidget::ClearPropertySubItems(QtVariantProperty *property)
{
    QList<QtProperty *> sub_properties = property->subProperties();

    for (QtProperty *sub_property : sub_properties)
    {
        property->removeSubProperty(sub_property);
        delete sub_property;
    }
}

void EquationManagerWidget::SetupUI()
{
    setWindowTitle("Equation Manager");
    setWindowFlags(
        Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint
    );

    property_browser_ = new QtTreePropertyBrowser(this);
    property_manager_ = new QtVariantPropertyManager(this);
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->addWidget(property_browser_);
    setLayout(main_layout);

    property_browser_->setHeaderVisible(true);

    for (const EquationGroupId &group_id : manager_->GetEquationGroupIds())
    {
        const EquationGroup *group = manager_->GetEquationGroup(group_id);
        for (const std::string &equation_name : group->GetEquationNames())
        {
            const Equation *equation = manager_->GetEquation(equation_name);
            if (equation != nullptr)
            {
                OnEquationAdded(equation);
            }
        }
    }
    setMinimumSize(600, 400);
}

} // namespace gui
} // namespace xequation
