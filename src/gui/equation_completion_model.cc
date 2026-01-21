#include "equation_completion_model.h"
#include "core/bitmask.hpp"
#include "core/equation.h"
#include "core/equation_common.h"
#include "python/python_completion_model.h"
#include <QFile>
#include <QLanguage>
#include <QSet>
#include <QString>
namespace xequation
{
namespace gui
{

EquationCompletionModel::EquationCompletionModel(const EquationContext *context, QObject *parent)
    : QSortFilterProxyModel(parent), context_(context)
{
    if (context->engine_info().name == "Python")
    {
        model_ = new PythonCompletionModel(this);
        setSourceModel(model_);
    }
}

void EquationCompletionModel::OnEquationUpdated(
    const Equation *equation, bitmask::bitmask<EquationUpdateFlag> update_flags
)
{
    if (!equation)
    {
        return;
    }

    if (update_flags & EquationUpdateFlag::kValue && equation->status() == ResultStatus::kSuccess)
    {
        const QString word = QString::fromStdString(equation->name());
        QString type = QString::fromStdString(context_->GetSymbolType(word.toStdString()));
        model_->RemoveCompletionItem(word, type);

        QString category = QString::fromStdString(context_->GetTypeCategory(type.toStdString()));
        model_->AddCompletionItem(word, type, category);
    }
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

void EquationCompletionModel::SetEquationGroup(const EquationGroup *group)
{
    group_ = group;
    invalidateFilter();
}

void EquationCompletionModel::AddCompletionItem(
    const QString &word, const QString &type, const QString &category, const QString &complete_content
)
{
    if (model_)
    {
        model_->AddCompletionItem(word, type, category, complete_content);
    }
}

void EquationCompletionModel::RemoveCompletionItem(const QString &word, const QString &type)
{
    if (model_)
    {
        model_->RemoveCompletionItem(word, type);
    }
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

    return true;
}

} // namespace gui
} // namespace xequation
