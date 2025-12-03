#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QVector>

namespace xequation
{
namespace gui
{
class Variable;
class VariableManager;
class VariableModel : public QAbstractItemModel
{
    Q_OBJECT
  public:
    explicit VariableModel(QObject *parent = nullptr);
    ~VariableModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void SetRootVariable(const QList<Variable *> &variable_list);
    void AddRootVariable(Variable *variable);
    void RemoveRootVariable(Variable *variable);
    void ClearRootVariables();
    Variable *GetRootVariableAt(int index) const;

  protected:
    void OnVariableChanged(Variable* variable);
    void OnVariablesChanged(const QList<Variable*>& variables);
    void OnVariableChildInserted(Variable* parent, Variable* child);
    void OnVariableChildRemoved(Variable* parent, Variable* child);
    void OnVariableChildrenInserted(Variable* parent, const QList<Variable*>& children);
    void OnVariableChildrenRemoved(Variable* parent, const QList<Variable*>& children);

    void VariableInserted(Variable* variable, Variable* parent = nullptr);
    void VariableRemoved(Variable* variable, Variable* parent = nullptr);

  private:
    Variable *GetVariableFromIndex(const QModelIndex &index) const;
    QModelIndex GetIndexFromVariable(Variable *data) const;
    int RowOfChildInParent(Variable *parent, Variable *child) const;

  private:
    QList<Variable *> root_variable_list_;
    QHash<VariableManager*, QList<Variable*>> variable_manager_cache_;
};

} // namespace gui
} // namespace xequation