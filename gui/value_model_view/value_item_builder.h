#pragma once
#include "value_item.h"

namespace xequation
{
namespace gui
{
class ValueItemBuilder
{
  public:
    virtual ~ValueItemBuilder() = default;
    virtual bool CanBuild(const Value &value) = 0;
    virtual ValueItem::UniquePtr
    CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr) = 0;
    virtual void LoadChildren(ValueItem *item, int begin, int end) = 0;

  protected:
    ValueItemBuilder() = default;
};

class ValueItemBuilderRegistry
{
  public:
    static ValueItemBuilderRegistry &GetInstance();

    void RegisterBuilder(std::unique_ptr<ValueItemBuilder> builder, int priority = 0);

    void UnRegisterBuilder(ValueItemBuilder *builder);

    ValueItemBuilder *FindBuilder(const Value &value) const;

    ValueItem::UniquePtr CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr) const;

    void LoadChildren(ValueItem *item, int begin, int end) const;

    void Clear();

  private:
    ValueItemBuilderRegistry() = default;
    ~ValueItemBuilderRegistry() = default;

    ValueItemBuilderRegistry(const ValueItemBuilderRegistry &) = delete;
    ValueItemBuilderRegistry &operator=(const ValueItemBuilderRegistry &) = delete;
    ValueItemBuilderRegistry(ValueItemBuilderRegistry &&) = delete;
    ValueItemBuilderRegistry &operator=(ValueItemBuilderRegistry &&) = delete;

  private:
    struct BuilderEntry
    {
        std::unique_ptr<ValueItemBuilder> builder;
        int priority;

        bool operator<(const BuilderEntry &other) const
        {
            return priority < other.priority;
        }
    };
    std::vector<BuilderEntry> builders_;
};

namespace BuilderUtils
{
inline void RegisterValueItemBuilder(std::unique_ptr<ValueItemBuilder> builder, int priority = 0)
{
    ValueItemBuilderRegistry::GetInstance().RegisterBuilder(std::move(builder), priority);
}

inline void UnRegisterValueItemBuilder(ValueItemBuilder *builder)
{
    ValueItemBuilderRegistry::GetInstance().UnRegisterBuilder(builder);
}

inline ValueItem::UniquePtr
CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr)
{
    return ValueItemBuilderRegistry::GetInstance().CreateValueItem(name, value, parent);
}

inline void LoadChildren(ValueItem *item, int begin, int end)
{
    return ValueItemBuilderRegistry::GetInstance().LoadChildren(item, begin, end);
}
} // namespace BuilderUtils

template <typename T>
class ValueItemBuilderAutoRegister
{
  public:
    ValueItemBuilderAutoRegister(int priority = 0)
    {
        BuilderUtils::RegisterValueItemBuilder(std::unique_ptr<T>(new T()), priority);
    }
};

#define REGISTER_VALUE_ITEM_BUILDER(BuilderClass, priority)                                                                      \
    static xequation::gui::ValueItemBuilderAutoRegister<BuilderClass> s_autoRegister_##BuilderClass(priority);

} // namespace gui
} // namespace xequation