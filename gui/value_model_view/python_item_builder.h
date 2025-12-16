#include "python/python_qt_wrapper.h"
#include "value_item_builder.h"


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

    virtual void LoadChildren(ValueItem *item) override {}

  protected:
    static QString GetTypeName(pybind11::handle obj, bool qualified = false);
    static QString GetObjectStr(pybind11::handle obj);
};

class PythonListItemBuilder : public PythonDefaultItemBuilder
{
  public:
    virtual bool CanBuild(const Value &value) override;
    virtual ValueItem::UniquePtr CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr) override;
    virtual void LoadChildren(ValueItem *item) override;
};

class PythonTupleItemBuilder : public PythonDefaultItemBuilder
{
  public:
    virtual bool CanBuild(const Value &value) override;
    virtual ValueItem::UniquePtr CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr) override;
    virtual void LoadChildren(ValueItem *item) override;
};

class PythonSetItemBuilder : public PythonDefaultItemBuilder
{
  public:
    virtual bool CanBuild(const Value &value) override;
    virtual ValueItem::UniquePtr CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr) override;
    virtual void LoadChildren(ValueItem *item) override;
};

class PythonDictItemBuilder : public PythonDefaultItemBuilder
{
  public:
    virtual bool CanBuild(const Value &value) override;
    virtual ValueItem::UniquePtr CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr) override;
    virtual void LoadChildren(ValueItem *item) override;
};
} // namespace gui
} // namespace xequation