#include "equation_browser_widget.h"
#include "core/equation.h"
#include "core/equation_common.h"
#include <QLayout>
#include <QVariant>
#include <string>
#include <tsl/ordered_set.h>

namespace xequation
{
namespace gui
{

EquationBrowserWidget::EquationBrowserWidget(QWidget *parent)
    : QWidget(parent)
{
    SetupUI();
    SetupConnections();
}

QString truncateText(const QString& text, int maxLength = 50) {
    if (text.isEmpty()) return text;
    
    QString result = text;
    
    int newlinePos = result.indexOf('\n');
    if (newlinePos != -1) {
        result = result.left(newlinePos) + "...";
    } else {
        if (result.length() > maxLength) {
            result = result.left(maxLength) + "...";
        }
    }
    
    return result;
}

void EquationBrowserWidget::OnEquationAdded(const Equation *equation)
{
    EquationPropertyItem item = CreatePropertyItem(equation);
    auto browser_item = property_browser_->addProperty(item.name_property);
    property_browser_->setExpanded(browser_item, false);
    equation_property_item_map_.insert(equation, item);
    property_equation_map_.insert(item.name_property, equation);
}

void EquationBrowserWidget::OnEquationRemoving(const Equation *equation)
{
    if (!equation_property_item_map_.contains(equation))
    {
        return;
    }

    EquationPropertyItem &item = equation_property_item_map_[equation];
    property_browser_->removeProperty(item.name_property);

    for (auto *group_prop : item.GetGroupProperties())
    {
        if (group_prop)
        {
            ClearPropertySubItems(group_prop);
        }
    }
    if (item.name_property)
    {
        ClearPropertySubItems(item.name_property);
        delete item.name_property;
    }
    item.ClearDependencyLists();

    equation_property_item_map_.remove(equation);
    property_equation_map_.remove(item.name_property);
}

void EquationBrowserWidget::OnEquationUpdated(
    const Equation *equation, bitmask::bitmask<EquationUpdateFlag> change_type
)
{
    if (!equation_property_item_map_.contains(equation))
    {
        return;
    }

    EquationPropertyItem &item = equation_property_item_map_[equation];

    if (change_type & EquationUpdateFlag::kContent)
    {
        QString displayText = truncateText(QString::fromStdString(equation->content()));
        item.content_property->setValue(displayText);
    }
    if (change_type & EquationUpdateFlag::kType)
    {
        item.type_property->setValue(QString::fromStdString(ItemTypeConverter::ToString(equation->type())));
    }
    if (change_type & EquationUpdateFlag::kStatus)
    {
        item.status_property->setValue(QString::fromStdString(ResultStatusConverter::ToString(equation->status())));
    }
    if (change_type & EquationUpdateFlag::kMessage)
    {
        item.message_property->setValue(QString::fromStdString(equation->message()));
    }
    if (change_type & EquationUpdateFlag::kDependencies)
    {
        UpdateDependencies(item, equation);
    }
    if (change_type & EquationUpdateFlag::kDependents)
    {
        UpdateDependents(item, equation);
    }
}

void EquationBrowserWidget::OnBrowserItemChanged(QtBrowserItem *item)
{
    if(item == nullptr)
    {
        return;
    }
    while (item->parent() != nullptr)
    {
        item = item->parent();
    }
    QtProperty *top_level_property = item->property();
    if (top_level_property && property_equation_map_.contains(top_level_property))
    {
        emit EquationSelected(property_equation_map_[top_level_property]);
    }
}

void EquationBrowserWidget::SetCurrentEquation(const Equation *equation, bool expand)
{
    if (!equation_property_item_map_.contains(equation))
    {
        return;
    }

    EquationPropertyItem &item = equation_property_item_map_[equation];
    auto browser_item = property_browser_->topLevelItem(item.name_property);
    property_browser_->setExpanded(browser_item, expand);
    property_browser_->setCurrentItem(browser_item);
}

void EquationBrowserWidget::UpdateDependencies(EquationPropertyItem &item, const Equation *equation)
{
    if (!item.dependencies_group_property)
        return;

    ClearPropertySubItems(item.dependencies_group_property);
    item.dependencies_properties.clear();

    UpdateEquationList(
        item.dependencies_group_property, item.dependencies_properties, equation->GetDependencies(), equation->manager()
    );
}

void EquationBrowserWidget::UpdateDependents(EquationPropertyItem &item, const Equation *equation)
{
    if (!item.dependents_group_property)
        return;

    ClearPropertySubItems(item.dependents_group_property);
    item.dependents_properties.clear();

    UpdateEquationList(
        item.dependents_group_property, item.dependents_properties, equation->GetDependents(), equation->manager()
    );
}

void EquationBrowserWidget::UpdateEquationList(
    QtVariantProperty *group_property, QList<QtVariantProperty *> &properties_list,
    const tsl::ordered_set<std::string> &equation_names, const EquationManager *manager
)
{
    if (!group_property)
        return;

    std::string label = "[";

    for (const auto &name : equation_names)
    {
        if (label.size() > 1)
        {
            label += ", ";
        }

        QtVariantProperty *name_property = static_cast<QtVariantProperty *>(
            property_manager_->addProperty(QVariant::String, QString::fromStdString(name))
        );

        if (manager->IsEquationExist(name))
        {
            std::string content = manager->GetEquation(name)->content();
            QString displayText = truncateText(QString::fromStdString(content));
            name_property->setValue(displayText);
        }

        group_property->addSubProperty(name_property);
        properties_list.append(name_property);
        label += name;
    }

    label += "]";
    group_property->setValue(QString::fromStdString(label));
}

EquationBrowserWidget::EquationPropertyItem EquationBrowserWidget::CreatePropertyItem(const Equation *equation)
{
    EquationPropertyItem item;

    item.name_property = static_cast<QtVariantProperty *>(property_manager_->addProperty(
        QtVariantPropertyManager::groupTypeId(), QString::fromStdString(equation->name())
    ));

    std::string content = equation->content();
    QString displayText = truncateText(QString::fromStdString(content));

    item.content_property = CreateBasicProperty("Content", displayText.toStdString());
    item.type_property = CreateBasicProperty("Type", ItemTypeConverter::ToString(equation->type()));
    item.status_property = CreateBasicProperty("Status", ResultStatusConverter::ToString(equation->status()));
    item.message_property = CreateBasicProperty("Message", equation->message());

    item.dependencies_group_property =
        static_cast<QtVariantProperty *>(property_manager_->addProperty(QVariant::String, "Dependencies"));
    UpdateEquationList(
        item.dependencies_group_property, item.dependencies_properties, equation->GetDependencies(), equation->manager()
    );

    item.dependents_group_property =
        static_cast<QtVariantProperty *>(property_manager_->addProperty(QVariant::String, "Dependents"));
    UpdateEquationList(
        item.dependents_group_property, item.dependents_properties, equation->GetDependents(), equation->manager()
    );

    AddSubProperties(item);

    return item;
}

QtVariantProperty *EquationBrowserWidget::CreateBasicProperty(const QString &name, const std::string &value)
{
    QtVariantProperty *property =
        static_cast<QtVariantProperty *>(property_manager_->addProperty(QVariant::String, name));
    property->setValue(QString::fromStdString(value));
    return property;
}

void EquationBrowserWidget::AddSubProperties(EquationPropertyItem &item)
{
    if (!item.name_property)
        return;

    for (auto *prop : item.GetMainProperties())
    {
        if (prop)
            item.name_property->addSubProperty(prop);
    }

    for (auto *group_prop : item.GetGroupProperties())
    {
        if (group_prop)
            item.name_property->addSubProperty(group_prop);
    }
}

void EquationBrowserWidget::ClearPropertySubItems(QtVariantProperty *property)
{
    if (!property)
        return;

    QList<QtProperty *> sub_properties = property->subProperties();
    for (QtProperty *sub_property : sub_properties)
    {
        property->removeSubProperty(sub_property);
        delete sub_property;
    }
}

void EquationBrowserWidget::SetupUI()
{
    setWindowTitle("Equation Browser");
    setWindowFlags(
        Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint
    );

    property_browser_ = new QtTreePropertyBrowser(this);
    property_manager_ = new QtVariantPropertyManager(this);

    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->addWidget(property_browser_);
    setLayout(main_layout);

    property_browser_->setHeaderVisible(false);

    setMinimumSize(500, 800);
}

void EquationBrowserWidget::SetupConnections()
{
    connect(
        property_browser_, &QtTreePropertyBrowser::currentItemChanged, this,
        &EquationBrowserWidget::OnBrowserItemChanged
    );
}

} // namespace gui
} // namespace xequation