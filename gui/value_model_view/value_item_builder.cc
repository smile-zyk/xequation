#include "value_item_builder.h"
#include "value_item.h"

namespace xequation
{
namespace gui
{
bool ValueItemBuilder::CanBuild(const Value &value)
{
    return false;
}

ValueItem::UniquePtr ValueItemBuilder::CreateValueItem(const QString &name, const Value &value, ValueItem *parent)
{
    return ValueItem::Create(name, value, parent);
}

void ValueItemBuilder::SetTypeString(ValueItem *item, const QString &type_string)
{
    if (item)
    {
        item->type_string_ = type_string;
    }
}

void ValueItemBuilder::SetDisplayValueString(ValueItem *item, const QString &display_value_string)
{
    if (item)
    {
        item->display_value_string_ = display_value_string;
    }
}

void ValueItemBuilder::SetHasChildren(ValueItem *item, bool has_children)
{
    if (item)
    {
        item->has_children_ = has_children;
    }
}

void ValueItemBuilder::SetIsLoaded(ValueItem *item, bool is_loaded)
{
    if (item)
    {
        item->is_loaded_ = is_loaded;
    }
}

// ValueItemBuilderRegistry implementation
ValueItemBuilderRegistry &ValueItemBuilderRegistry::GetInstance()
{
    static ValueItemBuilderRegistry instance;
    return instance;
}

void ValueItemBuilderRegistry::RegisterBuilder(std::unique_ptr<ValueItemBuilder> builder, int priority)
{
    if (!builder)
    {
        return;
    }
    
    BuilderEntry entry{std::move(builder), priority};
    
    // Insert in sorted order by priority (highest priority first)
    auto it = std::lower_bound(builders_.begin(), builders_.end(), entry,
                              [](const BuilderEntry &a, const BuilderEntry &b) {
                                  return a.priority > b.priority; // Descending order
                              });
    builders_.insert(it, std::move(entry));
}

void ValueItemBuilderRegistry::UnRegisterBuilder(ValueItemBuilder *builder)
{
    if (!builder)
    {
        return;
    }
    
    builders_.erase(std::remove_if(builders_.begin(), builders_.end(),
                                  [builder](const BuilderEntry &entry) {
                                      return entry.builder.get() == builder;
                                  }),
                   builders_.end());
}

ValueItemBuilder *ValueItemBuilderRegistry::FindBuilder(const Value &value) const
{
    for (const auto &entry : builders_)
    {
        if (entry.builder->CanBuild(value))
        {
            return entry.builder.get();
        }
    }
    return nullptr;
}

ValueItem::UniquePtr ValueItemBuilderRegistry::CreateValueItem(const QString &name, const Value &value, ValueItem *parent) const
{
    auto builder = FindBuilder(value);
    if (builder)
    {
        return builder->CreateValueItem(name, value, parent);
    }
    return nullptr;
}

void ValueItemBuilderRegistry::LoadChildren(ValueItem *item) const
{
    if (!item)
    {
        return;
    }
    
    auto builder = FindBuilder(item->value());
    if (builder)
    {
        builder->LoadChildren(item);
    }
}

void ValueItemBuilderRegistry::Clear()
{
    builders_.clear();
}

}
}