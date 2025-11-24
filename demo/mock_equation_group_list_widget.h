#pragma once

#include "core/equation.h"
#include "core/equation_group.h"
#include "core/equation_manager.h"
#include "core/equation_signals_manager.h"

#include <QListWidget>

class MockEquationGroupListWidget : public QListWidget
{
    Q_OBJECT
public:
    MockEquationGroupListWidget(xequation::EquationManager* manager, QWidget* parent = nullptr);
    ~MockEquationGroupListWidget() override = default;

signals:
    void OnEditEquationGroup(const xequation::EquationGroupId& id);
    void OnRemoveEquationGroup(const xequation::EquationGroupId& id);
    void OnCopyEquationGroup(const xequation::EquationGroupId& id);

private:
    void SetupUI();
    void SetupConnections();

    void OnEquationGroupAdded(const xequation::EquationGroup* group);
    void OnEquationGroupRemoving(const xequation::EquationGroup* group);
    void OnEquationGroupUpdated(const xequation::EquationGroup* group, bitmask::bitmask<xequation::EquationGroupUpdateFlag> change_type);

    void OnCustomContextMenuRequested(const QPoint& pos);

private:
    xequation::EquationManager* manager_;
    QMap<xequation::EquationGroupId, QListWidgetItem*> id_to_item_map_;
    QMap<QListWidgetItem*, xequation::EquationGroupId> item_to_id_map_;

    xequation::ScopedConnection group_added_connection_;
    xequation::ScopedConnection group_removing_connection_;
    xequation::ScopedConnection group_updated_connection_;
};