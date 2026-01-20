#include "equation_completion_model.h"
#include "core/equation.h"
#include "core/equation_common.h"
#include "python/python_completion_model.h"
#include <QString>
#include <QSet>
#include <QFile>
#include <QLanguage>

namespace xequation
{
namespace gui
{

EquationCompletionModel::EquationCompletionModel(const EquationContext* context, QObject *parent)
	: QSortFilterProxyModel(parent), context_(context)
{
	if(context->engine_info().name == "Python")
	{
		model_ = new PythonCompletionModel(this);
		setSourceModel(model_);
	}
}

void EquationCompletionModel::OnEquationAdded(const Equation *equation)
{
	if (!equation)
	{
		return;
	}

	QString word = QString::fromStdString(equation->name());
	QString type = QString::fromStdString(context_->GetSymbolType(word.toStdString()));
	QString category = QString::fromStdString(context_->GetTypeCategory(type.toStdString()));
	
    model_->AddCompletionItem(word, type, category);
}

void EquationCompletionModel::OnEquationRemoving(const Equation *equation)
{
	if (!equation)
	{
		return;
	}

	const QString word = QString::fromStdString(equation->name());
    QString type = QString::fromStdString(context_->GetSymbolType(word.toStdString()));
	model_->RemoveCompletionItem(word, type);
}

void EquationCompletionModel::SetDisplayOnlyWord(bool display_only_word)
{
	display_only_word_ = display_only_word;
	// Refresh displayed data for all rows
	if (rowCount() > 0)
	{
		emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
	}
}

void EquationCompletionModel::SetEquationGroup(const EquationGroup* group)
{
	group_ = group;
	invalidateFilter();
}

void EquationCompletionModel::SetFilterText(const QString &filter_text)
{
	filter_text_ = filter_text;
	invalidateFilter();
}

void EquationCompletionModel::SetCategory(const QString &category)
{
	filter_category_ = category;
	invalidateFilter();
}

QList<QString> EquationCompletionModel::GetAllCategories()
{
	QList<QString> categories;
	if (!model_)
	{
		return categories;
	}

	QSet<QString> names;
	const int row_count = model_->rowCount();
	for (int i = 0; i < row_count; ++i)
	{
		QModelIndex idx = model_->index(i, 0);
		QString word = model_->data(idx, CompletionModel::kWordRole).toString();

		// Group filter: skip words existing in the equation group
		if (group_ && group_->IsEquationExist(word.toStdString()))
		{
			continue;
		}

		QString name = model_->data(idx, CompletionModel::kCategoryRole).toString();

		if (names.contains(name))
		{
			continue;
		}

		names.insert(name);
		categories.append(name);
	}

	std::sort(categories.begin(), categories.end());
	return categories;
}

bool EquationCompletionModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
	auto *src = sourceModel();
	if (!src)
	{
		return true;
	}

	QModelIndex idx = src->index(source_row, 0, source_parent);
	QString word = src->data(idx, CompletionModel::kWordRole).toString();
	if (word.isEmpty())
	{
		return true;
	}

	// Group filter: hide words already present in the equation group
	if (group_ && group_->IsEquationExist(word.toStdString()))
	{
		return false;
	}

	if (!filter_category_.isEmpty())
	{
		QString category = src->data(idx, CompletionModel::kCategoryRole).toString();
		if (category != filter_category_)
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

QVariant EquationCompletionModel::data(const QModelIndex &index, int role) const
{
	if (display_only_word_ && role == Qt::DisplayRole)
	{
		QModelIndex srcIdx = mapToSource(index);
		return sourceModel()->data(srcIdx, CompletionModel::kWordRole);
	}

	return QSortFilterProxyModel::data(index, role);
}

} // namespace gui
} // namespace xequation
