#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QVector>
#include "variable_manager.h"

namespace xequation
{
namespace gui
{
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
    int IndexOfRootVariable(Variable *variable) const;
    bool IsContainRootVariable(Variable *variable) const;

  protected:
    void OnVariableBeginInsert(Variable* parent, int index, int count);
    void OnVariableEndInsert();
    void OnVariableBeginRemove(Variable* parent, int index, int count);
    void OnVariableEndRemove();
    void OnVariableChanged(Variable* variable);
    void OnVariablesChanged(const QList<Variable*>& variables);

    void ConnectVariableManager(Variable* variable);
    void DisconnectVariableManager(Variable* variable);

  private:
    Variable *GetVariableFromIndex(const QModelIndex &index) const;
    QModelIndex GetIndexFromVariable(Variable *variable) const;

  private:
    Variable::OrderedSet root_variable_set_;
    QHash<VariableManager*, QList<Variable*>> variable_manager_cache_;
};

} // namespace gui
} // namespace xequation