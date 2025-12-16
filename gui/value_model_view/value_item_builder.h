#include "value_item.h"

namespace xequation
{
namespace gui
{
class ValueItemBuilder
{
  public:
    virtual ~ValueItemBuilder() = default;
    virtual bool CanBuild(const Value &value);
    virtual ValueItem::UniquePtr CreateValueItem(const QString &name, const Value &value, ValueItem *parent = nullptr);
    virtual void LoadChildren(ValueItem *item){}
  protected:
    void SetTypeString(ValueItem *item, const QString &type_string);
    void SetDisplayValueString(ValueItem *item, const QString &display_value_string);
    void SetHasChildren(ValueItem *item, bool has_children);
    void SetIsLoaded(ValueItem *item, bool is_loaded);
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

    void LoadChildren(ValueItem *item) const;

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

inline void RegisterValueItemBuilder(std::unique_ptr<ValueItemBuilder> builder, int priority = 0)
{
    ValueItemBuilderRegistry::GetInstance().RegisterBuilder(std::move(builder), priority);
}

inline void UnRegisterValueItemBuilder(ValueItemBuilder *builder)
{
    ValueItemBuilderRegistry::GetInstance().UnRegisterBuilder(builder);
}

inline ValueItem::UniquePtr
CreateValueItemByBuilder(const QString &name, const Value &value, ValueItem *parent = nullptr)
{
    return ValueItemBuilderRegistry::GetInstance().CreateValueItem(name, value, parent);
}

inline ValueItemBuilder *FindValueItemBuilder(const Value &value)
{
    return ValueItemBuilderRegistry::GetInstance().FindBuilder(value);
}

template <typename T>
class ValueItemBuilderAutoRegister
{
  public:
    ValueItemBuilderAutoRegister(int priority = 0)
    {
        RegisterValueItemBuilder(std::unique_ptr<T>(new T()), priority);
    }
};

#define REGISTER_VALUE_ITEM_BUILDER_WITH_PRIORITY(BuilderClass, priority)                                                   \
    static xequation::gui::ValueItemBuilderAutoRegister<BuilderClass>                                 \
        s_autoRegister_##BuilderClass(priority)

#define REGISTER_VALUE_ITEM_BUILDER(BuilderClass)                                                   \
    static xequation::gui::ValueItemBuilderAutoRegister<BuilderClass>                                 \
        s_autoRegister_##BuilderClass;

} // namespace gui
} // namespace xequation