#include "value_item_builder.h"
#include "value_item.h"

namespace xequation
{
namespace gui
{
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

    auto it = std::lower_bound(builders_.begin(), builders_.end(), entry);
    builders_.insert(it, std::move(entry));
}

void ValueItemBuilderRegistry::UnRegisterBuilder(ValueItemBuilder *builder)
{
    if (!builder)
    {
        return;
    }

    builders_.erase(
        std::remove_if(
            builders_.begin(), builders_.end(),
            [builder](const BuilderEntry &entry) { return entry.builder.get() == builder; }
        ),
        builders_.end()
    );
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

ValueItem::UniquePtr
ValueItemBuilderRegistry::CreateValueItem(const QString &name, const Value &value, ValueItem *parent) const
{
    auto builder = FindBuilder(value);
    if (builder)
    {
        return builder->CreateValueItem(name, value, parent);
    }
    return nullptr;
}

void ValueItemBuilderRegistry::LoadChildren(ValueItem *item, int begin, int end) const
{
    if (!item)
    {
        return;
    }

    auto builder = FindBuilder(item->value());
    if (builder)
    {
        builder->LoadChildren(item, begin, end);
    }
}

void ValueItemBuilderRegistry::Clear()
{
    builders_.clear();
}

} // namespace gui
} // namespace xequation