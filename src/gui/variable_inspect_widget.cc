#include "variable_inspect_widget.h"
#include "core/equation_common.h"
#include "value_model/value_item.h"
#include "value_model/value_item_builder.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QVBoxLayout>
#include <core/equation_manager.h>
#include <core/equation_signals_manager.h>

namespace xequation
{
namespace gui
{

VariableInspectWidget::VariableInspectWidget(QWidget *parent) : QWidget(parent), model_(nullptr), view_(nullptr)
{
    SetupUI();
    SetupConnections();
}

void VariableInspectWidget::SetupUI()
{
    setWindowTitle("Variable Inspector");
    setWindowFlags(
        Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint
    );
    setContextMenuPolicy(Qt::CustomContextMenu);
    setMinimumSize(800, 600);

    view_ = new ValueTreeView(this);
    model_ = new ValueTreeModel(view_);

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
    add_watch_action_ = new QAction("Add Watch", this);

    copy_action_->setShortcut(QKeySequence::Copy);
    add_watch_action_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_W));

    const auto shortcut_context = Qt::WidgetWithChildrenShortcut;
    copy_action_->setShortcutContext(shortcut_context);
    add_watch_action_->setShortcutContext(shortcut_context);

    view_->addActions({copy_action_, add_watch_action_});
    view_->installEventFilter(this);
}

void VariableInspectWidget::SetupConnections()
{
    connect(
        this, &VariableInspectWidget::customContextMenuRequested, this,
        &VariableInspectWidget::OnCustomContextMenuRequested
    );

    connect(copy_action_, &QAction::triggered, this, &VariableInspectWidget::OnCopyVariableValue);
    connect(add_watch_action_, &QAction::triggered, this, &VariableInspectWidget::OnAddVariableToWatch);
}

void VariableInspectWidget::OnCurrentEquationChanged(const Equation *equation)
{
    SetCurrentEquation(equation);
}

void VariableInspectWidget::SetCurrentEquation(const Equation *equation)
{
    if (current_equation_ == equation)
    {
        return;
    }
    current_equation_ = equation;
    model_->Clear();
    if (current_equation_)
    {
        QString name = QString::fromStdString(current_equation_->name());
        if (current_equation_->status() != ResultStatus::kSuccess)
        {
            current_variable_item_ = ValueItem::Create(
                name, QString::fromStdString(current_equation_->message()),
                QString::fromStdString(ResultStatusConverter::ToString((current_equation_->status())))
            );
        }
        else
        {
            current_variable_item_ = gui::BuilderUtils::CreateValueItem(name, current_equation_->GetValue());
        }
        model_->AddRootItem(current_variable_item_.get());
    }
}

void VariableInspectWidget::OnEquationRemoving(const Equation *equation)
{
    if (equation == current_equation_)
    {
        SetCurrentEquation(nullptr);
    }
}

void VariableInspectWidget::OnEquationUpdated(
    const Equation *equation, bitmask::bitmask<EquationUpdateFlag> change_type
)
{
    bool should_update = false;
    if (equation->status() != ResultStatus::kSuccess)
    {
        if (change_type & EquationUpdateFlag::kStatus || change_type & EquationUpdateFlag::kMessage)
        {
            should_update = true;
        }
    }

    if (equation->status() == ResultStatus::kSuccess)
    {
        if (change_type & EquationUpdateFlag::kValue)
        {
            should_update = true;
        }
    }

    if (equation == current_equation_ && should_update)
    {
        current_equation_ = nullptr;
        SetCurrentEquation(equation);
    }
}

bool VariableInspectWidget::eventFilter(QObject *obj, QEvent *event)
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

        if (handle_action_shortcut(copy_action_) || handle_action_shortcut(add_watch_action_))
        {
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void VariableInspectWidget::OnCustomContextMenuRequested(const QPoint &pos)
{
    static QMenu *menu = nullptr;

    if (!menu)
    {
        menu = new QMenu(this);

        menu->addAction(copy_action_);
        menu->addAction(add_watch_action_);
    }

    menu->exec(mapToGlobal(pos));
}

void VariableInspectWidget::OnCopyVariableValue()
{
    QModelIndexList select_indexs = view_->selectionModel()->selectedRows();

    QString combined_text;

    for (const QModelIndex &index : select_indexs)
    {
        if (!index.isValid())
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

void VariableInspectWidget::OnAddVariableToWatch()
{
    QModelIndexList select_indexs = view_->selectionModel()->selectedRows();

    for (const QModelIndex &index : select_indexs)
    {
        if (!index.isValid())
        {
            continue;
        }

        ValueItem *item = model_->GetValueItemFromIndex(index);
        if (!item)
        {
            continue;
        }

        QVector<ValueItem *> hierarchy_items;
        ValueItem *current = item;
        while (current->parent() != nullptr)
        {
            hierarchy_items.push_back(current);
            current = current->parent();
        }
        hierarchy_items.push_back(current);
        std::reverse(hierarchy_items.begin(), hierarchy_items.end());

        QString expression;
        for (int i = 0; i < hierarchy_items.size(); ++i)
        {
            if (i == 0)
            {
                expression += hierarchy_items[i]->name();
            }
            else if (hierarchy_items[i - 1]->value_item_type() == "python-dict")
            {
                expression += QString("[") + hierarchy_items[i]->name() + QString("]");
            }
            else if (hierarchy_items[i - 1]->value_item_type() == "python-class")
            {
                expression += QString(".") + hierarchy_items[i]->name();
            }
            else
            {
                expression += hierarchy_items[i]->name();
            }
        }

        emit AddExpressionToWatch(expression);
    }
}

} // namespace gui
} // namespace xequation