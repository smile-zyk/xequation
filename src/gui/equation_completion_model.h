#pragma once

#include <QSortFilterProxyModel>

#include "code_editor/completion_list_model.h"
#include "core/equation.h"
#include "core/equation_context.h"
#include "core/equation_group.h"

namespace xequation
{
namespace gui
{
class EquationCompletionModel : public CompletionListModel
{
    Q_OBJECT
  public:
    explicit EquationCompletionModel(const EquationContext *context, QObject *parent = nullptr)
        : CompletionListModel(context->engine_info(), parent), context_(context)
    {
        InitWithLanguageDefinition(context->engine_info());
    }
    ~EquationCompletionModel() override = default;

    void OnEquationAdded(const Equation *equation);
    void OnEquationRemoving(const Equation *equation);

    const EquationContext* context() const
    {
        return context_;
    }

  private:
    void InitWithLanguageDefinition(const xequation::EquationEngineInfo &engine_info);

    const EquationContext* context_{};
};

class EquationCompletionFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
  public:
    explicit EquationCompletionFilterModel(EquationCompletionModel *model, QObject *parent = nullptr);
    ~EquationCompletionFilterModel() override = default;

    void OnEquationAdded(const Equation *equation);
    void OnEquationRemoving(const Equation *equation);

    void SetDisplayOnlyWord(bool display_only_word);
    void SetEquationGroup(const EquationGroup *group);
    void SetFilterText(const QString &filter_text);
    void SetCategory(const QString &category);
    QList<QString> GetAllCategories();

  protected:
    void setSourceModel(QAbstractItemModel *sourceModel) override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

  protected:
    EquationCompletionModel *model_;
    const EquationGroup *group_{};
    QString filter_text_;
    QString category_;
    bool display_only_word_{false};
    QSet<CompletionItemType> visible_types_;
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
};

} // namespace gui
} // namespace xequation