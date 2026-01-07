#include "expression_watch_widget.h"

#include "core/equation_common.h"
#include "value_model_view/value_item.h"
#include "value_model_view/value_item_builder.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QVBoxLayout>


#include <iostream>

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
    if (!parent.isValid() && row == root_items_.size())
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
        return root_items_.size() + 1;
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
        emit RequestAddWatchItem(new_expression);
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
    emit RequestReplaceWatchItem(root_items_[row], new_expression);
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
    return index.row() == root_items_.size() && index.isValid() && index.internalPointer() == placeholder_flag_;
}

void ExpressionWatchModel::AddWatchItem(ValueItem *item)
{
    if (!item)
        return;

    int row = root_items_.size();
    beginInsertRows(QModelIndex(), row, row);
    root_items_.push_back(item);
    endInsertRows();
}

void ExpressionWatchModel::RemoveWatchItem(ValueItem *item)
{
    if (!item)
        return;

    int index = root_items_.indexOf(item);
    if (index < 0)
        return;

    beginRemoveRows(QModelIndex(), index, index);
    root_items_.removeAt(index);
    endRemoveRows();
}

void ExpressionWatchModel::ReplaceWatchItem(ValueItem *old_item, ValueItem *new_item)
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

void ExpressionWatchWidget::OnEquationRemoved(const std::string &equation_name)
{
    auto range = expression_item_equation_name_bimap_.right.equal_range(equation_name);
    std::vector<ValueItem *> items_to_update;
    for (auto it = range.first; it != range.second; ++it)
    {
        ValueItem *item = it->get_left();
        items_to_update.push_back(item);
    }
    for (auto item : items_to_update)
    {
        auto expression = item->name();
        // recreate the watch item
        auto new_item = CreateWatchItem(expression);
        if (new_item)
        {
            model_->ReplaceWatchItem(item, new_item);
            DeleteWatchItem(item);
        }
    }
}

void ExpressionWatchWidget::OnEquationUpdated(
    const Equation *equation, bitmask::bitmask<EquationUpdateFlag> change_type
)
{
    if (change_type & EquationUpdateFlag::kValue)
    {
        // find all watch items depending on this equation
        auto range = expression_item_equation_name_bimap_.right.equal_range(equation->name());
        std::vector<ValueItem *> items_to_update;
        for (auto it = range.first; it != range.second; ++it)
        {
            ValueItem *item = it->get_left();
            items_to_update.push_back(item);
        }
        for (auto item : items_to_update)
        {
            auto expression = item->name();
            // recreate the watch item
            auto new_item = CreateWatchItem(expression);
            if (new_item)
            {
                model_->ReplaceWatchItem(item, new_item);
                DeleteWatchItem(item);
            }
        }
    }
}

void ExpressionWatchWidget::OnAddExpressionToWatch(const QString &expression)
{
    ValueItem *item = CreateWatchItem(expression);
    if (item)
    {
        model_->AddWatchItem(item);
        SetCurrentItemToPlaceholder();
    }
}

void ExpressionWatchWidget::OnEvalResultSubmitted(ValueItem *old_item, const InterpretResult &result)
{
    if (!old_item || expression_item_equation_name_bimap_.left.count(old_item) == 0)
        return;

    ValueItem::UniquePtr new_item;

    QString expression = old_item->name();

    if (result.status != ResultStatus::kSuccess)
    {
        new_item = ValueItem::Create(
            expression, QString::fromStdString(result.message),
            QString::fromStdString(ResultStatusConverter::ToString(result.status))
        );
    }
    else
    {
        Value value = result.value;
        new_item = BuilderUtils::CreateValueItem(expression, value);
    }

    auto new_item_ptr = new_item.get();
    // get old item dependencies
    auto range = expression_item_equation_name_bimap_.left.equal_range(old_item);
    std::vector<std::string> old_dependencies;
    for (auto it = range.first; it != range.second; ++it)
    {
        old_dependencies.push_back(it->get_right());
    }
    for (const auto &dep : old_dependencies)
    {
        expression_item_equation_name_bimap_.insert({new_item_ptr, dep});
    }
    // insert new item
    expression_item_map_.insert({expression.toStdString(), std::move(new_item)});

    model_->ReplaceWatchItem(old_item, new_item_ptr);
    DeleteWatchItem(old_item);
}

