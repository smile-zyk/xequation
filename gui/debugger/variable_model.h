#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QVector>
#include <tsl/ordered_map.h>

namespace xequation
{
namespace gui
{

class Variable
{
public:
    static std::unique_ptr<Variable> Create(const QString &name, const QString &value, const QString &type);
    void set_name(const QString &name) { name_ = name; }
    void set_value(const QString &value) { value_ = value; }
    void set_type(const QString &type) { type_ = type; }
    const QString &name() const { return name_; }
    const QString &value() const { return value_; }
    const QString &type() const { return type_; }
    const Variable* parent() const { return parent_; }

    void AddChild(std::unique_ptr<Variable> child);
    void RemoveChild(Variable *child);
    void ClearChildren();
    int ChildCount() const;

    QList<Variable*> GetChildren() const;
    Variable *GetChildAt(int index) const;
    int IndexOfChild(Variable *child) const;
    bool HasChild(Variable *child) const;
private:
    Variable(const QString &name, const QString &value, const QString &type);
    Variable(const Variable&) = delete;
    Variable& operator=(const Variable&) = delete;
    Variable(Variable&&) = delete;
    Variable& operator=(Variable&&) = delete;
    ~Variable();
    QString name_;
    QString value_;
    QString type_;
    const Variable* parent_ = nullptr;
    tsl::ordered_map<Variable*, std::unique_ptr<Variable>> children_;
};

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

    void AddRootVariable(Variable *variable);
    void RemoveRootVariable(Variable *variable);
    void ClearRootVariables();
    QList<Variable*> GetRootVariables() const;
    Variable *GetRootVariableAt(int index) const;
    int IndexOfRootVariable(Variable *variable) const;
    bool IsContainRootVariable(Variable *variable) const;

  private:
    Variable *GetVariableFromIndex(const QModelIndex &index) const;
    QModelIndex GetIndexFromVariable(Variable *variable) const;

  private:
    tsl::ordered_map<Variable*, std::unique_ptr<Variable>> root_variable_map_;
};

} // namespace gui
} // namespace xequation