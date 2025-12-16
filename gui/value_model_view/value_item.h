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
    static UniquePtr Create(const QString &name, const Value &value, ValueItem* parent = nullptr);
    static UniquePtr Create(const QString &name, const QString& display_value, const QString& type, ValueItem* parent = nullptr);
    bool HasChildren() const;
    void LoadChildren();
    void UnLoadChildren();
    void AddChild(ValueItem::UniquePtr child);
    void RemoveChild(ValueItem* child);
    ValueItem* GetChildAt(int index);
    int GetIndexOfChild(ValueItem* child) const;
    size_t ChildCount() const;

    void set_type(const QString &type) { type_ = type; }
    void set_display_value(const QString &display_value) { display_value_ = display_value; }
    void set_about_to_load_child_count(size_t count) { about_to_load_child_count_ = count; }
    
    const QString& name() const { return name_; }
    const Value& value() const { return value_; }
    const QString& type() const { return type_; }
    const QString& display_value() const { return display_value_; }
    ValueItem* parent() const { return parent_; }
    bool is_loaded() const { return is_loaded_; }
    bool has_children() const { return about_to_load_child_count_ > 0; }

protected:
    ValueItem(const QString &name, const Value &value, ValueItem* parent = nullptr);
    ValueItem(const QString &name, const QString& display_value, const QString& type, ValueItem* parent = nullptr);
    ~ValueItem();
private:
    QString name_;
    Value value_;
    QString type_;
    QString display_value_;
    ValueItem* parent_ = nullptr;
    QVector<std::unique_ptr<ValueItem>> children_;
    bool is_loaded_ = false;
    size_t about_to_load_child_count_ = 0;
};
}
}