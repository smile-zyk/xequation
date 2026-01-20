#pragma once

#include <QAbstractListModel>
#include <QMap>
#include <QSet>
#include "core/equation_common.h"

namespace xequation
{
namespace gui
{
struct CompletionItem
{
    QString word;
    QString type;
    QString category;
    QString complete_content;

    bool operator<(const CompletionItem &other) const
    {
        if (type != other.type)
        {
            return type < other.type;
        }
        return word < other.word;
    }
};

class CompletionModel : public QAbstractListModel
{
  public:
    static constexpr int kWordRole = Qt::UserRole + 1;
    static constexpr int kTypeRole = Qt::UserRole + 2;
    static constexpr int kCategoryRole = Qt::UserRole + 3;
    static constexpr int kCompleteContentRole = Qt::UserRole + 4;

    explicit CompletionModel(QObject *parent = nullptr);
    ~CompletionModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QList<QString> GetAllCategories() const;

    void AddCompletionItem(const QString &word, const QString &type, const QString &category = QString(), const QString &complete_content = QString());

    void RemoveCompletionItem(const QString &word, const QString &type);

    void Clear();

  private:
    void ResortItems();

    QList<CompletionItem> items_;

    QMap<QPair<QString, QString>, int> item_index_map_;
};
} // namespace gui
} // namespace xequation