#include "proxy_demo_widget.h"

#include "equation_manager_tree_view.h"
#include "expression_data_frame_tab_widget.h"
#include "expression_property_widget.h"

#include "core/equation_manager.h"

#include "environment.h"   // rel::Environment / rel::EnvironmentConfig

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QVBoxLayout>

#include <boost/uuid/string_generator.hpp>

#include <algorithm>
#include <vector>

namespace xresults
{
namespace gui
{

using namespace xequation;

namespace
{
// ---------------------------------------------------------------------
// Dataset (env.json) helpers
// ---------------------------------------------------------------------

// Absolute path of the bundled sample config <repo>/3rd/REL/case/test_env.json.
// The exe usually lives in <repo>/build/bin/<config>/, so walk up from the
// executable directory and also check the working directory.  Empty when no
// such file is found.
QString SampleEnvJsonPath()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 6; ++depth)
    {
        const QString candidate = dir.filePath("3rd/REL/case/test_env.json");
        if (QFileInfo::exists(candidate))
        {
            return QFileInfo(candidate).absoluteFilePath();
        }
        if (!dir.cdUp())
        {
            break;
        }
    }

    const QString cwd_candidate =
        QDir::current().filePath("3rd/REL/case/test_env.json");
    if (QFileInfo::exists(cwd_candidate))
    {
        return QFileInfo(cwd_candidate).absoluteFilePath();
    }
    return QString();
}
} // namespace

ProxyDemoWidget::ProxyDemoWidget(QWidget *parent) : QWidget(parent)
{
    SetupUI();
    SetupConnections();
    RefreshDatasetCombo();
    RefreshEquationList();
}

ProxyDemoWidget::~ProxyDemoWidget() = default;

