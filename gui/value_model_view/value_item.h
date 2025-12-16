#pragma once

#include <memory>
#include <QString>
#include <QVector>
#include <tsl/ordered_map.h>
#include <core/value.h>

namespace xequation
{
namespace gui
{
class ValueItem
{
public:
    using UniquePtr = std::unique_ptr<ValueItem>;
    QString GetType() const;
    QString GetDisplayValue() const;
    bool HasChildren() const;
    void LoadChildren();
    void UnLoadChildren();
    void AddChild(ValueItem::UniquePtr child);
    void RemoveChild(ValueItem* child);
    ValueItem* GetChildAt(int index);
    int GetIndexOfChild(ValueItem* child) const;
    size_t ChildCount() const;
    const QString& name() const { return name_; }
    const Value& value() const { return value_; }
    const QString& type_string() const { return type_string_; }
    const QString& display_value_string() const { return display_value_string_; }
    ValueItem* parent() const { return parent_; }
    bool is_loaded() const { return is_loaded_; }
    bool has_children() const { return has_children_; }

protected:
    friend class ValueItemBuilder;
    static UniquePtr Create(const QString &name, const Value &value, ValueItem* parent = nullptr);
    ValueItem(const QString &name, const Value &value, ValueItem* parent = nullptr);
    ~ValueItem();
private:
    QString name_;
    Value value_;
    QString type_string_;
    QString display_value_string_;
    ValueItem* parent_ = nullptr;
    QVector<std::unique_ptr<ValueItem>> children_;
    bool is_loaded_ = false;
    bool has_children_ = false;
};
}
}