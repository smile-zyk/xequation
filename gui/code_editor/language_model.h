#pragma once

#include <QAbstractListModel>
#include <QHighlightRule>
#include <QSet>

#include <core/equation.h>

namespace xequation
{
namespace gui
{
class LanguageModel : public QAbstractListModel
{
  public:
    static constexpr int kWordRole = Qt::UserRole + 1;
    static constexpr int kCategoryRole = Qt::UserRole + 2;
    explicit LanguageModel(const QString &language_name, QObject *parent = nullptr);
    ~LanguageModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    const QString &language_name() const
    {
        return language_name_;
    }

    void AddWordItem(const QString &word, const QString &category, const QString &complete_content);
    void RemoveWordItem(const QString &word);

  private:
    static QMap<QString, QString> language_define_file_map_;
    struct WordItem
    {
        QString word;
        QString category;
        QString complete_content;
    };
    QSet<QString> language_item_set_;
    QSet<QString> word_item_set_;
    QVector<WordItem> word_items_;
    QString language_name_;
};
} // namespace gui
} // namespace xequation