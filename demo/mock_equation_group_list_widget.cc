#include "mock_equation_group_list_widget.h"
#include "core/equation.h"
#include "core/equation_group.h"
#include "core/equation_manager.h"
#include "core/equation_signals_manager.h"

#include <QAction>
#include <QDebug>
#include <QMenu>
#include <boost/uuid/uuid_io.hpp>
#include <qvariant.h>


using namespace xequation;

MockEquationGroupListWidget::MockEquationGroupListWidget(xequation::EquationManager *manager, QWidget *parent)
    : QListWidget(parent), manager_(manager)
{
    manager_ = manager;

    SetupUI();
    SetupConnections();
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
    connect(
        this, &QListWidget::customContextMenuRequested, this, &MockEquationGroupListWidget::OnCustomContextMenuRequested
    );

    group_added_connection_ = manager_->signals_manager().ConnectScoped<EquationEvent::kEquationGroupAdded>(
        [this](const EquationGroup *group) { OnEquationGroupAdded(group); }
    );

    group_removing_connection_ = manager_->signals_manager().ConnectScoped<EquationEvent::kEquationGroupRemoving>(
        [this](const EquationGroup *group) { OnEquationGroupRemoving(group); }
    );

    group_updated_connection_ = manager_->signals_manager().ConnectScoped<EquationEvent::kEquationGroupUpdated>(
        [this](const EquationGroup *group, bitmask::bitmask<EquationGroupUpdateFlag> change_type) {}
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
            QListWidgetItem *item = reinterpret_cast<QListWidgetItem *>(menu->property("CurrentItem").value<quintptr>());
            qDebug() << "Edit action triggered for item:" << item->text();
            emit OnEditEquationGroup(item_to_id_map_.value(item));
        });
        connect(delete_action, &QAction::triggered, [this]() {
            QListWidgetItem *item = reinterpret_cast<QListWidgetItem *>(menu->property("CurrentItem").value<quintptr>());
            qDebug() << "Delete action triggered for item:" << item->text();
            emit OnRemoveEquationGroup(item_to_id_map_.value(item));
        });
        connect(copy_action, &QAction::triggered, [this]() {
            QListWidgetItem *item = reinterpret_cast<QListWidgetItem *>(menu->property("CurrentItem").value<quintptr>());
            qDebug() << "Copy action triggered for item:" << item->text();
            emit OnCopyEquationGroup(item_to_id_map_.value(item));
        });
    }
    menu->setProperty("CurrentItem", reinterpret_cast<quintptr>(item));
    menu->exec(mapToGlobal(pos));
}