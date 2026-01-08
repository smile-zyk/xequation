#include "equation_completion_model.h"
#include "core/equation.h"
#include "core/equation_common.h"
#include <QString>
#include <QSet>
#include <algorithm>

namespace xequation
{
namespace gui
{

void EquationCompletionModel::OnEquationAdded(const Equation *equation)
{
	if (!equation)
	{
		return;
	}

	QString word = QString::fromStdString(equation->name());
    QString type = QString::fromStdString(ItemTypeConverter::ToString(equation->type()));
	if(type == "Import" || type == "ImportFrom")
    {
        type = "Module";
    }
    AddCompletionItem(word, type, word);
}

void EquationCompletionModel::OnEquationRemoving(const Equation *equation)
{
	if (!equation)
	{
		return;
	}

	const QString word = QString::fromStdString(equation->name());
    QString type = QString::fromStdString(ItemTypeConverter::ToString(equation->type()));
	if(type == "Import" || type == "ImportFrom")
    {
        type = "Module";
    }
	RemoveCompletionItem(word, type);
}

EquationCompletionFilterModel::EquationCompletionFilterModel(EquationCompletionModel *model, QObject *parent)
	: QSortFilterProxyModel(parent), model_(model)
{
	setSourceModel(model);
}

void EquationCompletionFilterModel::setSourceModel(QAbstractItemModel *sourceModel)
{
	QSortFilterProxyModel::setSourceModel(sourceModel);

	model_ = qobject_cast<EquationCompletionModel *>(sourceModel);
}

void EquationCompletionFilterModel::SetDisplayOnlyWord(bool display_only_word)
{
	display_only_word_ = display_only_word;
	// Refresh displayed data for all rows
	if (rowCount() > 0)
	{
		emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
	}
}

void EquationCompletionFilterModel::SetEquationGroup(const EquationGroup* group)
{
	group_ = group;
	invalidateFilter();
}

void EquationCompletionFilterModel::SetFilterText(const QString &filter_text)
{
	filter_text_ = filter_text;
	invalidateFilter();
}

void EquationCompletionFilterModel::SetCategory(const QString &category)
{
	category_ = category;
	invalidateFilter();
}

QList<CompletionCategory> EquationCompletionFilterModel::GetAllCategories()
{
	QList<CompletionCategory> categories;
	if (!model_)
	{
		return categories;
	}

	QSet<QString> names;
	const int row_count = model_->rowCount();
	for (int i = 0; i < row_count; ++i)
	{
		QModelIndex idx = model_->index(i, 0);
		QString word = model_->data(idx, CompletionListModel::kWordRole).toString();

		// Group filter: skip words existing in the equation group
		if (group_ && group_->IsEquationExist(word.toStdString()))
		{
			continue;
		}

		QString name = model_->data(idx, CompletionListModel::kCategoryRole).toString();
		int priority = model_->data(idx, CompletionListModel::kPriorityRole).toInt();

		if (names.contains(name))
		{
			continue;
		}

		names.insert(name);
		categories.append(CompletionCategory{name, priority});
	}

	std::sort(categories.begin(), categories.end());
	return categories;
}

bool EquationCompletionFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
	auto *src = sourceModel();
	if (!src)
	{
		return true;
	}

	QModelIndex idx = src->index(source_row, 0, source_parent);
	QString word = src->data(idx, CompletionListModel::kWordRole).toString();
	if (word.isEmpty())
	{
		return true;
	}

	// Group filter: hide words already present in the equation group
	if (group_ && group_->IsEquationExist(word.toStdString()))
	{
		return false;
	}

	if (!category_.isEmpty())
	{
		QString category = src->data(idx, CompletionListModel::kCategoryRole).toString();
		if (category != category_)
		{
			return false;
		}
	}

	if (!filter_text_.isEmpty())
	{
		if (!word.startsWith(filter_text_, Qt::CaseInsensitive))
		{
			return false;
		}
	}

	return true;
}

QVariant EquationCompletionFilterModel::data(const QModelIndex &index, int role) const
{
	if (display_only_word_ && role == Qt::DisplayRole)
	{
		QModelIndex srcIdx = mapToSource(index);
		return sourceModel()->data(srcIdx, CompletionListModel::kWordRole);
	}

	return QSortFilterProxyModel::data(index, role);
}

} // namespace gui
} // namespace xequation
