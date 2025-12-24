#pragma once

#include <QAbstractListModel>
#include <QHighlightRule>

#include <core/equation.h>
#include <qchar.h>

namespace xequation
{
namespace gui
{
class EquationLanguageModel : public QAbstractListModel
{
  public:
    static constexpr int kWordRole = Qt::UserRole + 1;
    static constexpr int kCategoryRole = Qt::UserRole + 2;
    explicit EquationLanguageModel(const QString &language_name, QObject *parent = nullptr);
    ~EquationLanguageModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void OnEquationAdded(const Equation *equation);
    void OnEquationRemoving(const Equation *equation);

    const QString &language_name() const
    {
        return language_name_;
    }

  private:
    static QMap<QString, QString> language_define_file_map_;
    struct LanguageItem
    {
        QString word;
        QString category;
        QString complete_content;
    };
    QVector<LanguageItem> language_items_;
    QString language_name_;
};
} // namespace gui
} // namespace xequation