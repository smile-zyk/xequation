#include "value_item.h"

namespace xequation
{
namespace gui
{
ValueItem::ValueItem(const QString &name, const Value &value, ValueItem *parent)
    : name_(name), value_(value), parent_(parent)
{
}
} // namespace gui
} // namespace xequation