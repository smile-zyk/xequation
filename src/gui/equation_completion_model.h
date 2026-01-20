#pragma once

#include <QSortFilterProxyModel>

#include "code_editor/completion_model.h"
#include "core/equation.h"
#include "core/equation_context.h"
#include "core/equation_group.h"

namespace xequation
{
namespace gui
{
class EquationCompletionModel : public QSortFilterProxyModel
{
    Q_OBJECT
  public:
    explicit EquationCompletionModel(const EquationContext* context, QObject *parent = nullptr);
    ~EquationCompletionModel() override = default;

    void OnEquationAdded(const Equation *equation);
    void OnEquationRemoving(const Equation *equation);

    void SetDisplayOnlyWord(bool display_only_word);
    void SetEquationGroup(const EquationGroup *group);
    void SetFilterText(const QString &filter_text);
    void SetCategory(const QString &category);
    QList<QString> GetAllCategories();

    const EquationContext* context() const { return context_; }

  protected:
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

  protected:
    CompletionModel *model_{};
    const EquationContext* context_{};

    // editing group
    const EquationGroup *group_{};

    // filter text
    QString filter_text_;

    // filter category
    QString filter_category_;
    
    bool display_only_word_{false};
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
};

} // namespace gui
} // namespace xequation