void ExpressionWatchWidget::SetupUI()
{
    setWindowTitle("Expression Watch");
    setWindowFlags(
        Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint
    );
    setContextMenuPolicy(Qt::CustomContextMenu);
    setMinimumSize(800, 600);

    view_ = new ValueTreeView(this);
    model_ = new ExpressionWatchModel(view_);

    view_->SetValueModel(model_);
    view_->SetHeaderSectionResizeRatio(0, 1);
    view_->SetHeaderSectionResizeRatio(1, 3);
    view_->SetHeaderSectionResizeRatio(2, 1);
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->addWidget(view_);
    setLayout(main_layout);

    // setup actions
    copy_action_ = new QAction("Copy", this);
    paste_action_ = new QAction("Paste", this);
    edit_expression_action_ = new QAction("Edit", this);
    delete_watch_action_ = new QAction("Delete", this);
    select_all_action_ = new QAction("Select All", this);
    clear_all_action_ = new QAction("Clear All", this);

    copy_action_->setShortcut(QKeySequence::Copy);
    paste_action_->setShortcut(QKeySequence::Paste);
    select_all_action_->setShortcut(QKeySequence::SelectAll);
    delete_watch_action_->setShortcut(QKeySequence::Delete);

    const auto shortcut_context = Qt::WidgetWithChildrenShortcut;
    copy_action_->setShortcutContext(shortcut_context);
    paste_action_->setShortcutContext(shortcut_context);
    edit_expression_action_->setShortcutContext(shortcut_context);
    delete_watch_action_->setShortcutContext(shortcut_context);
    select_all_action_->setShortcutContext(shortcut_context);
    clear_all_action_->setShortcutContext(shortcut_context);

    view_->addActions(
        {copy_action_, paste_action_, edit_expression_action_, delete_watch_action_, select_all_action_,
         clear_all_action_}
    );
    view_->installEventFilter(this);
}

void ExpressionWatchWidget::SetupConnections()
{
    connect(model_, &ExpressionWatchModel::RequestAddWatchItem, this, &ExpressionWatchWidget::OnRequestAddWatchItem);

    connect(
        model_, &ExpressionWatchModel::RequestRemoveWatchItem, this, &ExpressionWatchWidget::OnRequestRemoveWatchItem
    );

    connect(
        model_, &ExpressionWatchModel::RequestReplaceWatchItem, this, &ExpressionWatchWidget::OnRequestReplaceWatchItem
    );

    connect(
        this, &ExpressionWatchWidget::customContextMenuRequested, this,
        &ExpressionWatchWidget::OnCustomContextMenuRequested
    );

    connect(
        view_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
        &ExpressionWatchWidget::OnSelectionChanged
    );

    connect(copy_action_, &QAction::triggered, this, &ExpressionWatchWidget::OnCopyExpressionValue);
    connect(paste_action_, &QAction::triggered, this, &ExpressionWatchWidget::OnPasteExpression);
    connect(edit_expression_action_, &QAction::triggered, this, &ExpressionWatchWidget::OnEditExpression);
    connect(delete_watch_action_, &QAction::triggered, this, &ExpressionWatchWidget::OnDeleteExpression);
    connect(select_all_action_, &QAction::triggered, this, &ExpressionWatchWidget::OnSelectAllExpressions);
    connect(clear_all_action_, &QAction::triggered, this, &ExpressionWatchWidget::OnClearAllExpressions);
}

