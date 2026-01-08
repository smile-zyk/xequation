#include "expression_watch_widget.h"

#include "core/equation_common.h"
#include "value_model/value_item.h"
#include "value_model/value_item_builder.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
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
        emit AddWatchItemRequested(new_expression);
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
    emit ReplaceWatchItemRequested(root_items_[row]->id(), new_expression);
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

void ExpressionWatchModel::RemoveWatchItem(const QUuid& id)
{
    int index = -1;
    for (int i = 0; i < root_items_.size(); ++i)
    {
        if (root_items_[i]->id() == id)
        {
            index = i;
            break;
        }
    }
    
    if (index < 0)
        return;

    beginRemoveRows(QModelIndex(), index, index);
    root_items_.removeAt(index);
    endRemoveRows();
}

void ExpressionWatchModel::ReplaceWatchItem(const QUuid& id, ValueItem *new_item)
{
    if (!new_item)
        return;

    int index = -1;
    for (int i = 0; i < root_items_.size(); ++i)
    {
        if (root_items_[i]->id() == id)
        {
            index = i;
            break;
        }
    }
    
    if (index < 0)
        return;

    beginRemoveRows(QModelIndex(), index, index);
    root_items_.removeAt(index);
    endRemoveRows();

    beginInsertRows(QModelIndex(), index, index);
    root_items_.insert(index, new_item);
    endInsertRows();
}

ExpressionWatchWidget::ExpressionWatchWidget(EquationCompletionModel *completion_model, QWidget *parent)
    : QWidget(parent), completion_model_(completion_model)
{
    SetupUI();
    SetupConnections();
}

void ExpressionWatchWidget::OnEquationRemoved(const std::string &equation_name)
{
    auto range = expression_item_equation_name_bimap_.right.equal_range(equation_name);
    std::vector<QUuid> ids_to_update;
    for (auto it = range.first; it != range.second; ++it)
    {
        const QUuid& id = it->get_left();
        ids_to_update.push_back(id);
    }
    for (const auto& id : ids_to_update)
    {
        auto it = expression_item_map_.find(id);
        if (it == expression_item_map_.end())
            continue;
        auto expression = it->second->name();
        // recreate the watch item
        auto new_item = CreateWatchItem(expression);
        if (new_item)
        {
            model_->ReplaceWatchItem(id, new_item);
            DeleteWatchItem(id);
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
        std::vector<QUuid> ids_to_update;
        for (auto it = range.first; it != range.second; ++it)
        {
            const QUuid& id = it->get_left();
            ids_to_update.push_back(id);
        }
        for (const auto& id : ids_to_update)
        {
            auto it = expression_item_map_.find(id);
            if (it == expression_item_map_.end())
                continue;
            auto expression = it->second->name();
            // recreate the watch item
            auto new_item = CreateWatchItem(expression);
            if (new_item)
            {
                model_->ReplaceWatchItem(id, new_item);
                DeleteWatchItem(id);
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

void ExpressionWatchWidget::OnEvalResultSubmitted(const QUuid& id, const InterpretResult &result)
{
    auto it = expression_item_map_.find(id);
    if (it == expression_item_map_.end())
        return;

    ValueItem::UniquePtr new_item;

    QString expression = it->second->name();

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
    auto new_item_id = new_item->id();
    // get old item dependencies
    auto range = expression_item_equation_name_bimap_.left.equal_range(id);
    std::vector<std::string> old_dependencies;
    for (auto dep_it = range.first; dep_it != range.second; ++dep_it)
    {
        old_dependencies.push_back(dep_it->get_right());
    }
    for (const auto &dep : old_dependencies)
    {
        expression_item_equation_name_bimap_.insert({new_item_id, dep});
    }
    // insert new item
    expression_item_map_.insert({new_item_id, std::move(new_item)});

    model_->ReplaceWatchItem(id, new_item_ptr);
    DeleteWatchItem(id);
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

    // setup completer delegate for first column
    completer_delegate_ = new CompletionLineEditDelegate(this);
    completer_delegate_->SetCompletionModel(completion_model_);
    view_->setItemDelegateForColumn(0, completer_delegate_);

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
    connect(model_, &ExpressionWatchModel::AddWatchItemRequested, this, &ExpressionWatchWidget::OnRequestAddWatchItem);

    connect(
        model_, &ExpressionWatchModel::RemoveWatchItemRequested, this, &ExpressionWatchWidget::OnRequestRemoveWatchItem
    );

    connect(
        model_, &ExpressionWatchModel::ReplaceWatchItemRequested, this, &ExpressionWatchWidget::OnRequestReplaceWatchItem
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
        auto item_id = item->id();
        for (const auto &dependency : dependencies)
        {
            expression_item_equation_name_bimap_.insert({item_id, dependency});
        }
        emit EvalResultAsyncRequested(item_id, expression);
    }
    auto item_ptr = item.get();
    expression_item_map_.insert({item->id(), std::move(item)});
    return item_ptr;
}

void ExpressionWatchWidget::DeleteWatchItem(const QUuid& id)
{
    auto range = expression_item_equation_name_bimap_.left.equal_range(id);
    expression_item_equation_name_bimap_.left.erase(range.first, range.second);

    expression_item_map_.erase(id);
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

void ExpressionWatchWidget::OnRequestRemoveWatchItem(const QUuid& id)
{
    model_->RemoveWatchItem(id);
    DeleteWatchItem(id);
}

void ExpressionWatchWidget::OnRequestReplaceWatchItem(const QUuid& id, const QString &new_expression)
{
    auto it = expression_item_map_.find(id);
    if (it == expression_item_map_.end())
    {
        return;
    }
    
    if (it->second->type() == "Calculating")
    {
        QMessageBox::warning(this, "Warning", "Cannot edit an expression that is still calculating.");
        return;
    }

    auto new_item = CreateWatchItem(new_expression);
    if (!new_item)
        return;
    model_->ReplaceWatchItem(id, new_item);
    DeleteWatchItem(id);
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

    QVector<QUuid> ids_to_remove;

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

        ids_to_remove.push_back(item->id());
    }

    for (const auto& id : ids_to_remove)
    {
        model_->RemoveWatchItem(id);
        DeleteWatchItem(id);
    }
}

void ExpressionWatchWidget::OnSelectAllExpressions()
{
    view_->selectAll();
}

void ExpressionWatchWidget::OnClearAllExpressions()
{
    QVector<QUuid> ids_to_remove;
    size_t root_count = model_->GetRootItemCount();
    for (size_t i = 0; i < root_count; ++i)
    {
        ValueItem *item = model_->GetRootItemAt(i);
        if (item)
        {
            ids_to_remove.push_back(item->id());
        }
    }

    for (const auto& id : ids_to_remove)
    {
        model_->RemoveWatchItem(id);
        DeleteWatchItem(id);
    }
}

} // namespace gui
} // namespace xequation