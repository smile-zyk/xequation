#include "proxy_demo_widget.h"

#include "equation_data_frame_model.h"
#include "equation_data_frame_view.h"
#include "equation_property_widget.h"

#include "xequation_proxy.h"
#include "core/equation_group.h"

#include <QComboBox>
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

    // ---- top: engine switch + statement input --------------------------
    engine_combo_ = new QComboBox(this);
    // Keep order consistent with CurrentEngineIndex(): 0=Python, 1=REL.
    engine_combo_->addItem("Python");
    engine_combo_->addItem("REL");
    QLabel *engine_label = new QLabel("Engine:", this);

    statement_edit_ = new QLineEdit(this);
    statement_edit_->setPlaceholderText(
        "Insert equation, e.g.  y = [1, 2, 3]  (name = expression)"
    );
    insert_button_ = new QPushButton("Insert", this);
    redefine_button_ = new QPushButton("Redefine", this);
    rename_button_ = new QPushButton("Rename", this);
    delete_button_ = new QPushButton("Delete", this);
    properties_button_ = new QPushButton("Properties", this);
    redefine_button_->setEnabled(false);  // requires a selected list item
    rename_button_->setEnabled(false);    // requires a selected list item
    delete_button_->setEnabled(false);    // requires a selected list item
    properties_button_->setEnabled(false); // requires a selected list item

    QHBoxLayout *input_layout = new QHBoxLayout();
    input_layout->addWidget(engine_label);
    input_layout->addWidget(engine_combo_);
    input_layout->addWidget(statement_edit_, 1);
    input_layout->addWidget(insert_button_);
    input_layout->addWidget(redefine_button_);
    input_layout->addWidget(rename_button_);
    input_layout->addWidget(delete_button_);
    input_layout->addWidget(properties_button_);

    // ---- middle: equation list + dataframe view + property window -----
    equation_list_ = new QListWidget(this);
    equation_list_->setSelectionMode(QAbstractItemView::SingleSelection);

    data_frame_view_ = new EquationDataFrameView(this);
    property_widget_ = new EquationPropertyWidget(this);
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
    connect(
        engine_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ProxyDemoWidget::OnEngineChanged
    );
    connect(insert_button_, &QPushButton::clicked, this, &ProxyDemoWidget::OnInsertEquation);
    connect(
        statement_edit_, &QLineEdit::returnPressed, this, &ProxyDemoWidget::OnInsertEquation
    );
    connect(redefine_button_, &QPushButton::clicked, this, &ProxyDemoWidget::OnRedefineEquation);
    connect(rename_button_, &QPushButton::clicked, this, &ProxyDemoWidget::OnRenameEquation);
    connect(delete_button_, &QPushButton::clicked, this, &ProxyDemoWidget::OnDeleteEquation);
    connect(properties_button_, &QPushButton::clicked, this, &ProxyDemoWidget::OnShowProperties);
    connect(
        equation_list_, &QListWidget::itemSelectionChanged, this,
        &ProxyDemoWidget::OnEquationListSelectionChanged
    );

    // Both engines' managers' removal signals are routed to the table model:
    // on deletion, if the model currently displays the removed equation it is
    // cleared automatically (the model does not manage connections).
    XEquationProxy &proxy = XEquationProxy::GetInstance();
    EquationDataFrameModel *model = data_frame_view_->table_model();
    EquationPropertyWidget *property = property_widget_;

    // kEquationRemoving: on deletion, model / property each decide whether it
    // is the currently displayed equation and clear (self-contained judgment;
    // widget only dispatches).
    removing_python_connection_ =
        proxy.manager(Engine::kPython)
            .signals_manager()
            .ConnectScoped<EquationEvent::kEquationRemoving>(
                [model, property](const Equation *eq)
                {
                    model->OnEquationRemoving(eq);
                    property->OnEquationRemoving(eq);
                }
            );
    removing_rel_connection_ =
        proxy.manager(Engine::kRel)
            .signals_manager()
            .ConnectScoped<EquationEvent::kEquationRemoving>(
                [model, property](const Equation *eq)
                {
                    model->OnEquationRemoving(eq);
                    property->OnEquationRemoving(eq);
                }
            );

    // kEquationUpdated: on equation update, likewise route to model / property,
    // each deciding to refresh.
    updated_python_connection_ =
        proxy.manager(Engine::kPython)
            .signals_manager()
            .ConnectScoped<EquationEvent::kEquationUpdated>(
                [model, property](const Equation *eq, bitmask::bitmask<EquationUpdateFlag> flags)
                {
                    model->OnEquationUpdated(eq, flags);
                    property->OnEquationUpdated(eq, flags);
                }
            );
    updated_rel_connection_ =
        proxy.manager(Engine::kRel)
            .signals_manager()
            .ConnectScoped<EquationEvent::kEquationUpdated>(
                [model, property](const Equation *eq, bitmask::bitmask<EquationUpdateFlag> flags)
                {
                    model->OnEquationUpdated(eq, flags);
                    property->OnEquationUpdated(eq, flags);
                }
            );
}

/// Map the combo index to XEquationProxy's Engine.
static Engine EngineFromCombo(int index)
{
    return (index == 1) ? Engine::kRel : Engine::kPython;
}

/// Get the equation's "row count" for the list suffix.  Only REL values have a
/// definite row count; Python values return 0.
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

