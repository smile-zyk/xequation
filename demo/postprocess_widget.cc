#include "postprocess_widget.h"

#include "explorer_view.h"
#include "data_frame_tab_widget.h"
#include "property_widget.h"
#include "tree_view_tag.h"  // UI-layer tag definitions

#include "core/equation_manager.h"

#include "environment.h"   // rel::Environment / rel::EnvironmentConfig
#include "dataset.h"       // xdataset::Dataset
#include "dataset_io.h"    // xdataset::DatasetIO::Load

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
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
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace xresults
{
namespace gui
{

using namespace xequation;

namespace
{
// ---------------------------------------------------------------------
// Project file helpers
// ---------------------------------------------------------------------

// Absolute path of the bundled sample project <repo>/demo/demo_project.json.
// The exe usually lives in <repo>/build/bin/<config>/, so walk up from the
// executable directory and also check the working directory.  Empty when no
// such file is found.
QString SampleProjectPath()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 6; ++depth)
    {
        const QString candidate = dir.filePath("demo/demo_project.json");
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
        QDir::current().filePath("demo/demo_project.json");
    if (QFileInfo::exists(cwd_candidate))
    {
        return QFileInfo(cwd_candidate).absoluteFilePath();
    }
    return QString();
}
} // namespace

PostprocessWidget::PostprocessWidget(QWidget *parent) : QWidget(parent)
{
    SetupUI();
    SetupConnections();
    RefreshDatasetCombo();
    RefreshEquationList();
}

PostprocessWidget::~PostprocessWidget() = default;

void PostprocessWidget::SetupUI()
{
    setWindowTitle("XEquation Demo");
    setMinimumSize(900, 600);

    // ---- top: dataset row (project file + current-dataset switch) -----
    QLabel *dataset_label = new QLabel("Dataset:", this);
    dataset_combo_ = new QComboBox(this);
    dataset_combo_->setEnabled(false);  // enabled once datasets are loaded
    dataset_combo_->setToolTip(
        "Datasets loaded from a project file.  Selecting one makes it "
        "the REL default dataset (bare DataArray names resolve against it)."
    );
    open_env_button_ = new QPushButton("Open Project\u2026", this);
    open_env_button_->setToolTip(
        "Open a project file (datasets + equations + expressions)\n"
        "(sample: demo/demo_project.json)."
    );
    save_project_button_ = new QPushButton("Save Project\u2026", this);
    save_project_button_->setToolTip(
        "Save the current equations / expressions / dataset references\n"
        "to a project file."
    );

    QHBoxLayout *dataset_layout = new QHBoxLayout();
    dataset_layout->addWidget(dataset_label);
    dataset_layout->addWidget(dataset_combo_, 1);
    dataset_layout->addWidget(open_env_button_);
    dataset_layout->addWidget(save_project_button_);

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

    manager_tree_ = new ExplorerView(mgr, this);
    manager_tree_->setMinimumWidth(220);

    equation_list_ = new QListWidget(this);
    equation_list_->setSelectionMode(QAbstractItemView::ExtendedSelection);

    data_frame_view_ = new DataFrameTabWidget(
        // Pass the REL manager directly (DataFrames come only from REL); the
        // tab widget needs the manager's query/register APIs.
        mgr,
        this
    );
    property_widget_ = new PropertyWidget(mgr, this);
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

void PostprocessWidget::SetupConnections()
{
    connect(insert_button_, &QPushButton::clicked, this, &PostprocessWidget::OnInsertEquation);
    connect(
        statement_edit_, &QLineEdit::returnPressed, this, &PostprocessWidget::OnInsertEquation
    );
    connect(redefine_button_, &QPushButton::clicked, this, &PostprocessWidget::OnRedefineEquation);
    connect(rename_button_, &QPushButton::clicked, this, &PostprocessWidget::OnRenameEquation);
    connect(delete_button_, &QPushButton::clicked, this, &PostprocessWidget::OnDeleteEquation);
    connect(watch_button_, &QPushButton::clicked, this, &PostprocessWidget::OnAddWatchExpression);
    connect(
        open_env_button_, &QPushButton::clicked, this, &PostprocessWidget::OnOpenProject
    );
    connect(
        save_project_button_, &QPushButton::clicked, this, &PostprocessWidget::OnSaveProject
    );
    connect(
        dataset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &PostprocessWidget::OnDatasetSelectionChanged
    );
    connect(
        equation_list_, &QListWidget::itemSelectionChanged, this,
        &PostprocessWidget::OnEquationListSelectionChanged
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
        manager_tree_, &QTreeView::clicked, this, &PostprocessWidget::OnManagerTreeClicked
    );
    connect(
        manager_tree_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
        [this]() { OnManagerTreeSelectionChanged(); }
    );

    // kEquationRemoved: an equation left the manager (Delete button, or the
    // manager-tree context menu which calls RemoveEquation directly).  The
    // equation list (middle-left) must be refreshed -- the Delete-button path
    // did this manually, but the tree path bypasses it.  Connecting here keeps
    // the list in sync for every removal source.  The Property / DataFrame tab
    // widgets now subscribe directly to the manager themselves.
    equation_removed_rel_connection_ =
        EquationManager::GetInstance()
            .signals_manager()
            .ConnectScoped<EquationEvent::kEquationRemoved>(
                [this](const std::string &equation_name)
                {
                    Q_UNUSED(equation_name);
                    RefreshEquationList();
                }
            );
}

bool PostprocessWidget::SplitStatement(
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

bool PostprocessWidget::IsValidIdentifier(const QString &name)
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

void PostprocessWidget::OnInsertEquation()
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

    // ȫ��Ԥ�죺identifier �﷨ + REL ������ + ����������ͨ����
    // AddEquation �����������������쳣��parse / cycle �Իᴴ�����̲���
    // ��ɫ����״̬��ʾ�����û��͵��޸�����
    if (!mgr.IsValidEquationName(name_std))
    {
        if (mgr.IsEquationExist(name_std))
        {
            QMessageBox::warning(
                this, "Duplicate Equation",
                "An equation with this name already exists: " + name
            );
            return;
        }
        // A REL builtin (constant like "pi", function like "sin") can never
        // be bound in the environment -- reject it here with a clear message
        // instead of creating an equation that fails on every Update().
        if (EquationManager::IsReservedName(name_std))
        {
            QMessageBox::warning(
                this, "Reserved Name",
                "'" + name + "' is a REL builtin (constant or function) and "
                "cannot be used as an equation name."
            );
            return;
        }
        QMessageBox::warning(
            this, "Invalid Name",
            "Name must be a valid identifier:\n"
            "letters / digits / underscore, not starting with a digit."
        );
        return;
    }

    // AddEquation always creates the equation.  A syntax error / dependency
    // cycle no longer throws: the equation is created but stays out of the
    // dependency graph (status kError + message).  Nothing is reported here --
    // the broken equation shows up red in the list (with the reason as its
    // tooltip) and in the property panel, so the user can fix it in place.
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

void PostprocessWidget::OnRedefineEquation()
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

    // EditEquation keeps the new formula even when it is broken: the equation
    // is detached from the graph (status kError) instead of throwing.  Nothing
    // is reported -- the list entry turns red and the property panel carries
    // the reason.
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
    catch (const std::exception &e)
    {
        QMessageBox::warning(this, "Error", e.what());
        return;
    }

    RefreshEquationList();
    SelectEquationByName(current_name);
}

void PostprocessWidget::OnRenameEquation()
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

    // ȫ��Ԥ�죺identifier �﷨ + REL ������ + ����������δ�����������
    // ������ǰ return����������������RenameEquation ��󲻻�����������
    // ���쳣��
    if (!mgr.IsValidEquationName(trimmed_new.toStdString()))
    {
        if (mgr.IsEquationExist(trimmed_new.toStdString()))
        {
            QMessageBox::warning(
                this, "Duplicate Equation",
                "An equation with this name already exists: " + trimmed_new
            );
            return;
        }
        // Same reserved-name guard as Insert: a builtin name can never be
        // bound.
        if (EquationManager::IsReservedName(trimmed_new.toStdString()))
        {
            QMessageBox::warning(
                this, "Reserved Name",
                "'" + trimmed_new + "' is a REL builtin (constant or function) and "
                "cannot be used as an equation name."
            );
            return;
        }
        // ���ף�ǰ��� IsValidIdentifier �ļ���Ѹ��Ǵ󲿷����Σ���
        QMessageBox::warning(
            this, "Invalid Name",
            "Name must be a valid identifier:\n"
            "letters / digits / underscore, not starting with a digit."
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
        // Update() triggers the chained recomputation.  A cycle keeps the new
        // name and detaches the equation instead of throwing (no report --
        // the list entry turns red).
        mgr.RenameEquation(equation->id, trimmed_new.toStdString());
        mgr.Update();
    }
    catch (const EquationException &e)
    {
        QMessageBox::warning(this, "Rename Failed", e.what());
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

void PostprocessWidget::OnDeleteEquation()
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

void PostprocessWidget::OnAddWatchExpression()
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

    // Add a watch tab that is NOT bound to any equation.  AddExpression()
    // registers the expression with the manager and opens its tab; a syntax
    // error / cycle only keeps it off the dependency graph (status kError), so
    // the tab still opens and shows the reason.
    try
    {
        AddExpression(expression, QString());
    }
    catch (const std::exception &e)
    {
        QMessageBox::warning(this, "Add Expression Failed", e.what());
    }
}

// =========================================================================
// Project file support
// =========================================================================

void PostprocessWidget::OnOpenProject()
{
    // Suggest the bundled sample (<repo>/demo/demo_project.json) when it can
    // be located next to the executable / in the working directory.
    const QString sample = SampleProjectPath();
    const QString start_dir = !sample.isEmpty()
        ? QFileInfo(sample).absolutePath()
        : QDir::currentPath();

    const QString path = QFileDialog::getOpenFileName(
        this, "Open Project", start_dir,
        "Project (*.json);;All Files (*)"
    );
    if (path.isEmpty())
    {
        return;
    }
    LoadProject(path);
}

void PostprocessWidget::OnSaveProject()
{
    // Default to the currently loaded project (if any), else the sample dir.
    QString start_dir = !project_path_.isEmpty()
        ? QFileInfo(project_path_).absolutePath()
        : QDir::currentPath();

    const QString path = QFileDialog::getSaveFileName(
        this, "Save Project", start_dir,
        "Project (*.json);;All Files (*)"
    );
    if (path.isEmpty())
    {
        return;
    }

    try
    {
        EquationManager::GetInstance().SaveToFile(path.toStdString());
        project_path_ = path;
        status_label_->setText(
            QString("Saved project to %1").arg(path)
        );
    }
    catch (const std::exception &e)
    {
        QMessageBox::warning(
            this, "Save Project Failed",
            QString("Failed to save project:\n%1\n\n%2").arg(path, e.what())
        );
    }
}

void PostprocessWidget::LoadProject(const QString &path)
{
    // Interactive wrapper: same work as the RPC entry point, failures become
    // a dialog instead of a JSON-RPC error.
    try
    {
        LoadProjectOrThrow(path);
    }
    catch (const std::exception &e)
    {
        QMessageBox::warning(
            this, "Load Project Failed",
            QString("Failed to load project:\n%1\n\n%2").arg(path, e.what())
        );
    }
}

void PostprocessWidget::LoadProjectOrThrow(const QString &path)
{
    // Drop the datasets of a previously loaded project so re-opening
    // REPLACES the active set (rel::Environment::LoadFromConfig itself
    // preserves existing registries).  A Block tab's frame is owned and
    // cached by the Block, which RemoveDataset destroys -- close any open
    // block tabs first so they do not dangle.
    data_frame_view_->ClearBlockTabs();
    for (const std::string &name : rel::Environment::DatasetNames())
    {
        rel::Environment::RemoveDataset(name);
    }

    // EquationManager::LoadFromFile clears equations/expressions, loads
    // the referenced datasets (relative dataset paths are resolved against
    // the project file's directory), restores equations/expressions, then
    // Update()s to recompute all values.  Host signal connections survive
    // (LoadFromFile does not disconnect external observers).
    EquationManager::GetInstance().LoadFromFile(path.toStdString());

    project_path_ = path;
    RefreshDatasetCombo();
    RefreshEquationList();
    manager_tree_->Refresh();   // dataset tree changed

    const std::vector<std::string> names = rel::Environment::DatasetNames();
    QString status = QString("Loaded %1: %2 dataset(s)")
                         .arg(QFileInfo(path).fileName())
                         .arg(static_cast<int>(names.size()));
    if (xdataset::Dataset *default_ds = rel::Environment::DefaultDataset())
    {
        status += QString(", default: %1")
                      .arg(QString::fromStdString(default_ds->name()));
    }
    if (!rel::Environment::PythonPlugins().empty() &&
        !rel::Environment::IsPythonAvailable())
    {
        status += "  (python_plugins ignored: no Python support in this build)";
    }
    status_label_->setText(status);
}

// =========================================================================
// Programmatic API (JSON-RPC surface + GUI share these)
// =========================================================================

void PostprocessWidget::ApplyStartupConfig(const QJsonObject &config,
                                           QStringList *errors)
{
    auto fail = [errors](const QString &what) {
        if (errors)
        {
            errors->append(what);
        }
    };

    // ---- datasets (name / format / path) --------------------------------
    for (const QJsonValue &value : config.value("datasets").toArray())
    {
        if (!value.isObject())
        {
            continue;
        }
        const QJsonObject o = value.toObject();
        const QString name = o.value("name").toString().trimmed();
        const QString path = o.value("path").toString().trimmed();
        if (name.isEmpty() || path.isEmpty())
        {
            fail(QStringLiteral("dataset entry needs \"name\" and \"path\""));
            continue;
        }
        const QString format = o.value("format").toString().trimmed();
        try
        {
            AddDataset(name, format.isEmpty() ? QStringLiteral("hdf5") : format,
                       path, false);
        }
        catch (const std::exception &e)
        {
            fail(QStringLiteral("dataset %1: %2").arg(name, QString::fromUtf8(e.what())));
        }
    }

    // ---- default dataset -------------------------------------------------
    const QString default_dataset = config.value("default_dataset").toString().trimmed();
    if (!default_dataset.isEmpty())
    {
        try
        {
            SetDefaultDataset(default_dataset);
        }
        catch (const std::exception &e)
        {
            fail(QStringLiteral("default_dataset %1: %2")
                     .arg(default_dataset, QString::fromUtf8(e.what())));
        }
    }

    // ---- equations (name / content / tag) --------------------------------
    for (const QJsonValue &value : config.value("equations").toArray())
    {
        if (!value.isObject())
        {
            continue;
        }
        const QJsonObject o = value.toObject();
        const QString name = o.value("name").toString().trimmed();
        const QString content = o.value("content").toString().trimmed();
        if (name.isEmpty() || content.isEmpty())
        {
            fail(QStringLiteral("equation entry needs \"name\" and \"content\""));
            continue;
        }
        try
        {
            AddEquation(name, content, o.value("tag").toString(), true);
        }
        catch (const std::exception &e)
        {
            fail(QStringLiteral("equation %1: %2").arg(name, QString::fromUtf8(e.what())));
        }
    }

    // ---- expressions (content / tag) -------------------------------------
    for (const QJsonValue &value : config.value("expressions").toArray())
    {
        if (!value.isObject())
        {
            continue;
        }
        const QJsonObject o = value.toObject();
        const QString content = o.value("content").toString().trimmed();
        if (content.isEmpty())
        {
            fail(QStringLiteral("expression entry needs \"content\""));
            continue;
        }
        try
        {
            AddExpression(content, o.value("tag").toString());
        }
        catch (const std::exception &e)
        {
            fail(QStringLiteral("expression %1: %2")
                     .arg(content, QString::fromUtf8(e.what())));
        }
    }

    RefreshEquationList();
    RefreshDatasetCombo();
    manager_tree_->Refresh();
}

ObjectId PostprocessWidget::AddEquation(const QString &name, const QString &content,
                                        const QString &tag, bool redefine)
{
    const std::string name_std = name.trimmed().toStdString();
    const std::string content_std = content.trimmed().toStdString();
    if (content_std.empty())
    {
        throw std::runtime_error("equation content must not be empty");
    }

    EquationManager &mgr = EquationManager::GetInstance();
    const std::string tag_std =
        tag.isEmpty() ? std::string(kEquationTagDefault) : tag.toStdString();

    ObjectId id;
    if (mgr.IsEquationExist(name_std))
    {
        if (!redefine)
        {
            throw std::runtime_error("equation already exists: " + name_std);
        }
        id = mgr.EditEquation(name_std, content_std);
    }
    else
    {
        if (!EquationManager::IsValidEquationIdentifier(name_std))
        {
            throw std::runtime_error("invalid or reserved equation name: " + name_std);
        }
        // A parse error / cycle does NOT throw: the equation is created with
        // status kError and stays off the dependency graph until it heals.
        id = mgr.AddEquation(name_std, content_std, tag_std);
    }
    mgr.Update();

    RefreshEquationList();
    SelectEquationByName(name.trimmed());
    return id;
}

ObjectId PostprocessWidget::AddExpression(const QString &content, const QString &tag)
{
    const std::string content_std = content.trimmed().toStdString();
    if (content_std.empty())
    {
        throw std::runtime_error("expression content must not be empty");
    }

    const std::string tag_std =
        tag.isEmpty() ? std::string(kWatchTagDefault) : tag.toStdString();

    const ObjectId id =
        EquationManager::GetInstance().AddExpression(content_std, tag_std);
    if (!id.is_nil())
    {
        data_frame_view_->AddExpression(id);
    }
    return id;
}

void PostprocessWidget::AddDataset(const QString &name, const QString &format,
                                   const QString &path, bool make_default)
{
    const std::string name_std = name.trimmed().toStdString();
    if (name_std.empty())
    {
        throw std::runtime_error("dataset name must not be empty");
    }
    if (!QFileInfo::exists(path))
    {
        throw std::runtime_error("dataset file not found: " + path.toStdString());
    }

    // A Block tab's frame is owned by the Block; replacing a dataset of the
    // same name destroys those Blocks, so close block tabs first.
    if (rel::Environment::FindDataset(name_std) != nullptr)
    {
        data_frame_view_->ClearBlockTabs();
        rel::Environment::RemoveDataset(name_std);
    }

    const std::string format_std =
        format.isEmpty() ? std::string("hdf5") : format.toStdString();
    xdataset::Dataset loaded = xdataset::DatasetIO::Load(
        format_std, QDir::toNativeSeparators(path).toStdString(), name_std);
    rel::Environment::AddDataset(std::unique_ptr<xdataset::Dataset>(
        new xdataset::Dataset(std::move(loaded))));

    if (make_default || rel::Environment::DefaultDatasetName().empty())
    {
        rel::Environment::SetDefaultDataset(name_std);
    }

    // Bare DataArray names resolve against the default dataset, and a new
    // dataset can heal equations that were detached -- recompute everything.
    EquationManager::GetInstance().Update();

    RefreshDatasetCombo();
    RefreshEquationList();
    manager_tree_->Refresh();
    status_label_->setText(
        QString("Added dataset %1 (%2)").arg(name, format_std.c_str()));
}

void PostprocessWidget::RemoveDataset(const QString &name)
{
    const std::string name_std = name.trimmed().toStdString();
    if (rel::Environment::FindDataset(name_std) == nullptr)
    {
        throw std::runtime_error("no such dataset: " + name_std);
    }

    // Block tabs hold a pointer into the Block's cached frame -- drop them
    // before the Dataset (and its Blocks) are destroyed.
    data_frame_view_->ClearBlockTabs();
    rel::Environment::RemoveDataset(name_std);
    EquationManager::GetInstance().Update();

    RefreshDatasetCombo();
    RefreshEquationList();
    manager_tree_->Refresh();
    status_label_->setText(QString("Removed dataset %1").arg(name));
}

void PostprocessWidget::SetDefaultDataset(const QString &name)
{
    const std::string name_std = name.trimmed().toStdString();
    if (rel::Environment::FindDataset(name_std) == nullptr)
    {
        throw std::runtime_error("no such dataset: " + name_std);
    }

    // Route through the combo so the UI state and the REL default stay in
    // sync (the combo handler performs SetDefaultDataset + Update + refresh).
    const int index = dataset_combo_->findText(name.trimmed());
    if (index >= 0 && index != dataset_combo_->currentIndex())
    {
        dataset_combo_->setCurrentIndex(index);
        return;  // currentIndexChanged -> OnDatasetSelectionChanged did the work
    }
    OnDatasetSelectionChanged(index >= 0 ? index : -1);
    if (index < 0)
    {
        rel::Environment::SetDefaultDataset(name_std);
        EquationManager::GetInstance().Update();
        RefreshDatasetCombo();
        manager_tree_->Refresh();
    }
}

std::vector<std::string> PostprocessWidget::DatasetNames() const
{
    // Registration order (no sort): newest dataset last.
    return rel::Environment::DatasetNames();
}

QString PostprocessWidget::DefaultDatasetName() const
{
    return QString::fromStdString(rel::Environment::DefaultDatasetName());
}

void PostprocessWidget::RaiseWindow()
{
    // A raise request from another process needs the full dance: un-minimize,
    // re-assert the window, then activate.  On Windows a plain raise() from a
    // background process is ignored, so the always-on-top flag is toggled to
    // force the Z-order change without leaving the window pinned.
    QWidget *window = this->window();
    if (window->isMinimized() || !window->isVisible())
    {
        window->showNormal();
    }
    window->raise();

#ifdef Q_OS_WIN
    const bool was_on_top = window->windowFlags().testFlag(Qt::WindowStaysOnTopHint);
    if (!was_on_top)
    {
        window->setWindowFlag(Qt::WindowStaysOnTopHint, true);
        window->show();
        window->setWindowFlag(Qt::WindowStaysOnTopHint, false);
        window->show();
    }
#endif

    window->activateWindow();
    QApplication::alert(window);
}

void PostprocessWidget::SetStatusText(const QString &text)
{
    status_label_->setText(text);
}

void PostprocessWidget::RefreshDatasetCombo()
{
    dataset_combo_->blockSignals(true);
    dataset_combo_->clear();

    // Registration order (no sort): newest dataset last, matching the tree.
    const std::vector<std::string> names = rel::Environment::DatasetNames();
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

void PostprocessWidget::OnDatasetSelectionChanged(int index)
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

void PostprocessWidget::RefreshEquationList()
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
        QListWidgetItem *item =
            new QListWidgetItem(QString::fromStdString(name));
        // An equation that exists but is not on the dependency graph (syntax
        // error / cycle) is flagged in place -- no dialog, just a red entry
        // whose tooltip carries the reason.
        if (!mgr.IsEquationRegistered(name))
        {
            const Equation *equation = mgr.GetEquation(name);
            item->setForeground(Qt::red);
            item->setToolTip(
                QString("Not on the dependency graph (will not be computed):\n%1")
                    .arg(equation ? QString::fromStdString(equation->message)
                                  : QString("unknown error"))
            );
        }
        equation_list_->addItem(item);
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

void PostprocessWidget::UpdateEquationButtons(bool enabled)
{
    redefine_button_->setEnabled(enabled);
    rename_button_->setEnabled(enabled);
    delete_button_->setEnabled(enabled);
}

void PostprocessWidget::OnEquationListSelectionChanged()
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
    UpdateEquationButtons(item != nullptr);

    // Selected equation ids (resolved from the item text; the tree matches
    // both equations and expressions by ObjectId).
    std::vector<ObjectId> selected_ids;
    selected_ids.reserve(items.size());
    for (const QListWidgetItem *it : items)
    {
        const Equation *equation =
            EquationManager::GetInstance().GetEquation(it->text().toStdString());
        if (equation)
        {
            selected_ids.push_back(equation->id);
        }
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
        manager_tree_->SetObjectSelection(selected_ids);
    }

    // Rule 3: unpinned tabs = the list's selected equations (ObjectIds).
    // Unpinned tree previews (data arrays / expressions / blocks) are dropped
    // -- they only appear while the tree is the last-clicked panel.  Pinned
    // tabs survive (SyncTabs keeps them regardless of the visible set).
    data_frame_view_->SyncBlockTabs({});
    data_frame_view_->SyncTabs(selected_ids);
}

QString PostprocessWidget::CurrentSelectedEquationName() const
{
    const QListWidgetItem *item = equation_list_->currentItem();
    if (!item)
    {
        return QString();
    }
    // Item text is the equation name itself.
    return item->text();
}

void PostprocessWidget::SelectEquationByName(const QString &name)
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

void PostprocessWidget::OnManagerTreeClicked()
{
    // Selection changes are already routed through OnManagerTreeSelectionChanged
    // (the selectionModel's selectionChanged fires on click too).  Because the
    // list-driven mirror replaces the whole tree selection with equation
    // leaves, clicking any tree-only node always *changes* the selection, so
    // this slot only gives the tree focus for keyboard navigation.
    manager_tree_->setFocus();
}

void PostprocessWidget::OnManagerTreeSelectionChanged()
{
    using SelectionInfo = ExplorerView::SelectionInfo;
    using NodeKind = ExplorerView::NodeKind;

    // The tree is the last-clicked panel.  Rules: equation leaves mirror into
    // the list (1); unpinned tabs follow the tree's selected items (3); the
    // property panel shows the last-clicked (focus) node (4).  A programmatic
    // Refresh() suppresses its own selectionChanged, so an empty set really is
    // a user deselection.
    const std::vector<SelectionInfo> infos = manager_tree_->SelectedInfos();
    if (infos.empty())
    {
        data_frame_view_->SyncBlockTabs({});
        data_frame_view_->SyncTabs({});
        return;
    }

    EquationManager &mgr = EquationManager::GetInstance();

    // Focus = the first selected node in tree order (infos is non-empty here).
    // It drives both the property panel (Rule 4) and the list's current item
    // (Rule 1).
    const SelectionInfo focus = infos.front();
    const bool focus_is_equation = (focus.kind == NodeKind::kEquation);

    // Rule 1: mirror equation leaves into the list (other node kinds leave it
    // untouched).  The list rows are keyed by name, so each equation id is
    // resolved back to its name only at this boundary.
    std::vector<ObjectId> eq_ids;
    for (const SelectionInfo &info : infos)
    {
        if (info.kind == NodeKind::kEquation && !info.object_id.is_nil())
        {
            eq_ids.push_back(info.object_id);
        }
    }
    if (!eq_ids.empty())
    {
        equation_list_->blockSignals(true);
        equation_list_->clearSelection();
        QListWidgetItem *current_item = nullptr;
        for (const ObjectId &eq_id : eq_ids)
        {
            const Equation *equation = mgr.GetEquationById(eq_id);
            if (!equation)
            {
                continue;
            }
            const QList<QListWidgetItem *> items =
                equation_list_->findItems(QString::fromStdString(equation->name), Qt::MatchExactly);
            if (items.isEmpty())
            {
                continue;
            }
            QListWidgetItem *list_item = items.first();
            list_item->setSelected(true);
            if (!current_item || (focus_is_equation && eq_id == focus.object_id))
            {
                current_item = list_item;
            }
        }
        equation_list_->setCurrentItem(current_item);
        equation_list_->blockSignals(false);
    }

    UpdateEquationButtons(equation_list_->currentItem() != nullptr);

    // Rule 3: unpinned tabs = the tree's selected items.  Equations / block /
    // data-array selections produce ObjectIds (a data array maps to a hidden
    // access expression); blocks are keyed separately.
    std::vector<ObjectId> visible_ids;
    std::vector<std::pair<QString, QString>> visible_blocks;
    for (const SelectionInfo &info : infos)
    {
        if ((info.kind == NodeKind::kEquation || info.kind == NodeKind::kExpression) &&
            !info.object_id.is_nil())
        {
            visible_ids.push_back(info.object_id);
        }
        else if (info.kind == NodeKind::kDataArray)
        {
            const ObjectId access_id = manager_tree_->GetDataArrayExpression(
                info.dataset, info.block_path, info.data_array);
            if (!access_id.is_nil())
            {
                visible_ids.push_back(access_id);
            }
        }
        else if (info.kind == NodeKind::kBlock)
        {
            visible_blocks.emplace_back(info.dataset, info.block_path);
        }
    }
    data_frame_view_->SyncBlockTabs(visible_blocks);
    data_frame_view_->SyncTabs(visible_ids);

    // Rule 4: property panel shows the focus node.
    switch (focus.kind)
    {
    case NodeKind::kEquation:
        property_widget_->SetObject(focus.object_id);
        return;
    case NodeKind::kExpression:
        if (!focus.object_id.is_nil() && mgr.GetExpression(focus.object_id))
        {
            property_widget_->SetObject(focus.object_id);
        }
        return;
    case NodeKind::kDataset:
        property_widget_->ShowDatasetNode(focus.dataset);
        return;
    case NodeKind::kBlock:
        property_widget_->ShowBlockNode(focus.dataset, focus.block_path);
        return;
    case NodeKind::kDataArray:
        // The property shows the access expression; SetObject(nil) when the
        // access expression could not be registered.
        property_widget_->SetObject(manager_tree_->GetDataArrayExpression(
            focus.dataset, focus.block_path, focus.data_array));
        return;
    default:
        // Non-displayable nodes (Datasets group, Tag group) -> clear.
        property_widget_->SetObject(xequation::ObjectId());
        return;
    }
}

} // namespace gui
} // namespace xresults
