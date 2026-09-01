#include "proxy_demo_widget.h"

#include "expression_data_frame_tab_widget.h"
#include "expression_property_widget.h"

#include "xequation_proxy.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QVBoxLayout>

#include <vector>

namespace xresults
{
namespace gui
{

using namespace xequation;

ProxyDemoWidget::ProxyDemoWidget(QWidget *parent) : QWidget(parent)
{
    SetupUI();
    SetupConnections();
    RefreshEquationList();
}

ProxyDemoWidget::~ProxyDemoWidget() = default;

void ProxyDemoWidget::SetupUI()
{
    setWindowTitle("XEquation Proxy Demo");
    setMinimumSize(900, 600);

    // ---- top: statement input ------------------------------------------
    statement_edit_ = new QLineEdit(this);
    statement_edit_->setPlaceholderText(
        "Insert equation, e.g.  y = [1, 2, 3]  (name = expression)"
    );
    insert_button_ = new QPushButton("Insert", this);
    redefine_button_ = new QPushButton("Redefine", this);
    rename_button_ = new QPushButton("Rename", this);
    delete_button_ = new QPushButton("Delete", this);
    properties_button_ = new QPushButton("Properties", this);
    watch_button_ = new QPushButton("Watch", this);
    redefine_button_->setEnabled(false);  // requires a selected list item
    rename_button_->setEnabled(false);    // requires a selected list item
    delete_button_->setEnabled(false);    // requires a selected list item
    properties_button_->setEnabled(false); // requires a selected list item

    QHBoxLayout *input_layout = new QHBoxLayout();
    input_layout->addWidget(statement_edit_, 1);
    input_layout->addWidget(insert_button_);
    input_layout->addWidget(redefine_button_);
    input_layout->addWidget(rename_button_);
    input_layout->addWidget(delete_button_);
    input_layout->addWidget(properties_button_);
    input_layout->addWidget(watch_button_);

    // ---- middle: equation list + dataframe view + property window -----
    equation_list_ = new QListWidget(this);
    equation_list_->setSelectionMode(QAbstractItemView::ExtendedSelection);

    data_frame_view_ = new ExpressionDataFrameTabWidget(
        // Pass the REL manager directly (DataFrames come only from REL);
        // the tab widget only needs the manager's const query/parse/eval APIs.
        XEquationProxy::GetInstance().rel_manager(),
        this
    );
    property_widget_ = new ExpressionPropertyWidget(this);
    property_widget_->setMinimumHeight(180);

    // Right side: vertical splitter with the table on top, property window below.
    QSplitter *right_splitter = new QSplitter(Qt::Vertical, this);
    right_splitter->addWidget(data_frame_view_);
    right_splitter->addWidget(property_widget_);
    right_splitter->setStretchFactor(0, 3);
    right_splitter->setStretchFactor(1, 1);
    right_splitter->setChildrenCollapsible(false);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(equation_list_);
    splitter->addWidget(right_splitter);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setChildrenCollapsible(false);

    // ---- bottom: status label ------------------------------------------
    status_label_ = new QLabel("No equations yet. Insert one above.", this);
    status_label_->setWordWrap(true);

    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->addLayout(input_layout);
    main_layout->addWidget(splitter, 1);
    main_layout->addWidget(status_label_);
    setLayout(main_layout);
}

void ProxyDemoWidget::SetupConnections()
{
    connect(insert_button_, &QPushButton::clicked, this, &ProxyDemoWidget::OnInsertEquation);
    connect(
        statement_edit_, &QLineEdit::returnPressed, this, &ProxyDemoWidget::OnInsertEquation
    );
    connect(redefine_button_, &QPushButton::clicked, this, &ProxyDemoWidget::OnRedefineEquation);
    connect(rename_button_, &QPushButton::clicked, this, &ProxyDemoWidget::OnRenameEquation);
    connect(delete_button_, &QPushButton::clicked, this, &ProxyDemoWidget::OnDeleteEquation);
    connect(properties_button_, &QPushButton::clicked, this, &ProxyDemoWidget::OnShowProperties);
    connect(watch_button_, &QPushButton::clicked, this, &ProxyDemoWidget::OnAddWatchExpression);
    connect(
        equation_list_, &QListWidget::itemSelectionChanged, this,
        &ProxyDemoWidget::OnEquationListSelectionChanged
    );

    // The REL manager's signals are routed to the tab widget / property widget:
    // each decides which tabs to clear / re-evaluate (self-contained judgment;
    // this widget only dispatches).
    XEquationProxy &proxy = XEquationProxy::GetInstance();
    ExpressionDataFrameTabWidget *tabs = data_frame_view_;
    ExpressionPropertyWidget *property = property_widget_;

    // kEquationRemoving: value is about to disappear; equation tabs showing it
    // are cleared, property widget clears its selection.
    removing_rel_connection_ =
        proxy.rel_manager()
            .signals_manager()
            .ConnectScoped<EquationEvent::kEquationRemoving>(
                [tabs, property](const Equation *eq)
                {
                    tabs->OnEquationRemoving(eq);
                    property->OnEquationRemoving(eq);
                }
            );

    // kEquationRemoved: name is gone; expression tabs depending on it are
    // re-evaluated (they resolve to NameError and keep their tab).
    removed_rel_connection_ =
        proxy.rel_manager()
            .signals_manager()
            .ConnectScoped<EquationEvent::kEquationRemoved>(
                [tabs](const std::string &name) { tabs->OnEquationRemoved(name); }
            );

    // kEquationUpdated: on value-ready (kValue) each tab decides to refresh.
    updated_rel_connection_ =
        proxy.rel_manager()
            .signals_manager()
            .ConnectScoped<EquationEvent::kEquationUpdated>(
                [tabs, property](const Equation *eq, bitmask::bitmask<EquationUpdateFlag> flags)
                {
                    tabs->OnEquationUpdated(eq, flags);
                    property->OnEquationUpdated(eq, flags);
                }
            );

    // kEquationAdded: a previously-missing dependency now exists; expression
    // tabs that NameError'd on it are re-evaluated; a same-name equation tab
    // is refreshed too.
    added_rel_connection_ =
        proxy.rel_manager()
            .signals_manager()
            .ConnectScoped<EquationEvent::kEquationAdded>(
                [tabs](const Equation *eq) { tabs->OnEquationAdded(eq); }
            );
}

/// Get the equation's "row count" for the list suffix.  Only REL values have a
/// definite row count; other values return 0.
static qlonglong GetRowCount(const Equation *equation)
{
    if (!equation || equation->status() != ResultStatus::kSuccess)
    {
        return 0;
    }
    const EquationValue &value = equation->GetValue();
    if (value.IsRelValue())
    {
        return static_cast<qlonglong>(value.AsRel().rows());
    }
    return 0;
}

bool ProxyDemoWidget::SplitStatement(
    const QString &statement, QString *name, QString *expr
)
{
    if (name == nullptr || expr == nullptr)
    {
        return false;
    }

    const QString trimmed = statement.trimmed();
    if (trimmed.isEmpty())
    {
        return false;
    }

    const int eq_pos = trimmed.indexOf('=');
    if (eq_pos <= 0)
    {
        return false;
    }

    *name = trimmed.left(eq_pos).trimmed();
    *expr = trimmed.mid(eq_pos + 1).trimmed();

    if (name->isEmpty() || expr->isEmpty())
    {
        return false;
    }

    // Name must be a valid identifier (letters/digits/underscore, not starting
    // with a digit).
    for (int i = 0; i < name->size(); ++i)
    {
        const QChar ch = name->at(i);
        const bool is_identifier_char =
            ch.isLetterOrNumber() || ch == '_';
        const bool is_valid_start = (i > 0) || ch.isLetter() || ch == '_';
        if (!is_identifier_char || !is_valid_start)
        {
            return false;
        }
    }

    return true;
}

bool ProxyDemoWidget::IsValidIdentifier(const QString &name)
{
    if (name.isEmpty())
    {
        return false;
    }

    // Name must be a valid identifier (letters/digits/underscore, not starting
    // with a digit).
    for (int i = 0; i < name.size(); ++i)
    {
        const QChar ch = name.at(i);
        const bool is_identifier_char =
            ch.isLetterOrNumber() || ch == '_';
        const bool is_valid_start = (i > 0) || ch.isLetter() || ch == '_';
        if (!is_identifier_char || !is_valid_start)
        {
            return false;
        }
    }
    return true;
}

QString ProxyDemoWidget::ReplaceIdentifierToken(
    const QString &content, const QString &old_name, const QString &new_name
)
{
    if (content.isEmpty() || old_name.isEmpty() || new_name.isEmpty())
    {
        return content;
    }

    // Token-level (word-boundary) replace: only full identifiers, not words
    // inside string literals, nor old_name as a prefix of another identifier
    // (e.g. old_name="a" must not replace the "a" in "abc").
    const QString pattern = QString("\\b%1\\b").arg(QRegularExpression::escape(old_name));
    const QRegularExpression re(pattern);
    QString result = content;
    return result.replace(re, new_name);
}

void ProxyDemoWidget::OnInsertEquation()
{
    QString name;
    QString expr;
    if (!SplitStatement(statement_edit_->text(), &name, &expr))
    {
        QMessageBox::warning(
            this, "Invalid Statement",
            "Enter an equation as:  name = expression\n"
            "e.g.  y = [1, 2, 3]"
        );
        return;
    }

    const std::string name_std = name.toStdString();

    XEquationProxy &proxy = XEquationProxy::GetInstance();
    EquationManager &mgr = proxy.rel_manager();

    if (mgr.IsEquationExist(name_std))
    {
        QMessageBox::warning(
            this, "Duplicate Equation",
            "An equation with this name already exists: " + name
        );
        return;
    }

    try
    {
        mgr.AddEquation(name_std, expr.toStdString());
        mgr.Update();
    }
    catch (const EquationException &e)
    {
        QMessageBox::warning(this, "Add Equation Failed", e.what());
        return;
    }
    catch (const ParseException &e)
    {
        QMessageBox::warning(this, "Parse Failed", e.what());
        return;
    }
    catch (const DependencyCycleException &e)
    {
        QMessageBox::warning(this, "Dependency Cycle", e.what());
        return;
    }
    catch (const std::exception &e)
    {
        QMessageBox::warning(this, "Error", e.what());
        return;
    }

    statement_edit_->clear();
    RefreshEquationList();

    // Select the newly inserted equation to show its DataFrame immediately.
    const QList<QListWidgetItem *> items = equation_list_->findItems(
        name, Qt::MatchExactly
    );
    if (!items.isEmpty())
    {
        equation_list_->setCurrentItem(items.first());
    }
}

void ProxyDemoWidget::OnRedefineEquation()
{
    const QString current_name = CurrentSelectedEquationName();
    if (current_name.isEmpty())
    {
        return;
    }

    XEquationProxy &proxy = XEquationProxy::GetInstance();
    EquationManager &mgr = proxy.rel_manager();
    const Equation *equation = mgr.GetEquation(current_name.toStdString());
    if (!equation)
    {
        return;
    }

    bool ok = false;
    const QString new_content = QInputDialog::getText(
        this, "Redefine Equation",
        QString("New formula for  %1  = ").arg(current_name),
        QLineEdit::Normal, QString::fromStdString(equation->content()), &ok
    );
    if (!ok)
    {
        return;
    }

    const QString trimmed = new_content.trimmed();
    if (trimmed.isEmpty())
    {
        QMessageBox::warning(this, "Invalid Formula", "Formula must not be empty.");
        return;
    }

    const EquationGroupId group_id = equation->group_id();
    const std::string name_std = current_name.toStdString();

    try
    {
        // Redefine formula: EditSingleEquation rebuilds dependency edges and
        // cascades invalidation to dependents; Update() re-computes in topo order.
        mgr.EditSingleEquation(group_id, name_std, trimmed.toStdString());
        mgr.Update();
    }
    catch (const EquationException &e)
    {
        QMessageBox::warning(this, "Redefine Failed", e.what());
        return;
    }
    catch (const ParseException &e)
    {
        QMessageBox::warning(this, "Parse Failed", e.what());
        return;
    }
    catch (const DependencyCycleException &e)
    {
        QMessageBox::warning(this, "Dependency Cycle", e.what());
        return;
    }
    catch (const std::exception &e)
    {
        QMessageBox::warning(this, "Error", e.what());
        return;
    }

    RefreshEquationList();
    SelectEquationByName(current_name);
}

void ProxyDemoWidget::OnRenameEquation()
{
    const QString current_name = CurrentSelectedEquationName();
    if (current_name.isEmpty())
    {
        return;
    }

    bool ok = false;
    const QString new_name = QInputDialog::getText(
        this, "Rename Equation",
        QString("New name for  %1 :").arg(current_name),
        QLineEdit::Normal, current_name, &ok
    );
    if (!ok)
    {
        return;
    }

    const QString trimmed_new = new_name.trimmed();
    if (!IsValidIdentifier(trimmed_new))
    {
        QMessageBox::warning(
            this, "Invalid Name",
            "Name must be a valid identifier:\n"
            "letters / digits / underscore, not starting with a digit."
        );
        return;
    }

    if (trimmed_new == current_name)
    {
        return;  // unchanged
    }

    XEquationProxy &proxy = XEquationProxy::GetInstance();
    EquationManager &mgr = proxy.rel_manager();

    if (mgr.IsEquationExist(trimmed_new.toStdString()))
    {
        QMessageBox::warning(
            this, "Duplicate Equation",
            "An equation with this name already exists: " + trimmed_new
        );
        return;
    }

    const Equation *equation = mgr.GetEquation(current_name.toStdString());
    if (!equation)
    {
        return;
    }

    const EquationGroupId group_id = equation->group_id();

    try
    {
        // Rename = rewrite the whole group statement text (token-level replace
        // old->new) then pass to EditEquationGroup: it removes the old equation,
        // adds the new one, rewrites all dependents' references, and cascades
        // invalidation; Update() triggers the chained recomputation.
        const std::string new_statement = ReplaceIdentifierToken(
            QString::fromStdString(mgr.GetEquationGroup(group_id)->statement()),
            current_name, trimmed_new
        ).toStdString();

        mgr.EditEquationGroup(group_id, new_statement);
        mgr.Update();
    }
    catch (const EquationException &e)
    {
        QMessageBox::warning(this, "Rename Failed", e.what());
        return;
    }
    catch (const ParseException &e)
    {
        QMessageBox::warning(this, "Parse Failed", e.what());
        return;
    }
    catch (const DependencyCycleException &e)
    {
        QMessageBox::warning(this, "Dependency Cycle", e.what());
        return;
    }
    catch (const std::exception &e)
    {
        QMessageBox::warning(this, "Error", e.what());
        return;
    }

    RefreshEquationList();
    SelectEquationByName(trimmed_new);
}

void ProxyDemoWidget::OnDeleteEquation()
{
    const QString current_name = CurrentSelectedEquationName();
    if (current_name.isEmpty())
    {
        return;
    }

    const QString display_name = current_name;
    const auto answer = QMessageBox::question(
        this, "Delete Equation",
        QString("Delete equation  '%1' ?\n"
                "This will also invalidate equations depending on it.").arg(display_name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No
    );
    if (answer != QMessageBox::Yes)
    {
        return;
    }

    XEquationProxy &proxy = XEquationProxy::GetInstance();
    EquationManager &mgr = proxy.rel_manager();
    const std::string name_std = current_name.toStdString();

    try
    {
        // Remove a single Equation by name: locate the owning group (in the
        // demo each equation maps to an independent group) and remove it.
        const Equation *equation = mgr.GetEquation(name_std);
        if (!equation)
        {
            throw xequation::EquationException::EquationNotFound(name_std);
        }
        mgr.RemoveEquationGroup(equation->group_id());
        mgr.Update();
    }
    catch (const EquationException &e)
    {
        QMessageBox::warning(this, "Delete Failed", e.what());
        return;
    }
    catch (const std::exception &e)
    {
        QMessageBox::warning(this, "Error", e.what());
        return;
    }

    // Refresh the list after deletion.  kEquationRemoving/kEquationRemoved
    // already handled the tabs (equation tab cleared, dependent expression
    // tabs re-evaluated); no manual tab reset needed here.
    property_widget_->SetEquation(nullptr);
    RefreshEquationList();
}

void ProxyDemoWidget::OnShowProperties()
{
    // The property widget is resident below the table and refreshes with the
    // selected item; this button only brings focus to it.
    if (property_widget_)
    {
        property_widget_->setFocus();
        property_widget_->raise();
    }
}

void ProxyDemoWidget::OnAddWatchExpression()
{
    bool ok = false;
    const QString expression = QInputDialog::getText(
        this, "Add Expression Watch",
        "Expression (e.g.  y * 2 + 1):", QLineEdit::Normal,
        QString(), &ok
    );
    if (!ok)
    {
        return;
    }

    const std::string trimmed = expression.trimmed().toStdString();
    if (trimmed.empty())
    {
        QMessageBox::warning(this, "Invalid Expression", "Expression must not be empty.");
        return;
    }

    // Add a watch tab that is NOT bound to any equation; it re-evaluates
    // through the engine whenever its dependencies' values are ready.
    // (DataFrame views are REL-only; the tab widget uses the REL engine.)
    data_frame_view_->AddExpression(trimmed);
}

void ProxyDemoWidget::RefreshEquationList()
{
    equation_list_->blockSignals(true);
    equation_list_->clear();

    XEquationProxy &proxy = XEquationProxy::GetInstance();
    EquationManager &mgr = proxy.rel_manager();
    const std::vector<std::string> names = mgr.GetEquationNames();
    for (const std::string &name : names)
    {
        const Equation *equation = mgr.GetEquation(name);
        QString item_text = QString::fromStdString(name);
        if (equation && equation->status() == ResultStatus::kSuccess)
        {
            item_text += QString("  [%1 row(s)]")
                             .arg(static_cast<qlonglong>(
                                 GetRowCount(equation)
                             ));
        }
        equation_list_->addItem(item_text);
    }
    equation_list_->blockSignals(false);

    if (names.empty())
    {
        data_frame_view_->ClearAll();
        status_label_->setText("No equations yet. Insert one above.");
    }
}

void ProxyDemoWidget::OnEquationListSelectionChanged()
{
    const QList<QListWidgetItem *> items = equation_list_->selectedItems();

    // Buttons / property widget act on the *current* (focus) item only.
    QListWidgetItem *item = equation_list_->currentItem();
    if (!item && !items.isEmpty())
    {
        item = items.first();
    }
    if (!item)
    {
        // No selection at all: keep watch tabs as they are (only unpinned
        // equation tabs get closed by SyncSelection with an empty set).
        property_widget_->SetEquation(nullptr);
        redefine_button_->setEnabled(false);
        rename_button_->setEnabled(false);
        delete_button_->setEnabled(false);
        properties_button_->setEnabled(false);
    }
    else
    {
        redefine_button_->setEnabled(true);
        rename_button_->setEnabled(true);
        delete_button_->setEnabled(true);
        properties_button_->setEnabled(true);
    }

    // Collect all selected equation names (item text: "name  [N row(s)]").
    std::vector<std::string> selected_names;
    for (const QListWidgetItem *it : items)
    {
        selected_names.push_back(it->text().section(' ', 0, 0).toStdString());
    }

    // Sync the tab widget: open / close / refresh equation tabs to match the
    // current selection (pinned and expression tabs are never closed).
    // DataFrame views are REL-only, so the sync targets the REL engine.
    data_frame_view_->SyncSelection(selected_names);

    // Property widget follows the focus item.
    if (item)
    {
        const QString name = item->text().section(' ', 0, 0);
        XEquationProxy &proxy = XEquationProxy::GetInstance();
        property_widget_->SetEquation(
            proxy.rel_manager().GetEquation(name.toStdString())
        );
    }
}

QString ProxyDemoWidget::CurrentSelectedEquationName() const
{
    const QListWidgetItem *item = equation_list_->currentItem();
    if (!item)
    {
        return QString();
    }
    // Item text format: "name  [N row(s)]" -- take the part before the space as the name.
    return item->text().section(' ', 0, 0);
}

void ProxyDemoWidget::SelectEquationByName(const QString &name)
{
    const QList<QListWidgetItem *> items =
        equation_list_->findItems(name, Qt::MatchExactly);
    if (!items.isEmpty())
    {
        equation_list_->setCurrentItem(items.first());
    }
}

} // namespace gui
} // namespace xresults
