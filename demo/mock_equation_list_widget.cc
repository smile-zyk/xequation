#include "mock_equation_list_widget.h"
#include "core/equation.h"
#include "core/equation_group.h"
#include "core/equation_manager.h"
#include "core/equation_signals_manager.h"
#include <qlistwidget.h>

using namespace xequation;

MockEquationListWidget::MockEquationListWidget(xequation::EquationManager *manager, QWidget *parent)
    : QListWidget(parent), manager_(manager)
{
    manager_ = manager;

    SetupUI();

    group_added_connection_ = manager_->signals_manager().ConnectScoped<EquationEvent::kEquationGroupAdded>([this](const EquationGroup *group) {
        OnEquationGroupAdded(group);
    });

    group_removing_connection_ = manager_->signals_manager().ConnectScoped<EquationEvent::kEquationGroupRemoving>([this](const EquationGroup *group) {
        OnEquationGroupRemoving(group);
    });

    group_updated_connection_ = manager_->signals_manager().ConnectScoped<EquationEvent::kEquationGroupUpdated>([this](const EquationGroup *group, bitmask::bitmask<EquationGroupUpdateFlag> change_type) {
    });
}

void MockEquationListWidget::SetupUI()
{
    std::vector<EquationGroupId> ids = manager_->GetEquationGroupIds();
    for(const auto& id : ids)
    {
        const EquationGroup* group = manager_->GetEquationGroup(id);
        
        OnEquationGroupAdded(group);
    }
}

void MockEquationListWidget::OnEquationGroupAdded(const xequation::EquationGroup *group)
{
    if (!group)
        return;

    QString show_text = QString::fromStdString(group->statement());

    QListWidgetItem *item = new QListWidgetItem(show_text);
    addItem(item);
    item_map_.insert(group->id(), item);
}

void MockEquationListWidget::OnEquationGroupRemoving(const xequation::EquationGroup *group)
{
    if (!group || !item_map_.contains(group->id()))
    {
        return;
    }

    QListWidgetItem *item = item_map_.value(group->id());
    if (item)
    {
        int item_row = row(item);
        if (item_row >= 0)
        {
            takeItem(item_row);
        }
        delete item;
        item_map_.remove(group->id());
    }
}

void MockEquationListWidget::OnEquationGroupUpdated(const xequation::EquationGroup *group, bitmask::bitmask<xequation::EquationGroupUpdateFlag> change_type)
{
    if (!group || !item_map_.contains(group->id()))
    {
        return;
    }

    if (change_type & EquationGroupUpdateFlag::kStatement)
    {
        QListWidgetItem *item = item_map_.value(group->id());
        if (item)
        {
            QString show_text = QString::fromStdString(group->statement());
            item->setText(show_text);
        }
    }
}