ValueItem *ExpressionWatchWidget::CreateWatchItem(const QString &expression)
{
    ParseResult parse_result;
    emit ParseResultRequested(expression, parse_result);

    if (parse_result.items.size() != 1)
    {
        return nullptr;
    }

    ValueItem::UniquePtr item;
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
        // create calculating value item
        item = ValueItem::Create(expression, "Calculating...", "Calculating");
        const auto &dependencies = parse_item.dependencies;
        for (const auto &dependency : dependencies)
        {
            expression_item_equation_name_bimap_.insert({item.get(), dependency});
        }
        emit EvalResultAsyncRequested(item.get());
    }
    auto item_ptr = item.get();
    expression_item_map_.insert({expression.toStdString(), std::move(item)});
    return item_ptr;
}

void ExpressionWatchWidget::DeleteWatchItem(ValueItem *item)
{
    if (!item)
        return;

    auto it = expression_item_equation_name_bimap_.left.find(item);
    if (it != expression_item_equation_name_bimap_.left.end())
    {
        expression_item_equation_name_bimap_.left.erase(it);
    }

    auto expression = item->name().toStdString();
    auto range = expression_item_map_.equal_range(expression);
    for (auto it = range.first; it != range.second; ++it)
    {
        if (it->second.get() == item)
        {
            expression_item_map_.erase(it);
            break;
        }
    }
}

void ExpressionWatchWidget::SetCurrentItemToPlaceholder()
{
    view_->setCurrentIndex(model_->index(model_->rowCount() - 1, 0, QModelIndex()));
}

void ExpressionWatchWidget::OnRequestAddWatchItem(const QString &expression)
{
    auto item = CreateWatchItem(expression);
    if (!item)
        return;
    model_->AddWatchItem(item);
}

void ExpressionWatchWidget::OnRequestRemoveWatchItem(ValueItem *item)
{
    model_->RemoveWatchItem(item);
    DeleteWatchItem(item);
}

void ExpressionWatchWidget::OnRequestReplaceWatchItem(ValueItem *old_item, const QString &new_expression)
{
    if (old_item->type() == "Calculating")
    {
        QMessageBox::warning(this, "Warning", "Cannot edit an expression that is still calculating.");
        return;
    }
    auto new_item = CreateWatchItem(new_expression);
    if (!new_item)
        return;
    model_->ReplaceWatchItem(old_item, new_item);
    DeleteWatchItem(old_item);
}

enum SelectionFlags
{
    NoSelection = 0x00,
    HasSelection = 0x01,
    SingleSelection = 0x02,
    HasPlaceholder = 0x04,
    IsTopLevel = 0x08
};

void ExpressionWatchWidget::OnSelectionChanged(const QItemSelection &, const QItemSelection &)
{
    QModelIndexList select_indexs = view_->selectionModel()->selectedRows();
    int flags = GetSelectionFlags(select_indexs, model_);

    bool is_copy_enable = (flags & HasSelection) && !((flags & SingleSelection) && (flags & HasPlaceholder));
    bool is_paste_enable = true;
    bool is_edit_enable = (flags & SingleSelection) && (flags & IsTopLevel) && !(flags & HasPlaceholder);
    bool is_delete_enable = is_copy_enable;
    bool is_select_all_enable = true;
    bool is_clear_all_enable = true;

    copy_action_->setEnabled(is_copy_enable);
    paste_action_->setEnabled(is_paste_enable);
    edit_expression_action_->setEnabled(is_edit_enable);
    delete_watch_action_->setEnabled(is_delete_enable);
    select_all_action_->setEnabled(is_select_all_enable);
    clear_all_action_->setEnabled(is_clear_all_enable);
}

int ExpressionWatchWidget::GetSelectionFlags(const QModelIndexList &indexes, ExpressionWatchModel *model)
{
    int flags = NoSelection;

    if (indexes.isEmpty())
    {
        return flags;
    }

    flags |= HasSelection;

    if (indexes.size() == 1)
    {
        flags |= SingleSelection;

        if (model->IsPlaceHolderIndex(indexes[0]))
        {
            flags |= HasPlaceholder;
        }

        if (!indexes[0].parent().isValid())
        {
            flags |= IsTopLevel;
        }
    }

    return flags;
}