void ProxyDemoWidget::SetupUI()
{
    setWindowTitle("XEquation Proxy Demo");
    setMinimumSize(900, 600);

    // ---- top: dataset row (env.json + current-dataset switch) ---------
    QLabel *dataset_label = new QLabel("Dataset:", this);
    dataset_combo_ = new QComboBox(this);
    dataset_combo_->setEnabled(false);  // enabled once datasets are loaded
    dataset_combo_->setToolTip(
        "Datasets loaded from an environment JSON.  Selecting one makes it "
        "the REL default dataset (bare DataArray names resolve against it)."
    );
    open_env_button_ = new QPushButton("Open env.json\u2026", this);
    open_env_button_->setToolTip(
        "Open an environment JSON listing datasets + the default dataset "
        "(sample: 3rd/REL/case/test_env.json)."
    );

    QHBoxLayout *dataset_layout = new QHBoxLayout();
    dataset_layout->addWidget(dataset_label);
    dataset_layout->addWidget(dataset_combo_, 1);
    dataset_layout->addWidget(open_env_button_);

    // ---- top: statement input ------------------------------------------
    statement_edit_ = new QLineEdit(this);
    statement_edit_->setPlaceholderText(
        "Insert equation, e.g.  y = [1, 2, 3]  (name = expression)"
    );
    insert_button_ = new QPushButton("Insert", this);
    equation_tag_combo_ = new QComboBox(this);
    equation_tag_combo_->addItems({QString::fromLatin1(kEquationTagDefault),
                                   QString::fromLatin1(kMarkerTagDefault)});
    equation_tag_combo_->setToolTip("Tag applied when Insert creates the equation.");
    redefine_button_ = new QPushButton("Redefine", this);
    rename_button_ = new QPushButton("Rename", this);
    delete_button_ = new QPushButton("Delete", this);
    properties_button_ = new QPushButton("Properties", this);
    watch_button_ = new QPushButton("Watch", this);
    expression_tag_combo_ = new QComboBox(this);
    expression_tag_combo_->addItems({QString::fromLatin1(kWatchTagDefault),
                                     QString::fromLatin1(kGraphTagDefault)});
    expression_tag_combo_->setToolTip("Tag applied when Watch registers the expression.");
    redefine_button_->setEnabled(false);  // requires a selected list item
    rename_button_->setEnabled(false);    // requires a selected list item
    delete_button_->setEnabled(false);    // requires a selected list item
    properties_button_->setEnabled(false); // requires a selected list item

    QHBoxLayout *input_layout = new QHBoxLayout();
    input_layout->addWidget(statement_edit_, 1);
    input_layout->addWidget(equation_tag_combo_);
    input_layout->addWidget(insert_button_);
    input_layout->addWidget(redefine_button_);
    input_layout->addWidget(rename_button_);
    input_layout->addWidget(delete_button_);
    input_layout->addWidget(properties_button_);
    input_layout->addWidget(expression_tag_combo_);
    input_layout->addWidget(watch_button_);

    // ---- middle: manager tree + equation list + dataframe/property -----
    EquationManager &mgr = EquationManager::GetInstance();

    manager_tree_ = new EquationManagerTreeView(mgr, this);
    manager_tree_->setMinimumWidth(220);

    equation_list_ = new QListWidget(this);
    equation_list_->setSelectionMode(QAbstractItemView::ExtendedSelection);

    data_frame_view_ = new ExpressionDataFrameTabWidget(
        // Pass the REL manager directly (DataFrames come only from REL); the
        // tab widget needs the manager's query/register APIs.
        mgr,
        this
    );
    property_widget_ = new ExpressionPropertyWidget(mgr, this);
    property_widget_->setMinimumHeight(180);

    // Right side: vertical splitter with the table on top, property window below.
    QSplitter *right_splitter = new QSplitter(Qt::Vertical, this);
    right_splitter->addWidget(data_frame_view_);
    right_splitter->addWidget(property_widget_);
    right_splitter->setStretchFactor(0, 3);
    right_splitter->setStretchFactor(1, 1);
    right_splitter->setChildrenCollapsible(false);

    // Far-left column: the live manager tree (datasets + tag-grouped items).
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(manager_tree_);
    splitter->addWidget(equation_list_);
    splitter->addWidget(right_splitter);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 3);
    splitter->setChildrenCollapsible(false);

    // ---- bottom: status label ------------------------------------------
    status_label_ = new QLabel("No equations yet. Insert one above.", this);
    status_label_->setWordWrap(true);

    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->addLayout(dataset_layout);
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
        open_env_button_, &QPushButton::clicked, this, &ProxyDemoWidget::OnOpenEnvJson
    );
    connect(
        dataset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &ProxyDemoWidget::OnDatasetSelectionChanged
    );
    connect(
        equation_list_, &QListWidget::itemSelectionChanged, this,
        &ProxyDemoWidget::OnEquationListSelectionChanged
    );
    // Manager tree: selection change (keyboard + click) drives the property /
    // DataFrame panels.  Clicking also focuses (see OnManagerTreeClicked).
    connect(
        manager_tree_, &QTreeView::clicked, this, &ProxyDemoWidget::OnManagerTreeClicked
    );
    connect(
        manager_tree_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
        [this]() { OnManagerTreeSelectionChanged(); }
    );

    // The REL manager's signals are routed to the tab widget / property widget:
    // each decides which tabs to clear / re-evaluate (self-contained judgment;
    // this widget only dispatches).
    ExpressionDataFrameTabWidget *tabs = data_frame_view_;
    ExpressionPropertyWidget *property = property_widget_;

    // kEquationRemoving: value is about to disappear; equation tabs showing it
    // are cleared, property widget clears its selection.
    removing_rel_connection_ =
        EquationManager::GetInstance()
            .signals_manager()
            .ConnectScoped<EquationEvent::kEquationRemoving>(
                [tabs, property](const Equation *eq)
                {
                    tabs->OnEquationRemoving(eq);
                    property->OnEquationRemoving(eq);
                }
            );

    // kEquationUpdated: on value-ready (kValue) each tab decides to refresh.
    updated_rel_connection_ =
        EquationManager::GetInstance()
            .signals_manager()
            .ConnectScoped<EquationEvent::kEquationUpdated>(
                [tabs, property](const Equation *eq, bitmask::bitmask<EquationUpdateFlag> flags)
                {
                    tabs->OnEquationUpdated(eq, flags);
                    property->OnEquationUpdated(eq, flags);
                }
            );

    // kExpressionUpdated: a registered watch expression has a fresh value;
    // refresh its tab and the property widget if it displays that expression.
    expression_updated_rel_connection_ =
        EquationManager::GetInstance()
            .signals_manager()
            .ConnectScoped<EquationEvent::kExpressionUpdated>(
                [tabs, property](const Expression *expr, bitmask::bitmask<ExpressionUpdateFlag> flags)
                {
                    tabs->OnExpressionUpdated(expr, flags);
                    property->OnExpressionUpdated(expr, flags);
                }
            );
}

