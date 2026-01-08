#include "value_item.h"
#include "value_item_builder.h"

namespace xequation
{
namespace gui
{

ValueItem::ValueItem(const QString &name, const Value &value, ValueItem *parent)
    : name_(name), value_(value), parent_(parent), id_(QUuid::createUuid())
{
}

ValueItem::ValueItem(const QString &name, const QString &display_value, const QString &type, ValueItem *parent)
    : name_(name), value_(), type_(type), display_value_(display_value), parent_(parent), id_(QUuid::createUuid())
{
}

ValueItem::UniquePtr ValueItem::Create(const QString &name, const Value &value, ValueItem *parent)
{
    return std::unique_ptr<ValueItem>(new ValueItem(name, value, parent));
}

ValueItem::UniquePtr ValueItem::Create(const QString &name, const QString &display_value, const QString &type, ValueItem *parent)
{
    return std::unique_ptr<ValueItem>(new ValueItem(name, display_value, type, parent));
}

void ValueItem::LoadChildren(int begin, int end)
{
    if (IsLoaded())
        return;

    BuilderUtils::LoadChildren(this, begin, end);
}

void ValueItem::UnLoadChildren()
{
    children_.clear();
}

void ValueItem::AddChild(ValueItem::UniquePtr child)
{
    if (!child)
        return;

    child->parent_ = this;
    children_.push_back(std::move(child));
}

void ValueItem::RemoveChild(ValueItem *child)
{
    if (!child)
        return;

    for (int i = 0; i < children_.size(); ++i)
    {
        if (children_[i].get() == child)
        {
            children_.erase(children_.begin() + i);
            break;
        }
    }
}

ValueItem *ValueItem::GetChildAt(int index) const
{
    if (index < 0 || index >= children_.size())
        return nullptr;
    return children_[index].get();
}

int ValueItem::GetIndexOfChild(ValueItem *child) const
{
    if (!child)
        return -1;

    for (int i = 0; i < children_.size(); ++i)
    {
        if (children_[i].get() == child)
            return i;
    }
    return -1;
}
} // namespace gui
} // namespace xequation