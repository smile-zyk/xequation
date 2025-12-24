#include "language_model.h"
#include "core/equation_common.h"
#include <QFile>
#include <QLanguage>
#include <qglobal.h>
#include <qnamespace.h>

namespace xequation {
namespace gui {

QMap<QString, QString> LanguageModel::language_define_file_map_ = {
    {"Python", ":/languages/python.xml"},
};

LanguageModel::LanguageModel(const QString& language_name, QObject* parent)
    : QAbstractListModel(parent), language_name_(language_name)
{
    auto it = language_define_file_map_.find(language_name);
    if (it == language_define_file_map_.end())
    {
        return;
    }

    QString define_file = it.value();
    QFile fl(define_file);

    if (!fl.open(QIODevice::ReadOnly))
    {
        return;
    }

    QLanguage language(&fl);

    if (!language.isLoaded())
    {
        return;
    }

    auto keys = language.keys();
    for (auto&& key : keys)
    {
        auto names = language.names(key);
        for (auto&& name : names)
        {
            LanguageItem item;
            item.word = name;
            item.category = key;
            item.complete_content = name;
            language_items_.append(item);
        }
    }
}

LanguageModel::~LanguageModel()
{
}

int LanguageModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return language_items_.size();
}

QVariant LanguageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= language_items_.size())
    {
        return QVariant();
    }

    const LanguageItem& item = language_items_.at(index.row());
    // display "{word}\t{category}"
    if (role == Qt::DisplayRole)
    {
        return item.word + "\t" + item.category;
    }
    else if (role == Qt::EditRole)
    {
        return item.complete_content;
    }
    else if (role == kWordRole)
    {
        return item.word;
    }
    else if (role == kCategoryRole)
    {
        return item.category;
    }

    return QVariant();
}

void LanguageModel::OnEquationAdded(const Equation* equation)
{
    QString name = QString::fromStdString(equation->name());
    QString type = QString::fromStdString(ItemTypeConverter::ToString(equation->type()));
    if(type == "Import" || type == "ImportFrom")
    {
        type = "Module";
    }

    LanguageItem item;
    item.word = name;
    item.category = type;
    item.complete_content = name;
    beginInsertRows(QModelIndex(), language_items_.size(), language_items_.size());
    language_items_.append(item);
    endInsertRows();
}

void LanguageModel::OnEquationRemoving(const Equation* equation)
{
    QString name = QString::fromStdString(equation->name());
    for (int i = 0; i < language_items_.size(); ++i)
    {
        if (language_items_[i].word == name)
        {
            beginRemoveRows(QModelIndex(), i, i);
            language_items_.removeAt(i);
            endRemoveRows();
            break;
        }
    }
}
}
}