bool ExpressionWatchWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == view_ && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress))
    {
        auto *key_event = static_cast<QKeyEvent *>(event);

        auto handle_action_shortcut = [&](QAction *action) {
            if (!action || action->shortcut().isEmpty())
            {
                return false;
            }

            QKeySequence key_sequence(key_event->modifiers() | key_event->key());
            if (key_sequence != action->shortcut())
            {
                return false;
            }

            if (event->type() == QEvent::KeyPress && action->isEnabled())
            {
                action->trigger();
            }

            event->accept();
            return true;
        };

        if (handle_action_shortcut(copy_action_) || handle_action_shortcut(paste_action_) ||
            handle_action_shortcut(edit_expression_action_) || handle_action_shortcut(delete_watch_action_) ||
            handle_action_shortcut(select_all_action_) || handle_action_shortcut(clear_all_action_))
        {
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void ExpressionWatchWidget::OnCustomContextMenuRequested(const QPoint &pos)
{
    static QMenu *menu = nullptr;

    if (!menu)
    {
        menu = new QMenu(this);

        menu->addAction(copy_action_);
        menu->addAction(paste_action_);
        menu->addAction(edit_expression_action_);
        menu->addAction(delete_watch_action_);
        menu->addAction(select_all_action_);
        menu->addAction(clear_all_action_);
    }

    menu->exec(mapToGlobal(pos));
}

void ExpressionWatchWidget::OnCopyExpressionValue()
{
    QModelIndexList select_indexs = view_->selectionModel()->selectedRows();

    QString combined_text;

    for (const QModelIndex &index : select_indexs)
    {
        if (!index.isValid() || model_->IsPlaceHolderIndex(index))
        {
            continue;
        }

        ValueItem *item = model_->GetValueItemFromIndex(index);
        if (!item)
        {
            continue;
        }

        combined_text += QString::fromStdString(item->value().ToString()) + "\n";
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(combined_text);
}

void ExpressionWatchWidget::OnPasteExpression()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString expression = clipboard->text().trimmed();
    if (expression.isEmpty())
    {
        return;
    }

    auto item = CreateWatchItem(expression);
    if (!item)
        return;
    model_->AddWatchItem(item);
    SetCurrentItemToPlaceholder();
}

void ExpressionWatchWidget::OnEditExpression()
{
    QModelIndex current_index = view_->currentIndex();
    if (!current_index.isValid() || current_index.parent().isValid() || model_->IsPlaceHolderIndex(current_index))
    {
        return;
    }
    auto index = model_->index(current_index.row(), 0, QModelIndex());
    view_->edit(index);
}

void ExpressionWatchWidget::OnDeleteExpression()
{
    QModelIndexList select_indexs = view_->selectionModel()->selectedRows();

    QVector<ValueItem *> items_to_remove;

    for (const QModelIndex &index : select_indexs)
    {
        if (!index.isValid() || model_->IsPlaceHolderIndex(index))
        {
            return;
        }

        ValueItem *item = model_->GetValueItemFromIndex(index);
        if (!item)
        {
            return;
        }

        items_to_remove.push_back(item);
    }

    for (auto item : items_to_remove)
    {
        model_->RemoveWatchItem(item);
        DeleteWatchItem(item);
    }
}

void ExpressionWatchWidget::OnSelectAllExpressions()
{
    view_->selectAll();
}

void ExpressionWatchWidget::OnClearAllExpressions()
{
    QVector<ValueItem *> items_to_remove;
    size_t root_count = model_->GetRootItemCount();
    for (size_t i = 0; i < root_count; ++i)
    {
        ValueItem *item = model_->GetRootItemAt(i);
        if (item)
        {
            items_to_remove.push_back(item);
        }
    }

    for (auto item : items_to_remove)
    {
        model_->RemoveWatchItem(item);
        DeleteWatchItem(item);
    }
}

} // namespace gui
} // namespace xequation