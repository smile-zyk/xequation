#pragma once

#include <QtTreePropertyBrowser>
#include <QtVariantPropertyManager>

#include "core/equation.h"
#include "qtpropertybrowser.h"
#include "qtvariantproperty.h"

namespace xequation
{
namespace gui
{

struct EquationPropertyData
{
    std::string name;
    std::string content;
    Equation::Type type;
    Equation::Status status;
    std::string message;
    std::vector<std::string> dependencies;
};

class EquationPropertyManager : public QtVariantPropertyManager
{
    Q_OBJECT
  public:
    EquationPropertyManager(QObject *parent = nullptr);
    ~EquationPropertyManager() override{};

    QVariant value(const QtProperty *property) const override;
    
    bool hasValue(const QtProperty *property) const override { return false; }
    bool isPropertyTypeSupported(int propertyType) const override;

    void setValue(QtProperty *property, const QVariant &val) override;

    QtProperty* addEquationProperty(const Equation *equation);

  private:
    QMap<const QtProperty *, const Equation *> property_equation_map_;
    QMap<const QtProperty *, QMap<QString, QtProperty *>> equation_property_to_attribute_property_map_;
    QMap<QtProperty *, QtProperty *> attribute_property_to_equation_property_map_;

    QtVariantPropertyManager *attribute_property_manager_ = nullptr;

  private:
    static bool RegisterMetaType();
    static bool meta_type_registered;
    void initializeAttributeProperties(const Equation *equation, QtProperty *equation_property);
};
} // namespace gui
} // namespace xequation

Q_DECLARE_METATYPE(const xequation::Equation *);