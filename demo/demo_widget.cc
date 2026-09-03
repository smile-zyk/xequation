#include "demo_widget.h"

#include "equation_manager_tree_view.h"
#include "expression_data_frame_tab_widget.h"
#include "expression_property_widget.h"
#include "tree_view_tag.h"  // UI-layer tag definitions

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
#include <QSignalBlocker>
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

DemoWidget::DemoWidget(QWidget *parent) : QWidget(parent)
{
    SetupUI();
    SetupConnections();
    RefreshDatasetCombo();
    RefreshEquationList();
}

DemoWidget::~DemoWidget() = default;

void DemoWidget::SetupUI()
{
    setWindowTitle("XEquation Demo");
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
    redefine_button_ = new QPushButton("Redefine", this);
    rename_button_ = new QPushButton("Rename", this);
    delete_button_ = new QPushButton("Delete", this);
    watch_button_ = new QPushButton("Watch", this);
    redefine_button_->setEnabled(false);  // requires a selected list item
    rename_button_->setEnabled(false);    // requires a selected list item
    delete_button_->setEnabled(false);    // requires a selected list item

    QHBoxLayout *input_layout = new QHBoxLayout();
    input_layout->addWidget(statement_edit_, 1);
    input_layout->addWidget(insert_button_);
    input_layout->addWidget(redefine_button_);
    input_layout->addWidget(rename_button_);
    input_layout->addWidget(delete_button_);
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

void DemoWidget::SetupConnections()
{
    connect(insert_button_, &QPushButton::clicked, this, &DemoWidget::OnInsertEquation);
    connect(
        statement_edit_, &QLineEdit::returnPressed, this, &DemoWidget::OnInsertEquation
    );
    connect(redefine_button_, &QPushButton::clicked, this, &DemoWidget::OnRedefineEquation);
    connect(rename_button_, &QPushButton::clicked, this, &DemoWidget::OnRenameEquation);
    connect(delete_button_, &QPushButton::clicked, this, &DemoWidget::OnDeleteEquation);
    connect(watch_button_, &QPushButton::clicked, this, &DemoWidget::OnAddWatchExpression);
    connect(
        open_env_button_, &QPushButton::clicked, this, &DemoWidget::OnOpenEnvJson
    );
    connect(
        dataset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &DemoWidget::OnDatasetSelectionChanged
    );
    connect(
        equation_list_, &QListWidget::itemSelectionChanged, this,
        &DemoWidget::OnEquationListSelectionChanged
    );
    // Clicking a row re-runs the list-driven sync even when the selection did
    // not change (e.g. re-clicking the already-selected row after the tree was
    // the last-clicked panel re-shows the list's equation tabs).
    connect(
        equation_list_, &QListWidget::itemClicked, this,
        [this](QListWidgetItem *) { OnEquationListSelectionChanged(); }
    );
    // Manager tree: selection change (keyboard + click) drives the property /
    // DataFrame panels.  Clicking also focuses (see OnManagerTreeClicked).
    connect(
        manager_tree_, &QTreeView::clicked, this, &DemoWidget::OnManagerTreeClicked
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

    // kExpressionRemoving: the expression was removed from the manager (tree
    // leaf right-click Delete, or the tree pruning access expressions after an
    // env reload).  Close its tab (even when pinned) and clear the property
    // widget if it displays that expression.
    expression_removing_rel_connection_ =
        EquationManager::GetInstance()
            .signals_manager()
            .ConnectScoped<EquationEvent::kExpressionRemoving>(
                [tabs, property](const Expression *expr)
                {
                    tabs->OnExpressionRemoving(expr);
                    property->OnExpressionRemoving(expr);
                }
            );
}

bool DemoWidget::SplitStatement(
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

bool DemoWidget::IsValidIdentifier(const QString &name)
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

void DemoWidget::OnInsertEquation()
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
        mgr.AddEquation(name_std, expr.toStdString(), kEquationTagDefault);
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

void DemoWidget::OnRedefineEquation()
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

void DemoWidget::OnRenameEquation()
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

void DemoWidget::OnDeleteEquation()
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

void DemoWidget::OnAddWatchExpression()
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
        expr_id = mgr.AddExpression(trimmed, kWatchTagDefault);
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

void DemoWidget::OnOpenEnvJson()
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

void DemoWidget::LoadEnvJson(const QString &path)
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

void DemoWidget::RefreshDatasetCombo()
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

void DemoWidget::OnDatasetSelectionChanged(int index)
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

void DemoWidget::RefreshEquationList()
{
    // Remember the current selection so a refresh (insert / rename / delete)
    // keeps the same equations selected and their tabs stable.
    QStringList selected_names;
    for (const QListWidgetItem *item : equation_list_->selectedItems())
    {
        selected_names.append(item->text());
    }
    const QString current_name = equation_list_->currentItem()
        ? equation_list_->currentItem()->text()
        : QString();

    equation_list_->blockSignals(true);
    equation_list_->clear();

    EquationManager &mgr = EquationManager::GetInstance();
    const std::vector<std::string> names = mgr.GetEquationNames();
    for (const std::string &name : names)
    {
        equation_list_->addItem(QString::fromStdString(name));
    }

    // Re-apply the previous selection (items that still exist).
    for (const QString &name : selected_names)
    {
        const QList<QListWidgetItem *> items =
            equation_list_->findItems(name, Qt::MatchExactly);
        if (!items.isEmpty())
        {
            items.first()->setSelected(true);
        }
    }
    QListWidgetItem *current_item = nullptr;
    if (!current_name.isEmpty())
    {
        const QList<QListWidgetItem *> items =
            equation_list_->findItems(current_name, Qt::MatchExactly);
        if (!items.isEmpty())
        {
            current_item = items.first();
        }
    }
    equation_list_->setCurrentItem(current_item);
    equation_list_->blockSignals(false);

    if (names.empty())
    {
        // Do NOT clear the tab widget here: watch-expression tabs must survive
        // even when the last equation is deleted.  Equation tabs are removed by
        // kEquationRemoving; expression tabs are independent.
        status_label_->setText("No equations yet. Insert one above.");
    }

    // Keep the equation tabs / property / tree in sync with the (possibly
    // changed) list selection.
    OnEquationListSelectionChanged();
}

void DemoWidget::OnEquationListSelectionChanged()
{
    // The equation LIST is the last-clicked panel (mirrors from the tree
    // block this widget's signals).  Per the rules:
    //   - property always shows the last-clicked equation row (rule 4);
    //   - the list selection mirrors into the tree (rule 2);
    //   - unpinned tabs = the list's selected equations (rule 3: tabs follow
    //     the last-clicked panel, and the list only holds equations).

    const QList<QListWidgetItem *> items = equation_list_->selectedItems();

    // Buttons / property widget act on the *current* (focus) item only.
    QListWidgetItem *item = equation_list_->currentItem();
    if (!item && !items.isEmpty())
    {
        item = items.first();
    }
    if (!item)
    {
        // No selection at all: nothing new was clicked, so the property panel
        // keeps its last-clicked content (deletion clears it explicitly).
        redefine_button_->setEnabled(false);
        rename_button_->setEnabled(false);
        delete_button_->setEnabled(false);
    }
    else
    {
        redefine_button_->setEnabled(true);
        rename_button_->setEnabled(true);
        delete_button_->setEnabled(true);
    }

    // Selected equation names (item text is the equation name).
    std::vector<QString> selected_qnames;
    selected_qnames.reserve(items.size());
    for (const QListWidgetItem *it : items)
    {
        selected_qnames.push_back(it->text());
    }

    // Rule 4: the property panel always shows the last-clicked object.
    if (item)
    {
        const QString name = item->text();
        const Equation *equation = EquationManager::GetInstance().GetEquation(name.toStdString());
        property_widget_->SetObject(equation ? equation->id : xequation::ObjectId());
    }

    // Rule 2: mirror the equation selection into the tree (equation leaves
    // only).  The tree's selection is blocked during the mirror so the tree
    // handler does not re-run.
    {
        QSignalBlocker blocker(manager_tree_->selectionModel());
        manager_tree_->SetEquationSelection(selected_qnames);
    }

    // Rule 3: unpinned tabs = the list's selected equations (ObjectIds).
    // Unpinned tree previews (data arrays / expressions) are dropped -- they
    // only appear while the tree is the last-clicked panel.  Pinned tabs
    // survive (SyncTabs keeps them regardless of the visible set).
    std::vector<ObjectId> visible_ids;
    visible_ids.reserve(items.size());
    for (const QListWidgetItem *it : items)
    {
        const Equation *equation =
            EquationManager::GetInstance().GetEquation(it->text().toStdString());
        if (equation)
        {
            visible_ids.push_back(equation->id);
        }
    }
    data_frame_view_->SyncTabs(visible_ids);
}

QString DemoWidget::CurrentSelectedEquationName() const
{
    const QListWidgetItem *item = equation_list_->currentItem();
    if (!item)
    {
        return QString();
    }
    // Item text is the equation name itself.
    return item->text();
}

void DemoWidget::SelectEquationByName(const QString &name)
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

void DemoWidget::OnManagerTreeClicked()
{
    // Selection changes are already routed through OnManagerTreeSelectionChanged
    // (the selectionModel's selectionChanged fires on click too).  Because the
    // list-driven mirror replaces the whole tree selection with equation
    // leaves, clicking any tree-only node always *changes* the selection, so
    // this slot only gives the tree focus for keyboard navigation.
    manager_tree_->setFocus();
}

void DemoWidget::OnManagerTreeSelectionChanged()
{
    using SelectionInfo = EquationManagerTreeView::SelectionInfo;
    using NodeKind = EquationManagerTreeView::NodeKind;

    // The manager TREE is the last-clicked panel (mirrors from the list block
    // this widget's signals).  Per the rules:
    //   - tree-selected equations mirror into the equation list (rule 1);
    //   - unpinned tabs = the tree's selected items (rule 3) -- equations,
    //     expressions and data arrays produce tabs; dataset / block / tag
    //     nodes produce none, so unpinned equation tabs close when only such
    //     nodes are selected (strict follow of the last-clicked panel);
    //   - property shows the last-clicked tree node (rule 4).

    // Selected nodes in tree order.  A programmatic Refresh() suppresses its
    // own selectionChanged, so an empty set here is a genuine user
    // deselection.
    const std::vector<SelectionInfo> infos = manager_tree_->SelectedInfos();
    if (infos.empty())
    {
        // User deselected everything in the tree: the tree contributes no
        // items.  Unpinned tabs (equations AND tree previews) close; pinned
        // tabs survive.
        data_frame_view_->SyncTabs({});
        return;
    }

    EquationManager &mgr = EquationManager::GetInstance();

    // The focus node decides the property panel: the tree's current item when
    // it is selected (keyboard navigation can move the current index without
    // selecting), otherwise the first selected node.
    SelectionInfo focus;
    const QModelIndex current_index = manager_tree_->currentIndex();
    if (current_index.isValid() &&
        manager_tree_->selectionModel()->isSelected(current_index))
    {
        focus = manager_tree_->CurrentSelection();
    }
    if (focus.kind == NodeKind::kGroupDatasets || focus.kind == NodeKind::kTag)
    {
        focus = infos.front();
    }
    const bool focus_is_equation = (focus.kind == NodeKind::kEquation);

    // ---- 1. equation leaves: mirror into the equation list (rule 1) ------
    // Only when the tree selection contains an equation is the list touched;
    // dataset / data-array / expression-only selections leave it as-is.
    std::vector<QString> eq_names;
    for (const SelectionInfo &info : infos)
    {
        if (info.kind == NodeKind::kEquation)
        {
            eq_names.push_back(info.name);
        }
    }
    if (!eq_names.empty())
    {
        equation_list_->blockSignals(true);
        equation_list_->clearSelection();
        QListWidgetItem *current_item = nullptr;
        for (const QString &name : eq_names)
        {
            const QList<QListWidgetItem *> items =
                equation_list_->findItems(name, Qt::MatchExactly);
            if (items.isEmpty())
            {
                continue;
            }
            QListWidgetItem *list_item = items.first();
            list_item->setSelected(true);
            if (!current_item || (focus_is_equation && name == focus.name))
            {
                current_item = list_item;
            }
        }
        equation_list_->setCurrentItem(current_item);
        equation_list_->blockSignals(false);
    }

    // Buttons act on the equation list's current item (mirrored above).
    const bool has_list_current = (equation_list_->currentItem() != nullptr);
    redefine_button_->setEnabled(has_list_current);
    rename_button_->setEnabled(has_list_current);
    delete_button_->setEnabled(has_list_current);

    // ---- 2. unpinned tabs = the tree's selected items (rule 3) ------------
    // Resolve every selected item to its ObjectId:
    //   - equation leaf -> equation id;
    //   - expression leaf -> expression id;
    //   - data array -> hidden access-expression id (lazily created once,
    //     cached on the node; reused on later clicks).
    std::vector<ObjectId> visible_ids;
    for (const SelectionInfo &info : infos)
    {
        if (info.kind == NodeKind::kEquation)
        {
            if (const Equation *equation = mgr.GetEquation(info.name.toStdString()))
            {
                visible_ids.push_back(equation->id);
            }
        }
        else if (info.kind == NodeKind::kExpression && !info.object_id.is_nil())
        {
            visible_ids.push_back(info.object_id);
        }
        else if (info.kind == NodeKind::kDataArray)
        {
            const ObjectId access_id = manager_tree_->EnsureDataArrayExpression(
                info.dataset, info.block_path, info.data_array);
            if (!access_id.is_nil())
            {
                visible_ids.push_back(access_id);
            }
        }
    }
    data_frame_view_->SyncTabs(visible_ids);

    // ---- 3. property panel: the last-clicked tree node (rule 4) -----------
    if (focus.kind == NodeKind::kEquation)
    {
        const Equation *equation = mgr.GetEquation(focus.name.toStdString());
        property_widget_->SetObject(equation ? equation->id
                                             : xequation::ObjectId());
        return;
    }

    if (focus.kind == NodeKind::kExpression)
    {
        if (!focus.object_id.is_nil() && mgr.GetExpression(focus.object_id))
        {
            property_widget_->SetObject(focus.object_id);
        }
        return;
    }

    // dataset / block / data array
    if (focus.kind == NodeKind::kDataset || focus.kind == NodeKind::kBlock)
    {
        // No ObjectId behind a Dataset / Block node: the property panel stays
        // empty until a manager object is selected.
        property_widget_->SetObject(xequation::ObjectId());
        return;
    }

    if (focus.kind == NodeKind::kDataArray)
    {
        // The property shows the access expression (its content is the
        // dataset path); the tab was opened in step 2 through SyncTabs.
        const ObjectId access_id = manager_tree_->EnsureDataArrayExpression(
            focus.dataset, focus.block_path, focus.data_array);
        if (!access_id.is_nil())
        {
            property_widget_->SetObject(access_id);
            return;
        }
        // Registration failed: nothing with an ObjectId to show.
        property_widget_->SetObject(xequation::ObjectId());
    }
}

} // namespace gui
} // namespace xresults