int ProxyDemoWidget::CurrentEngineIndex() const
{
    return engine_combo_->currentIndex();
}

void ProxyDemoWidget::OnEngineChanged(int /*index*/)
{
    // Engine switch: clear the list and table, then re-fetch equations for the new engine.
    equation_list_->clear();
    data_frame_view_->Clear();
    status_label_->setText("Switched engine. Insert an equation above.");
    RefreshEquationList();
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
    const Engine engine = EngineFromCombo(CurrentEngineIndex());

    if (proxy.IsEquationExist(engine, name_std))
    {
        QMessageBox::warning(
            this, "Duplicate Equation",
            "An equation with this name already exists: " + name
        );
        return;
    }

    try
    {
        proxy.AddEquation(engine, name_std, expr.toStdString());
        proxy.Update(engine);
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
    const Engine engine = EngineFromCombo(CurrentEngineIndex());
    const Equation *equation = proxy.GetEquation(engine, current_name.toStdString());
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
        proxy.EditSingleEquation(engine, group_id, name_std, trimmed.toStdString());
        proxy.Update(engine);
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
    const Engine engine = EngineFromCombo(CurrentEngineIndex());

    if (proxy.IsEquationExist(engine, trimmed_new.toStdString()))
    {
        QMessageBox::warning(
            this, "Duplicate Equation",
            "An equation with this name already exists: " + trimmed_new
        );
        return;
    }

    const Equation *equation = proxy.GetEquation(engine, current_name.toStdString());
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
            QString::fromStdString(proxy.GetEquationGroup(engine, group_id)->statement()),
            current_name, trimmed_new
        ).toStdString();

        proxy.EditEquationGroup(engine, group_id, new_statement);
        proxy.Update(engine);
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
    const Engine engine = EngineFromCombo(CurrentEngineIndex());
    const std::string name_std = current_name.toStdString();

    try
    {
        proxy.RemoveEquation(engine, name_std);
        proxy.Update(engine);
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

    // Refresh the list after deletion; kEquationRemoving clears the table, and
    // this is a fallback.
    data_frame_view_->Clear();
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

void ProxyDemoWidget::RefreshEquationList()
{
    equation_list_->blockSignals(true);
    equation_list_->clear();

    XEquationProxy &proxy = XEquationProxy::GetInstance();
    const Engine engine = EngineFromCombo(CurrentEngineIndex());
    const std::vector<std::string> names = proxy.GetEquationNames(engine);
    for (const std::string &name : names)
    {
        const Equation *equation = proxy.GetEquation(engine, name);
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
        data_frame_view_->Clear();
        status_label_->setText("No equations yet. Insert one above.");
    }
}

void ProxyDemoWidget::OnEquationListSelectionChanged()
{
    const QListWidgetItem *item = equation_list_->currentItem();
    if (!item)
    {
        data_frame_view_->Clear();
        property_widget_->SetEquation(nullptr);
        redefine_button_->setEnabled(false);
        rename_button_->setEnabled(false);
        delete_button_->setEnabled(false);
        properties_button_->setEnabled(false);
        return;
    }

    // Item text format: "name  [N row(s)]" -- take the part before the space as the name.
    const QString text = item->text();
    const QString name = text.section(' ', 0, 0);
    redefine_button_->setEnabled(true);
    rename_button_->setEnabled(true);
    delete_button_->setEnabled(true);
    properties_button_->setEnabled(true);
    ShowEquation(name);
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

void ProxyDemoWidget::ShowEquation(const QString &equation_name)
{
    XEquationProxy &proxy = XEquationProxy::GetInstance();
    const Engine engine = EngineFromCombo(CurrentEngineIndex());
    const std::string name_std = equation_name.toStdString();
    const Equation *equation = proxy.GetEquation(engine, name_std);

    // Refresh the property widget (shows placeholder if equation is null; even
    // on compute failure, shows meta info + red Message).
    property_widget_->SetEquation(equation);

    if (!equation)
    {
        data_frame_view_->Clear();
        status_label_->setText("Equation not found: " + equation_name);
        return;
    }

    if (equation->status() != ResultStatus::kSuccess)
    {
        data_frame_view_->Clear();
        status_label_->setText(
            QString("%1: %2")
                .arg(equation_name)
                .arg(QString::fromStdString(equation->message()))
        );
        return;
    }

    const EquationValue &value = equation->GetValue();
    data_frame_view_->SetEquation(equation);

    if (value.IsRelValue())
    {
        const rel::Value &rel_value = value.AsRel();
        status_label_->setText(
            QString("%1  |  engine=REL  |  type=%2  |  shape=[%3]  |  rows=%4")
                .arg(equation_name)
                .arg(QString::fromStdString(
                    xdataset::DataTypeToString(rel_value.data_type())
                ))
                .arg(QString::fromStdString(
                    rel_value.data_shape().to_string()
                ))
                .arg(static_cast<qlonglong>(rel_value.rows()))
        );
    }
    else if (value.IsPyObject())
    {
        status_label_->setText(
            QString("%1  |  engine=Python  |  type=%2")
                .arg(equation_name)
                .arg(QString::fromStdString(value.AsPyObject().TypeName()))
        );
    }
    else
    {
        status_label_->setText(
            QString("%1: only REL / Python values are supported")
                .arg(equation_name)
        );
    }
}

} // namespace gui
} // namespace xresults
