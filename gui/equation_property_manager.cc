#include "equation_property_manager.h"
#include "qtpropertybrowser.h"
#include "qtvariantproperty.h"
#include <qvariant.h>

namespace xequation
{
namespace gui
{
bool EquationPropertyManager::meta_type_registered = false;

EquationPropertyManager::EquationPropertyManager(QObject *parent) : QtVariantPropertyManager(parent)
{
    static bool registered = RegisterMetaType();
    attribute_property_manager_ = new QtVariantPropertyManager(this);
}

QVariant EquationPropertyManager::value(const QtProperty *property) const
{
    if (property_equation_map_.contains(property))
    {
        const Equation *equation = property_equation_map_.value(property);
        return QVariant::fromValue<const xequation::Equation *>(equation);
    }
    return QVariant();
}

bool EquationPropertyManager::isPropertyTypeSupported(int propertyType) const
{
    if (propertyType == qMetaTypeId<const xequation::Equation *>())
    {
        return true;
    }
    return false;
}

void EquationPropertyManager::setValue(QtProperty *property, const QVariant &val)
{
    if (property_equation_map_.contains(property))
    {
        return;
    }
    QtVariantPropertyManager::setValue(property, val);
}

void EquationPropertyManager::initializeAttributeProperties(const Equation *equation, QtProperty *equation_property)
{
    if (equation_property_to_attribute_property_map_.contains(equation_property))
    {
        return;
    }

    QtVariantProperty *content_property = attribute_property_manager_->addProperty(QVariant::String, "Content");
    QtVariantProperty *type_property = attribute_property_manager_->addProperty(QVariant::String, "Type");
    QtVariantProperty *status_property = attribute_property_manager_->addProperty(QVariant::String, "Status");
    QtVariantProperty *message_property = attribute_property_manager_->addProperty(QVariant::String, "Message");

    content_property->setVisible(true);
    type_property->setVisible(true);
    status_property->setVisible(true);
    message_property->setVisible(true);

    content_property->setValue(QString::fromStdString(equation->content()));
    type_property->setValue(QString::fromStdString(Equation::TypeToString(equation->type())));
    status_property->setValue(QString::fromStdString(Equation::StatusToString(equation->status())));
    if(equation->message().empty())
    {
        message_property->setValue(QString("No errors"));
    }
    else
    {
        message_property->setValue(QString::fromStdString(equation->message()));
    }

    equation_property_to_attribute_property_map_[equation_property] = {
        {"Content", content_property},
        {"Type", type_property},
        {"Status", status_property},
        {"Message", message_property}};

    attribute_property_to_equation_property_map_[content_property] = equation_property;
    attribute_property_to_equation_property_map_[type_property] = equation_property;
    attribute_property_to_equation_property_map_[status_property] = equation_property;
    attribute_property_to_equation_property_map_[message_property] = equation_property;

    equation_property->addSubProperty(content_property);
    equation_property->addSubProperty(type_property);
    equation_property->addSubProperty(status_property);
    equation_property->addSubProperty(message_property);
}

bool EquationPropertyManager::RegisterMetaType()
{
    if (!meta_type_registered)
    {
        qRegisterMetaType<const xequation::Equation *>("const xequation::Equation*");
        meta_type_registered = true;
    }
    return true;
}

QtProperty *EquationPropertyManager::addEquationProperty(const Equation *equation)
{
    QtVariantProperty *equation_property =
        addProperty(qMetaTypeId<const xequation::Equation *>(), QString::fromStdString(equation->name()));
    equation_property->setVisible(true);
    property_equation_map_[equation_property] = equation;
    initializeAttributeProperties(equation, equation_property);
    
    return equation_property;
}
} // namespace gui
} // namespace xequation