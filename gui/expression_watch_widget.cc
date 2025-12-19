#include "expression_watch_widget.h"
#include "core/equation_common.h"
#include "value_model_view/value_item.h"
#include <QVBoxLayout>

namespace xequation
{
namespace gui
{
ExpressionWatchModel::ExpressionWatchModel(QObject *parent) : ValueTreeModel(parent)
{
    placeholder_flag_ = reinterpret_cast<void *>(0x1);
}

QModelIndex ExpressionWatchModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid() && row == GetRootItemCount())
    {
        return createIndex(row, column, placeholder_flag_);
    }
    return ValueTreeModel::index(row, column, parent);
}

QModelIndex ExpressionWatchModel::parent(const QModelIndex &child) const
{
    if (IsPlaceHolderIndex(child))
    {
        return QModelIndex();
    }
    return ValueTreeModel::parent(child);
}

int ExpressionWatchModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
    {
        return GetRootItemCount() + 1;
    }
    return ValueTreeModel::rowCount(parent);
}

QVariant ExpressionWatchModel::data(const QModelIndex &index, int role) const
{
    if (IsPlaceHolderIndex(index))
    {
        if (index.column() == 0)
        {
            switch (role)
            {
            case Qt::DisplayRole:
                return "Add item to watch...";
            case Qt::EditRole:
                return "";
            case Qt::ForegroundRole:
                return QColor(Qt::gray);
            case Qt::FontRole: {
                QFont font;
                font.setItalic(true);
                return font;
            }
            default:
                return QVariant();
            }
        }
        return QVariant();
    }
    return ValueTreeModel::data(index, role);
}

bool ExpressionWatchModel::hasChildren(const QModelIndex &parent) const
{
    if (!parent.isValid())
    {
        // The root has an extra placeholder child
        return true;
    }
    if (IsPlaceHolderIndex(parent))
    {
        return false;
    }
    return ValueTreeModel::hasChildren(parent);
}

bool ExpressionWatchModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;
    QString new_expression = value.toString().trimmed();

    if (new_expression.isEmpty())
    {
        return false;
    }

    if (IsPlaceHolderIndex(index))
    {
        emit RequestAddWatchExpression(new_expression);
        return true;
    }

    int row = index.row();
    if (row < 0 || row >= root_items_.size())
    {
        return false;
    }

    QString current_expression = root_items_[row]->name();
    if (current_expression == new_expression)
    {
        return false;
    }
    emit RequestReplaceWatchExpression(current_expression, new_expression);
    return true;
}

Qt::ItemFlags ExpressionWatchModel::flags(const QModelIndex &index) const
{
    if (index.column() == 0 && !index.parent().isValid())
    {
        return Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsSelectable;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool ExpressionWatchModel::IsPlaceHolderIndex(const QModelIndex &index) const
{
    return index.row() == GetRootItemCount() && index.isValid() && index.internalPointer() == placeholder_flag_;
}

void ExpressionWatchModel::OnExpressionValueItemAdded(ValueItem *item)
{
    if (!item)
        return;

    int row = root_items_.size();
    beginInsertRows(QModelIndex(), row, row);
    root_items_.push_back(item);
    endInsertRows();
}

void ExpressionWatchModel::OnExpressionValueItemReplaced(ValueItem *old_item, ValueItem *new_item)
{
    if (!old_item || !new_item)
        return;

    int index = root_items_.indexOf(old_item);
    if (index < 0)
        return;

    beginRemoveRows(QModelIndex(), index, index);
    root_items_.removeAt(index);
    endRemoveRows();

    beginInsertRows(QModelIndex(), index, index);
    root_items_.insert(index, new_item);
    endInsertRows();
}

ExpressionWatchWidget::ExpressionWatchWidget(QWidget *parent) : QWidget(parent)
{
    SetupUI();
    SetupConnections();
}

void ExpressionWatchWidget::SetupUI()
{
    setWindowTitle("Expression Watch");
    setWindowFlags(
        Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint
    );

    view_ = new ValueTreeView(this);
    model_ = new ExpressionWatchModel(view_);

    view_->SetValueModel(model_);
    view_->SetHeaderSectionResizeRatio(0, 1);
    view_->SetHeaderSectionResizeRatio(1, 3);
    view_->SetHeaderSectionResizeRatio(2, 1);

    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->addWidget(view_);
    setLayout(main_layout);

    setMinimumSize(800, 600);
}

void ExpressionWatchWidget::SetupConnections()
{
    connect(
        model_, &ExpressionWatchModel::RequestAddWatchExpression, this,
        &ExpressionWatchWidget::OnRequestAddWatchExpression
    );

    connect(
        model_, &ExpressionWatchModel::RequestReplaceWatchExpression, this,
        &ExpressionWatchWidget::OnRequestReplaceWatchExpression
    );
}

void ExpressionWatchWidget::OnRequestAddWatchExpression(const QString &expression)
{
    ValueItem::UniquePtr item;
    if (eval_handler_ == nullptr || parse_handler_ == nullptr)
    {
        return;
    }

    // first parse the expression to check for errors
    ParseResult parse_result = parse_handler_(expression.toStdString());
    if (parse_result.items.size() != 1)
    {
        return;
    }
    const ParseResultItem &parse_item = parse_result.items[0];
    if (parse_item.status != ResultStatus::kSuccess)
    {
        item = ValueItem::Create(
            expression, QString::fromStdString(parse_item.message),
            QString::fromStdString(ResultStatusConverter::ToString(parse_item.status))
        );
    }
    else 
    {
        const auto& dependencies = parse_item.dependencies;
        for(const auto& dep : dependencies)
        {
            equation_to_watch_expressions_map_[QString::fromStdString(dep)].insert(expression);
        }
        // then evaluate the expression
        InterpretResult interpret_result = eval_handler_(expression.toStdString());
        if (interpret_result.status != ResultStatus::kSuccess)
        {
            item = ValueItem::Create(
                expression, QString::fromStdString(interpret_result.message),
                QString::fromStdString(ResultStatusConverter::ToString(interpret_result.status))
            );
        }
        else
        {
            Value value = interpret_result.value;

        }
    }

    ValueItem::UniquePtr item = ValueItem::Create(variable_name, "test", "type");
    model_->OnExpressionValueItemAdded(item.get());
    watch_items_map_[variable_name] = std::move(item);
}

void ExpressionWatchWidget::OnRequestReplaceWatchExpression(
    const QString &old_variable_name, const QString &new_variable_name
)
{
    ValueItem::UniquePtr item = ValueItem::Create(new_variable_name, "test", "type");
    ValueItem *old_item = nullptr;
    if (watch_items_map_.find(old_variable_name) != watch_items_map_.end())
    {
        old_item = watch_items_map_[old_variable_name].get();
    }
    watch_items_map_[new_variable_name] = std::move(item);
    model_->OnExpressionValueItemReplaced(old_item, watch_items_map_[new_variable_name].get());
    watch_items_map_.erase(old_variable_name);
}

} // namespace gui
} // namespace xequation