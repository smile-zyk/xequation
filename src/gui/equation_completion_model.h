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

    void OnEquationUpdated(const Equation *equation, bitmask::bitmask<EquationUpdateFlag> update_flags);
    void OnEquationRemoving(const Equation *equation);

    void SetEquationGroup(const EquationGroup *group);

    void AddCompletionItem(const QString &word, const QString &type, const QString &category = "", const QString &complete_content = "");
    void RemoveCompletionItem(const QString &word, const QString &type);

    const EquationContext* context() const { return context_; }

  protected:
    CompletionModel *model_{};
    const EquationContext* context_{};

    // editing group
    const EquationGroup *group_{};

    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
};

} // namespace gui
} // namespace xequation