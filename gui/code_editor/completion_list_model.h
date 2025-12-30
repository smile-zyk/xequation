#pragma once

#include <QAbstractListModel>
#include <QMap>
#include <QSet>
#include <qchar.h>


namespace xequation
{
namespace gui
{

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

    bool operator<(const CompletionItem &other) const
    {
        if (category.priority != other.category.priority)
        {
            return category.priority < other.category.priority;
        }
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

    static const QList<CompletionCategory> &GetPredefinedCategories();

    explicit CompletionListModel(const QString &language_name, QObject *parent = nullptr);
    ~CompletionListModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QList<CompletionCategory> GetAllCategories() const;

    void AddCompletionItem(const QString &word, const QString &category_name, const QString &complete_content);

    void RemoveCompletionItem(const QString &word, const QString &category_name);

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