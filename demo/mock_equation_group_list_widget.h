#pragma once

#include "core/equation.h"
#include "core/equation_group.h"
#include "core/equation_manager.h"

#include <QListWidget>
#include <vector>

class MockEquationGroupListWidget : public QListWidget
{
    Q_OBJECT
public:
    MockEquationGroupListWidget(xequation::EquationManager* manager, QWidget* parent = nullptr);
    ~MockEquationGroupListWidget() override = default;

    void SetCurrentEquationGroup(const xequation::EquationGroupId& id);
    const xequation::EquationGroupId& GetCurrentEquationGroupId() const;
    std::vector<xequation::EquationGroupId> GetSelectedEquationGroupIds() const;

signals:
    void EditEquationGroupRequested(const xequation::EquationGroupId& id);
    void RemoveEquationGroupRequested(const xequation::EquationGroupId& id);
    void CopyEquationGroupRequested(const xequation::EquationGroupId& id);
    void UpdateEquationGroupRequested(const xequation::EquationGroupId& id);
    void AddEquationGroupToExpressionWatchRequested(const xequation::EquationGroupId& id);
    void EquationGroupSelected(const xequation::EquationGroupId& id);
    void EquationGroupsSelected(const std::vector<xequation::EquationGroupId>& ids);

private:
    void SetupUI();
    void SetupConnections();

    void OnEquationGroupAdded(const xequation::EquationGroup* group);
    void OnEquationGroupRemoving(const xequation::EquationGroup* group);
    void OnEquationGroupUpdated(const xequation::EquationGroup* group, bitmask::bitmask<xequation::EquationGroupUpdateFlag> change_type);

    void OnCustomContextMenuRequested(const QPoint& pos);
    void OnCurrentItemChanged(QListWidgetItem* current, QListWidgetItem* previous);
    void OnSelectionChanged();

private:
    xequation::EquationManager* manager_;
    QMap<xequation::EquationGroupId, QListWidgetItem*> id_to_item_map_;
    QMap<QListWidgetItem*, xequation::EquationGroupId> item_to_id_map_;
};