#pragma once

#include <QAbstractListModel>
#include <QMap>
#include <QSet>

namespace xequation
{
namespace gui
{

enum class CompletionItemType
{
    Tmp = 0,      // 最高优先级
    Local = 1,    // 中等优先级
    Builtin = 2   // 最低优先级
};

inline uint qHash(CompletionItemType type, uint seed = 0) noexcept
{
    return ::qHash(static_cast<int>(type), seed);
}

struct CompletionCategory
{
    QString name;
    int priority;

    bool operator==(const CompletionCategory &other) const
    {
        return name == other.name;
    }

    bool operator<(const CompletionCategory &other) const
    {
        return priority < other.priority;
    }
};

struct CompletionItem
{
    QString word;
    CompletionCategory category;
    QString complete_content;
    CompletionItemType type;

    bool operator<(const CompletionItem &other) const
    {
        // 先按 type 排序 (tmp, local, builtin)
        if (type != other.type)
        {
            return type < other.type;
        }
        // 再按 category priority 排序
        if (category.priority != other.category.priority)
        {
            return category.priority < other.category.priority;
        }
        // 最后按 word 排序
        return word < other.word;
    }
};

class CompletionListModel : public QAbstractListModel
{
  public:
    static constexpr int kWordRole = Qt::UserRole + 1;
    static constexpr int kCategoryRole = Qt::UserRole + 2;
    static constexpr int kCompleteContentRole = Qt::UserRole + 3;
    static constexpr int kPriorityRole = Qt::UserRole + 4;
    static constexpr int kTypeRole = Qt::UserRole + 5;

    static const QList<CompletionCategory> &GetPredefinedCategories();

    explicit CompletionListModel(const QString &language_name, QObject *parent = nullptr);
    ~CompletionListModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QList<CompletionCategory> GetAllCategories() const;

    void AddCompletionItem(const QString &word, const QString &category_name, const QString &complete_content, CompletionItemType type);
    
    // 保留原接口，默认加到 Local
    void AddCompletionItem(const QString &word, const QString &category_name, const QString &complete_content);

    void RemoveCompletionItem(const QString &word, const QString &category_name);

    void RemoveCompletionItemsByType(CompletionItemType type);

    void Clear();

    const QString &language_name() const
    {
        return language_name_;
    }

  private:
    CompletionCategory FindCategoryByName(const QString &name) const;

    void ResortItems();

    void InitWithLanguageDefinition();

    QList<CompletionItem> items_;

    QString language_name_;

    QMap<QPair<QString, QString>, int> item_index_map_;
};
} // namespace gui
} // namespace xequation