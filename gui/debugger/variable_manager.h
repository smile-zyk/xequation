#pragma once
#include <QObject>
#include <QString>
#include <QSet>
#include <tsl/ordered_set.h>

namespace xequation
{
namespace gui
{

class VariableManager;

class Variable
{
  public:
    using OrderedSet = tsl::ordered_set<Variable*>;
    Variable(const QString &name, const QString &value, const QString &type);
    ~Variable();

    void AddChild(Variable *child);
    void AddChildren(const QList<Variable*> &children);
    void RemoveChild(Variable *child);
    void RemoveChildren(const QList<Variable*> &children);
    void ClearChildren();

    Variable *GetChildAt(int index) const;
    int IndexOfChild(Variable *child) const;
    bool HasChild(Variable *child) const;

    void SetValue(const QString &value);
    void SetType(const QString &type);

    const QString &name() const
    {
        return name_;
    }

    const QString &value() const
    {
        return value_;
    }

    const QString &type() const
    {
        return type_;
    }

    Variable *parent() const
    {
        return parent_;
    }

    int ChildCount() const
    {
        return child_ordered_set_.size();
    }
    
    const OrderedSet& children() const
    {
        return child_ordered_set_;
    }

    VariableManager* manager()
    {
        return manager_;
    }

  private:
    friend class VariableManager;
    QString name_;
    QString value_;
    QString type_;
    OrderedSet child_ordered_set_;
    Variable *parent_;
    VariableManager* manager_;
};

class VariableManager : public QObject
{
    Q_OBJECT
  public:
    VariableManager(QObject* parent = nullptr);

    ~VariableManager();

    Variable *CreateVariable(const QString &name, const QString &value, const QString &type);
    void RemoveVariable(Variable* variable);
    Variable* GetVariableAt(int index);
    void Clear();

    bool IsContain(Variable* variable) const;
    void BeginUpdate();
    void EndUpdate();
    void SetVariableValue(Variable* variable, const QString &value);
    void SetVariableType(Variable* variable, const QString &type);
    
    void AddVariableChild(Variable* parent, Variable* child);
    void RemoveVariableChild(Variable* parent, Variable* child);
    void AddVariableChildren(Variable* parent, const QList<Variable*>& children);
    void RemoveVariableChildren(Variable* parent, const QList<Variable*>& children);

    int Count() const
    {
        return variable_set_.size();
    }

  signals:
    void VariableBeginInsert(Variable* parent, int index, int count);
    void VariableEndInsert();
    void VariableBeginRemove(Variable* parent, int index, int count);
    void VariableEndRemove();
    void VariableChanged(Variable* variable);
    void VariablesChanged(const QList<Variable*>& variables);

  private:
    Variable::OrderedSet variable_set_;
    bool updating_ = false;
    QList<Variable*> updated_variables_;
};

} // namespace gui
} // namespace xequation