/// Get the equation's "row count" for the list suffix.  Only REL values have a
/// definite row count; other values return 0.
static qlonglong GetRowCount(const Equation *equation)
{
    if (!equation || equation->status != ResultStatus::kSuccess)
    {
        return 0;
    }
    const EquationValue value = EquationManager::GetInstance().GetEquationValue(equation->name);
    if (value.HasValue())
    {
        return static_cast<qlonglong>(value.Value().rows());
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

    EquationManager &mgr = EquationManager::GetInstance();

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
        mgr.AddEquation(name_std, expr.toStdString(),
                        equation_tag_combo_->currentText().toStdString());
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

    EquationManager &mgr = EquationManager::GetInstance();
    const Equation *equation = mgr.GetEquation(current_name.toStdString());
    if (!equation)
    {
        return;
    }

    bool ok = false;
    const QString new_content = QInputDialog::getText(
        this, "Redefine Equation",
        QString("New formula for  %1  = ").arg(current_name),
        QLineEdit::Normal, QString::fromStdString(equation->content), &ok
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

    const ObjectId equation_id = equation->id;

    try
    {
        // Redefine formula: EditEquation rebuilds dependency edges and
        // cascades invalidation to dependents; Update() re-computes in topo order.
        mgr.EditEquation(equation_id, trimmed.toStdString());
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

    EquationManager &mgr = EquationManager::GetInstance();

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

    try
    {
        // Rename: EquationManager::RenameEquation keeps the same identity
        // (id) under the new name and cascades invalidation to dependents;
        // Update() triggers the chained recomputation.
        mgr.RenameEquation(equation->id, trimmed_new.toStdString());
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

    EquationManager &mgr = EquationManager::GetInstance();
    const std::string name_std = current_name.toStdString();

    try
    {
        // Remove the equation by name.
        const Equation *equation = mgr.GetEquation(name_std);
        if (!equation)
        {
            throw xequation::EquationException::EquationNotFound(name_std);
        }
        mgr.RemoveEquation(name_std);
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
    property_widget_->SetObject(xequation::ObjectId());
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

    // Add a watch tab that is NOT bound to any equation.  The host registers
    // the expression with the manager (AddExpression) to get its id, then
    // hands the id to the tab widget -- the tab widget itself never inspects
    // expression strings.  (DataFrame views are REL-only; the tab widget uses
    // the REL engine.)
    EquationManager &mgr = EquationManager::GetInstance();

    ObjectId expr_id;
    try
    {
        expr_id = mgr.AddExpression(trimmed,
                                    expression_tag_combo_->currentText().toStdString());
    }
    catch (const std::exception &e)
    {
        QMessageBox::warning(this, "Parse Failed", e.what());
        return;
    }
    if (expr_id.is_nil())
    {
        return;
    }
    data_frame_view_->AddExpression(expr_id);
}

// =========================================================================
// Dataset (env.json) support
// =========================================================================

void ProxyDemoWidget::OnOpenEnvJson()
{
    // Suggest the bundled sample (<repo>/3rd/REL/case/test_env.json) when it
    // can be located next to the executable / in the working directory.
    const QString sample = SampleEnvJsonPath();
    const QString start_dir = !sample.isEmpty()
        ? QFileInfo(sample).absolutePath()
        : QDir::currentPath();

    const QString path = QFileDialog::getOpenFileName(
        this, "Open Environment JSON", start_dir,
        "Environment JSON (*.json);;All Files (*)"
    );
    if (path.isEmpty())
    {
        return;
    }
    LoadEnvJson(path);
}

void ProxyDemoWidget::LoadEnvJson(const QString &path)
{
    try
    {
        // Parse/validate first: throws on JSON syntax / IO errors BEFORE any
        // global state is touched.
        const rel::EnvironmentConfig cfg =
            rel::EnvironmentConfig::Load(path.toStdString());

        // Drop the datasets of a previously loaded env so re-opening an
        // env.json REPLACES the active set (rel::Environment::LoadFromConfig
        // itself preserves existing registries).
        if (!env_json_path_.isEmpty())
        {
            for (const std::string &name : rel::Environment::DatasetNames())
            {
                rel::Environment::RemoveDataset(name);
            }
        }

        // REL owns the real load: dataset files (relative dataset paths are
        // resolved against the config file's directory), default dataset
        // ("default_dataset" or the first entry) and python_plugins
        // (BUILD_PYTHON=ON: loaded against the interpreter that main.cc
        // initialized).
        rel::Environment::LoadFromConfig(path.toStdString());

        env_json_path_ = path;
        RefreshDatasetCombo();
        manager_tree_->Refresh();   // dataset tree changed

        // Equations / watch expressions referencing dataset DataArrays (bare
        // names resolve against the default dataset) follow the new data.
        try
        {
            EquationManager::GetInstance().Update();
        }
        catch (const std::exception &e)
        {
            (void)e;  // per-node failures surface on the equation / expression
        }

        const std::vector<std::string> names = rel::Environment::DatasetNames();
        QString status = QString("Loaded %1: %2 dataset(s)")
                             .arg(QFileInfo(path).fileName())
                             .arg(static_cast<int>(names.size()));
        if (xdataset::Dataset *default_ds = rel::Environment::DefaultDataset())
        {
            status += QString(", default: %1")
                          .arg(QString::fromStdString(default_ds->name()));
        }
        if (!cfg.python_plugins.empty() && !rel::Environment::IsPythonAvailable())
        {
            status += "  (python_plugins ignored: no Python support in this build)";
        }
        status_label_->setText(status);
    }
    catch (const std::exception &e)
    {
        QMessageBox::warning(
            this, "Load Environment Failed",
            QString("Failed to load environment:\n%1\n\n%2").arg(path, e.what())
        );
    }
}

void ProxyDemoWidget::RefreshDatasetCombo()
{
    dataset_combo_->blockSignals(true);
    dataset_combo_->clear();

    std::vector<std::string> names = rel::Environment::DatasetNames();
    std::sort(names.begin(), names.end());
    for (const std::string &name : names)
    {
        dataset_combo_->addItem(QString::fromStdString(name));
    }

    // Select the REL default dataset (also covers the case where the combo is
    // refreshed without reloading: e.g. another widget changed the default).
    if (xdataset::Dataset *default_ds = rel::Environment::DefaultDataset())
    {
        const int index =
            dataset_combo_->findText(QString::fromStdString(default_ds->name()));
        if (index >= 0)
        {
            dataset_combo_->setCurrentIndex(index);
        }
    }
    dataset_combo_->setEnabled(dataset_combo_->count() > 0);
    dataset_combo_->blockSignals(false);
}

void ProxyDemoWidget::OnDatasetSelectionChanged(int index)
{
    if (index < 0)
    {
        return;
    }
    const QString name = dataset_combo_->itemText(index);
    if (name.isEmpty())
    {
        return;
    }

    // Everything listed in the combo is a live dataset; make it the REL
    // default (bare DataArray references resolve against it).
    rel::Environment::SetDefaultDataset(name.toStdString());

    // The "(default)" marker moved: refresh the dataset tree.
    manager_tree_->Refresh();

    // Recompute so equation / watch values follow the newly selected dataset.
    try
    {
        EquationManager::GetInstance().Update();
    }
    catch (const std::exception &e)
    {
        (void)e;  // per-node failures surface on the equation / expression
    }

    status_label_->setText(QString("Default dataset: %1").arg(name));
}

void ProxyDemoWidget::RefreshEquationList()
{
    equation_list_->blockSignals(true);
    equation_list_->clear();

    EquationManager &mgr = EquationManager::GetInstance();
    const std::vector<std::string> names = mgr.GetEquationNames();
    for (const std::string &name : names)
    {
        const Equation *equation = mgr.GetEquation(name);
        QString item_text = QString::fromStdString(name);
        if (equation && equation->status == ResultStatus::kSuccess)
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
        // Do NOT clear the tab widget here: watch-expression tabs must survive
        // even when the last equation is deleted.  Equation tabs are removed by
        // kEquationRemoving; expression tabs are independent.
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
        property_widget_->SetObject(xequation::ObjectId());
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

    // Property widget follows the focus item.  The object identity is the
    // equation's id (the same id the tab widget uses).
    if (item)
    {
        const QString name = item->text().section(' ', 0, 0);
        const Equation *equation = EquationManager::GetInstance().GetEquation(name.toStdString());
        property_widget_->SetObject(equation ? equation->id : xequation::ObjectId());
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

// =========================================================================
// Manager tree panel routing
// =========================================================================

void ProxyDemoWidget::OnManagerTreeClicked()
{
    // Selection already routes via OnManagerTreeSelectionChanged (currentChanged
    // fires on click too).  This slot exists only to give the tree focus so
    // keyboard navigation has a natural anchor.
    manager_tree_->setFocus();
}

void ProxyDemoWidget::OnManagerTreeSelectionChanged()
{
    using SelectionInfo = EquationManagerTreeView::SelectionInfo;
    const SelectionInfo info = manager_tree_->CurrentSelection();
    if (info.kind == EquationManagerTreeView::NodeKind::kGroupDatasets ||
        info.kind == EquationManagerTreeView::NodeKind::kTag)
    {
        return;  // group / tag node: nothing specific to inspect
    }

    EquationManager &mgr = EquationManager::GetInstance();

    // ---- equation leaf: reuse the equation-list flow ---------------------
    if (info.kind == EquationManagerTreeView::NodeKind::kEquation)
    {
        if (mgr.IsEquationExist(info.name.toStdString()))
        {
            const Equation *equation = mgr.GetEquation(info.name.toStdString());
            if (equation)
            {
                property_widget_->SetObject(equation->id);
                data_frame_view_->AddEquation(equation->id, /*auto_pin=*/false);
            }
            SelectEquationByName(info.name);
        }
        return;
    }

    // ---- expression leaf -------------------------------------------------
    if (info.kind == EquationManagerTreeView::NodeKind::kExpression)
    {
        try
        {
            boost::uuids::string_generator gen;
            const ObjectId id = gen(info.object_id.toStdString());
            if (!id.is_nil() && mgr.GetExpression(id))
            {
                property_widget_->SetObject(id);
                // Selection-driven display (not an explicit watch add): do not
                // auto-pin so the tab follows the current selection.
                data_frame_view_->AddExpression(id, /*auto_pin=*/false);
            }
        }
        catch (const std::exception &)
        {
            // Malformed id: nothing to route.
        }
        return;
    }

    // ---- dataset / block / data array ------------------------------------
    const std::string dataset_name = info.dataset.toStdString();
    xdataset::Dataset *dataset = rel::Environment::FindDataset(dataset_name);
    if (!dataset)
    {
        property_widget_->ShowInfo(info.dataset, {{tr("Dataset"), info.dataset}});
        return;
    }

    if (info.kind == EquationManagerTreeView::NodeKind::kDataset)
    {
        const std::string default_name =
            rel::Environment::DefaultDataset() ? rel::Environment::DefaultDataset()->name()
                                               : std::string();
        std::vector<std::pair<QString, QString>> rows;
        rows.push_back({tr("Name"), info.dataset});
        rows.push_back({tr("Default"), QString::fromStdString(dataset_name == default_name ? "yes" : "no")});
        rows.push_back({tr("Blocks"),
                        QString::number(static_cast<qlonglong>(dataset->block_count()))});
        property_widget_->ShowInfo(info.dataset, rows);
        return;
    }

    const std::string block_path = info.block_path.toStdString();
    if (info.kind == EquationManagerTreeView::NodeKind::kBlock)
    {
        try
        {
            const xdataset::Block &block = dataset->GetBlock(block_path);

            // One readable line per DataSeries: "name — Type, N rows[, Unit]".
            auto describe_series = [](const xdataset::DataSeries &series) -> QString {
                QString text = QString::fromLatin1(xdataset::DataTypeToString(series.data_type()));
                text += QStringLiteral(", %1 rows").arg(static_cast<qlonglong>(series.size()));
                if (series.unit().has_dimension())
                {
                    text += QStringLiteral(", %1")
                                .arg(QString::fromStdString(series.unit().to_string()));
                }
                return text;
            };
            auto describe_indep = [&](const xdataset::IndependentSpec &spec) -> QString {
                QString text = QString::fromStdString(spec.name);
                text += QStringLiteral(" \u2014 ") + describe_series(spec.data);
                if (spec.dimension.is_regular())
                {
                    text += QStringLiteral(", regular dim=%1")
                                .arg(static_cast<qlonglong>(spec.dimension.regular_size()));
                }
                else
                {
                    text += QStringLiteral(", ragged groups=%1")
                                .arg(static_cast<qlonglong>(spec.dimension.element_count()));
                }
                return text;
            };

            QStringList indep_lines;
            for (const std::string &name : block.independents())
            {
                indep_lines.push_back(describe_indep(block.independent_spec(name)));
            }
            QStringList dep_lines;
            for (const std::string &name : block.dependents())
            {
                const xdataset::DependentSpec &spec = block.dependent_spec(name);
                dep_lines.push_back(QString::fromStdString(spec.name) +
                                    QStringLiteral(" \u2014 ") +
                                    describe_series(spec.data));
            }

            std::vector<std::pair<QString, QString>> rows;
            rows.push_back({tr("Dataset"), info.dataset});
            rows.push_back({tr("Block path"), info.block_path});
            rows.push_back({tr("Independents (%1)")
                                .arg(static_cast<int>(indep_lines.size())),
                            indep_lines.join(QStringLiteral("\n"))});
            rows.push_back({tr("Dependents (%1)")
                                .arg(static_cast<int>(dep_lines.size())),
                            dep_lines.join(QStringLiteral("\n"))});
            property_widget_->ShowInfo(info.block_path, rows);
        }
        catch (const std::exception &)
        {
            std::vector<std::pair<QString, QString>> rows;
            rows.push_back({tr("Dataset"), info.dataset});
            rows.push_back({tr("Block path"), info.block_path});
            property_widget_->ShowInfo(info.block_path, rows);
        }
        return;
    }

    if (info.kind == EquationManagerTreeView::NodeKind::kDataArray)
    {
        const std::string array_name = info.data_array.toStdString();
        try
        {
            const xdataset::DataArray &array =
                dataset->GetDataArray(block_path, array_name);
            // Like a selected Equation: show the full rel::Value details in the
            // property widget (name carries the dataset + block context).
            const EquationValue value{rel::Value(array)};
            property_widget_->ShowRelValue(
                QStringLiteral("%1.%2")
                    .arg(info.block_path, info.data_array),
                value
            );

            // Show the array's DataFrame in the shared TabWidget preview.
            data_frame_view_->ShowValue(
                QStringLiteral("%1.%2")
                    .arg(info.block_path, info.data_array),
                value
            );
        }
        catch (const std::exception &)
        {
            // Missing / unreadable array: fall back to basic info.
            std::vector<std::pair<QString, QString>> rows;
            rows.push_back({tr("Data array"), info.data_array});
            rows.push_back({tr("Dataset"), info.dataset});
            rows.push_back({tr("Block path"), info.block_path});
            property_widget_->ShowInfo(info.data_array, rows);
        }
    }
}

} // namespace gui
} // namespace xresults
