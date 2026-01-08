#pragma once
#include "python_qt_wrapper.h"
#include "value_model/value_item_builder.h"

namespace xequation
{
namespace gui
{
class PythonDefaultItemBuilder : public ValueItemBuilder
{
  public:
    PythonDefaultItemBuilder();
    ~PythonDefaultItemBuilder() override;

    virtual bool CanBuild(const Value &value) override;

    virtual ValueItem::UniquePtr CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr) override;

    virtual void LoadChildren(ValueItem *item, int begin, int end) override {}
  protected:
    static QString GetTypeName(pybind11::handle obj, bool qualified = false);
    static QString GetObjectRepr(pybind11::handle obj);
};

class PythonListItemBuilder : public PythonDefaultItemBuilder
{
  public:
    virtual bool CanBuild(const Value &value) override;
    virtual ValueItem::UniquePtr CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr) override;
    virtual void LoadChildren(ValueItem *item, int begin, int end) override;
};

class PythonTupleItemBuilder : public PythonDefaultItemBuilder
{
  public:
    virtual bool CanBuild(const Value &value) override;
    virtual ValueItem::UniquePtr CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr) override;
    virtual void LoadChildren(ValueItem *item, int begin, int end) override;
};

class PythonSetItemBuilder : public PythonDefaultItemBuilder
{
  public:
    virtual bool CanBuild(const Value &value) override;
    virtual ValueItem::UniquePtr CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr) override;
    virtual void LoadChildren(ValueItem *item, int begin, int end) override;
};

class PythonDictItemBuilder : public PythonDefaultItemBuilder
{
  public:
    virtual bool CanBuild(const Value &value) override;
    virtual ValueItem::UniquePtr CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr) override;
    virtual void LoadChildren(ValueItem *item, int begin, int end) override;
};

class PythonClassItemBuilder : public PythonDefaultItemBuilder
{
  public:
    virtual bool CanBuild(const Value &value) override;
    virtual ValueItem::UniquePtr CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr) override;
    virtual void LoadChildren(ValueItem *item, int begin, int end) override;
};

} // namespace gui
} // namespace xequation