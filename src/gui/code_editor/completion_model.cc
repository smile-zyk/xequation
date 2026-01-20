#include "completion_model.h"
#include <algorithm>

namespace xequation
{
namespace gui
{

CompletionModel::CompletionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

CompletionModel::~CompletionModel() {}

int CompletionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return items_.size();
}

QVariant CompletionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size())
    {
        return QVariant();
    }

    const CompletionItem &item = items_.at(index.row());
    
    // DisplayRole: "word    category"
    if (role == Qt::DisplayRole)
    {
        return item.word + "    " + item.type;
    }
    // EditRole: complete_content
    else if (role == Qt::EditRole)
    {
        return item.complete_content;
    }
    else if (role == kWordRole)
    {
        return item.word;
    }
    else if (role == kTypeRole)
    {
        return item.type;
    }
    else if (role == kCategoryRole)
    {
        return item.category;
    }
    else if (role == kCompleteContentRole)
    {
        return item.complete_content;
    }

    return QVariant();
}

QList<QString> CompletionModel::GetAllCategories() const
{
    QSet<QString> category_names;
    QList<QString> categories;
    
    for (const auto& item : items_)
    {
        if (!category_names.contains(item.category))
        {
            category_names.insert(item.category);
            categories.append(item.category);
        }
    }
    
    std::sort(categories.begin(), categories.end());
    
    return categories;
}

void CompletionModel::AddCompletionItem(const QString& word, const QString& type, const QString& category, const QString& complete_content)
{
    auto key = qMakePair(word, type);
    
    if (item_index_map_.contains(key))
    {
        RemoveCompletionItem(word, type);
    }
    
    CompletionItem item;
    item.word = word;
    item.type = type;

    if(category.isEmpty())
    {
        item.category = type;
    }
    else
    {
        item.category = category;
    }

    if(complete_content.isEmpty())
    {
        item.complete_content = word;
    }
    else
    {
        item.complete_content = complete_content;
    }
    
    items_.append(item);
    
    ResortItems();
}

void CompletionModel::RemoveCompletionItem(const QString& word, const QString& type)
{
    auto key = qMakePair(word, type);
    
    if (!item_index_map_.contains(key))
    {
        return;
    }
    
    int idx = item_index_map_[key];
    
    if (idx < 0 || idx >= items_.size())
    {
        return;
    }
    
    beginRemoveRows(QModelIndex(), idx, idx);
    items_.removeAt(idx);
    endRemoveRows();
    
    item_index_map_.clear();
    for (int i = 0; i < items_.size(); ++i)
    {
        auto k = qMakePair(items_[i].word, items_[i].type);
        item_index_map_[k] = i;
    }
}

void CompletionModel::Clear()
{
    beginResetModel();
    items_.clear();
    item_index_map_.clear();
    endResetModel();
}

void CompletionModel::ResortItems()
{
    beginResetModel();
    std::sort(items_.begin(), items_.end());
    
    item_index_map_.clear();
    for (int i = 0; i < items_.size(); ++i)
    {
        auto k = qMakePair(items_[i].word, items_[i].type);
        item_index_map_[k] = i;
    }
    endResetModel();
}
} // namespace gui
} // namespace xequation