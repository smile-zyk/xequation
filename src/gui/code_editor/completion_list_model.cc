#include "completion_list_model.h"
#include <QFile>
#include <QLanguage>
#include <algorithm>

namespace xequation
{
namespace gui
{

static QMap<QString, QString> kLanguageDefineFileMap = {
    {"Python", ":/code_editor/languages/python.xml"},
};

static QList<CompletionCategory> kPredefinedCategories = {
    {"Variable", 0},
    {"Function", 1},
    {"Class", 2},
    {"Module", 3},
    {"Builtin", 4},
    {"Keyword", 5},
};

const QList<CompletionCategory>& CompletionListModel::GetPredefinedCategories()
{
    return kPredefinedCategories;
}

CompletionListModel::CompletionListModel(const QString &language_name, QObject *parent)
    : QAbstractListModel(parent), language_name_(language_name)
{
    InitWithLanguageDefinition();
}

CompletionListModel::~CompletionListModel() {}

void CompletionListModel::InitWithLanguageDefinition()
{
    auto it = kLanguageDefineFileMap.find(language_name_);
    if (it == kLanguageDefineFileMap.end())
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
    for (auto &&key : keys)
    {
        auto names = language.names(key);
        for (auto &&name : names)
        {
            AddCompletionItem(name, key, name);
        }
    }
}

int CompletionListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return items_.size();
}

QVariant CompletionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size())
    {
        return QVariant();
    }

    const CompletionItem &item = items_.at(index.row());
    
    // DisplayRole: "word    category"
    if (role == Qt::DisplayRole)
    {
        return item.word + "    " + item.category.name;
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
    else if (role == kCategoryRole)
    {
        return item.category.name;
    }
    else if (role == kCompleteContentRole)
    {
        return item.complete_content;
    }
    else if (role == kPriorityRole)
    {
        return item.category.priority;
    }
    else if (role == kTypeRole)
    {
        return static_cast<int>(item.type);
    }

    return QVariant();
}

QList<CompletionCategory> CompletionListModel::GetAllCategories() const
{
    QSet<QString> category_names;
    QList<CompletionCategory> categories;
    
    for (const auto& item : items_)
    {
        if (!category_names.contains(item.category.name))
        {
            category_names.insert(item.category.name);
            categories.append(item.category);
        }
    }
    
    std::sort(categories.begin(), categories.end());
    
    return categories;
}

CompletionCategory CompletionListModel::FindCategoryByName(const QString& name) const
{
    for (const auto& cat : kPredefinedCategories)
    {
        if (cat.name == name)
        {
            return cat;
        }
    }
    
    return CompletionCategory{name, 999};
}

void CompletionListModel::AddCompletionItem(const QString& word, const QString& category_name, const QString& complete_content, CompletionItemType type)
{
    auto key = qMakePair(word, category_name);
    
    if (item_index_map_.contains(key))
    {
        int idx = item_index_map_[key];
        if (idx >= 0 && idx < items_.size())
        {
            items_[idx].complete_content = complete_content;
            items_[idx].type = type;
            QModelIndex modelIdx = index(idx);
            emit dataChanged(modelIdx, modelIdx);
            
            // 类型变化可能影响排序，需要重新排序
            ResortItems();
        }
        return;
    }
    
    CompletionItem item;
    item.word = word;
    item.category = FindCategoryByName(category_name);
    item.complete_content = complete_content;
    item.type = type;
    if(category_name == "Function" || category_name == "Class")
    {
        item.complete_content += "()";
    }
    
    int insert_pos = 0;
    for (int i = 0; i < items_.size(); ++i)
    {
        if (item < items_[i])
        {
            insert_pos = i;
            break;
        }
        insert_pos = i + 1;
    }
    
    beginInsertRows(QModelIndex(), insert_pos, insert_pos);
    items_.insert(insert_pos, item);
    endInsertRows();
    
    item_index_map_.clear();
    for (int i = 0; i < items_.size(); ++i)
    {
        auto k = qMakePair(items_[i].word, items_[i].category.name);
        item_index_map_[k] = i;
    }
}

void CompletionListModel::AddCompletionItem(const QString& word, const QString& category_name, const QString& complete_content)
{
    AddCompletionItem(word, category_name, complete_content, CompletionItemType::Local);
}

void CompletionListModel::RemoveCompletionItem(const QString& word, const QString& category_name)
{
    auto key = qMakePair(word, category_name);
    
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
        auto k = qMakePair(items_[i].word, items_[i].category.name);
        item_index_map_[k] = i;
    }
}

void CompletionListModel::RemoveCompletionItemsByType(CompletionItemType type)
{
    // 从后向前遍历删除，避免索引变化问题
    for (int i = items_.size() - 1; i >= 0; --i)
    {
        if (items_[i].type == type)
        {
            beginRemoveRows(QModelIndex(), i, i);
            items_.removeAt(i);
            endRemoveRows();
        }
    }
    
    // 重建索引
    item_index_map_.clear();
    for (int i = 0; i < items_.size(); ++i)
    {
        auto k = qMakePair(items_[i].word, items_[i].category.name);
        item_index_map_[k] = i;
    }
}

void CompletionListModel::Clear()
{
    beginResetModel();
    items_.clear();
    item_index_map_.clear();
    endResetModel();
}

void CompletionListModel::ResortItems()
{
    beginResetModel();
    std::sort(items_.begin(), items_.end());
    
    item_index_map_.clear();
    for (int i = 0; i < items_.size(); ++i)
    {
        auto k = qMakePair(items_[i].word, items_[i].category.name);
        item_index_map_[k] = i;
    }
    endResetModel();
}
} // namespace gui
} // namespace xequation