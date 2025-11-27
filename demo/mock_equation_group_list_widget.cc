#include "mock_equation_group_list_widget.h"
#include "core/equation.h"
#include "core/equation_group.h"
#include "core/equation_manager.h"
#include "core/equation_signals_manager.h"

#include <QAction>
#include <QDebug>
#include <QMenu>
#include <boost/uuid/uuid_io.hpp>

using namespace xequation;

MockEquationGroupListWidget::MockEquationGroupListWidget(xequation::EquationManager *manager, QWidget *parent)
    : QListWidget(parent), manager_(manager)
{
    manager_ = manager;

    SetupUI();
    SetupConnections();
}

void MockEquationGroupListWidget::SetCurrentEquationGroup(const xequation::EquationGroupId& id)
{
    if (id_to_item_map_.contains(id))
    {
        QListWidgetItem* item = id_to_item_map_.value(id);
        setCurrentItem(item);
    }
}

const xequation::EquationGroupId& MockEquationGroupListWidget::GetCurrentEquationGroupId() const
{
    QListWidgetItem* current_item = currentItem();
    if (current_item && item_to_id_map_.contains(current_item))
    {
        auto it = item_to_id_map_.constFind(current_item);
        if (it != item_to_id_map_.constEnd())
        {
            return it.value();
        }
    }
    static const xequation::EquationGroupId empty_id = EquationGroupId();
    return empty_id;
}

void MockEquationGroupListWidget::SetupUI()
{
    std::vector<EquationGroupId> ids = manager_->GetEquationGroupIds();
    for (const auto &id : ids)
    {
        const EquationGroup *group = manager_->GetEquationGroup(id);

        OnEquationGroupAdded(group);
    }

    setContextMenuPolicy(Qt::CustomContextMenu);
}

void MockEquationGroupListWidget::SetupConnections()
{
    group_added_connection_ = manager_->signals_manager().ConnectScoped<EquationEvent::kEquationGroupAdded>(
        [this](const EquationGroup *group) { OnEquationGroupAdded(group); }
    );

    group_removing_connection_ = manager_->signals_manager().ConnectScoped<EquationEvent::kEquationGroupRemoving>(
        [this](const EquationGroup *group) { OnEquationGroupRemoving(group); }
    );

    group_updated_connection_ = manager_->signals_manager().ConnectScoped<EquationEvent::kEquationGroupUpdated>(
        [this](const EquationGroup *group, bitmask::bitmask<EquationGroupUpdateFlag> change_type) {
            OnEquationGroupUpdated(group, change_type);
        }
    );
    
    connect(
        this, &QListWidget::customContextMenuRequested, this, &MockEquationGroupListWidget::OnCustomContextMenuRequested
    );

    connect(
        this, &QListWidget::currentItemChanged, this, &MockEquationGroupListWidget::OnCurrentItemChanged
    );
}

void MockEquationGroupListWidget::OnEquationGroupAdded(const xequation::EquationGroup *group)
{
    if (!group)
        return;

    QString show_text = QString::fromStdString(group->statement());

    QListWidgetItem *item = new QListWidgetItem(show_text);
    addItem(item);
    id_to_item_map_.insert(group->id(), item);
    item_to_id_map_.insert(item, group->id());
}

void MockEquationGroupListWidget::OnEquationGroupRemoving(const xequation::EquationGroup *group)
{
    if (!group || !id_to_item_map_.contains(group->id()))
    {
        return;
    }

    QListWidgetItem *item = id_to_item_map_.value(group->id());
    if (item)
    {
        int item_row = row(item);
        if (item_row >= 0)
        {
            takeItem(item_row);
        }
        delete item;
        id_to_item_map_.remove(group->id());
        item_to_id_map_.remove(item);
    }
}

void MockEquationGroupListWidget::OnEquationGroupUpdated(
    const xequation::EquationGroup *group, bitmask::bitmask<xequation::EquationGroupUpdateFlag> change_type
)
{
    if (!group || !id_to_item_map_.contains(group->id()))
    {
        return;
    }

    if (change_type & EquationGroupUpdateFlag::kStatement)
    {
        QListWidgetItem *item = id_to_item_map_.value(group->id());
        if (item)
        {
            QString show_text = QString::fromStdString(group->statement());
            item->setText(show_text);
        }
    }
}

void MockEquationGroupListWidget::OnCustomContextMenuRequested(const QPoint &pos)
{
    QListWidgetItem *item = itemAt(pos);

    if (!item)
    {
        return;
    }

    static QMenu *menu = nullptr;
    static QAction *edit_action = nullptr;
    static QAction *delete_action = nullptr;
    static QAction *copy_action = nullptr;

    if (!menu)
    {
        menu = new QMenu(this);

        edit_action = menu->addAction("Edit");
        delete_action = menu->addAction("Delete");
        copy_action = menu->addAction("Copy");

        connect(edit_action, &QAction::triggered, [this]() {
            QListWidgetItem *item =
                reinterpret_cast<QListWidgetItem *>(menu->property("CurrentItem").value<quintptr>());
            emit EditEquationGroupRequested(item_to_id_map_.value(item));
        });
        connect(delete_action, &QAction::triggered, [this]() {
            QListWidgetItem *item =
                reinterpret_cast<QListWidgetItem *>(menu->property("CurrentItem").value<quintptr>());
            emit RemoveEquationGroupRequested(item_to_id_map_.value(item));
        });
        connect(copy_action, &QAction::triggered, [this]() {
            QListWidgetItem *item =
                reinterpret_cast<QListWidgetItem *>(menu->property("CurrentItem").value<quintptr>());
            emit CopyEquationGroupRequested(item_to_id_map_.value(item));
        });
    }
    menu->setProperty("CurrentItem", reinterpret_cast<quintptr>(item));
    menu->exec(mapToGlobal(pos));
}

void MockEquationGroupListWidget::OnCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);

    if (!current)
    {
        return;
    }

    emit EquationGroupSelected(item_to_id_map_.value(current));
}