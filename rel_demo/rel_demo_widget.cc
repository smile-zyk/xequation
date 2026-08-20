#include "rel_demo_widget.h"

#include "rel_engine/rel_equation_engine.h"

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

#include "core/equation_group.h"

namespace xequation
{
namespace rel_demo
{

RelDemoWidget::RelDemoWidget(QWidget *parent)
    : QWidget(parent),
      equation_manager_(
          rel_engine::RelEquationEngine::GetInstance().CreateEquationManager()
      )
{
    SetupUI();
    SetupConnections();
}

RelDemoWidget::~RelDemoWidget() = default;

void RelDemoWidget::SetupUI()
{
    setWindowTitle("REL DataFrame Demo");
    setMinimumSize(900, 600);

    // ---- top: statement input ------------------------------------------
    statement_edit_ = new QLineEdit(this);
    statement_edit_->setPlaceholderText(
        "Insert equation, e.g.  y = [1, 2, 3]  (name = expression)"
    );
    insert_button_ = new QPushButton("Insert", this);
    redefine_button_ = new QPushButton("Redefine", this);
    rename_button_ = new QPushButton("Rename", this);
    redefine_button_->setEnabled(false);  // 需要先选中列表项
    rename_button_->setEnabled(false);    // 需要先选中列表项

    QHBoxLayout *input_layout = new QHBoxLayout();
    input_layout->addWidget(statement_edit_, 1);
    input_layout->addWidget(insert_button_);
    input_layout->addWidget(redefine_button_);
    input_layout->addWidget(rename_button_);

    // ---- middle: equation list + dataframe view ------------------------
    equation_list_ = new QListWidget(this);
    equation_list_->setSelectionMode(QAbstractItemView::SingleSelection);

    data_frame_view_ = new DataFrameTableView(this);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(equation_list_);
    splitter->addWidget(data_frame_view_);
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

void RelDemoWidget::SetupConnections()
{
    connect(insert_button_, &QPushButton::clicked, this, &RelDemoWidget::OnInsertEquation);
    connect(
        statement_edit_, &QLineEdit::returnPressed, this, &RelDemoWidget::OnInsertEquation
    );
    connect(redefine_button_, &QPushButton::clicked, this, &RelDemoWidget::OnRedefineEquation);
    connect(rename_button_, &QPushButton::clicked, this, &RelDemoWidget::OnRenameEquation);
    connect(
        equation_list_, &QListWidget::itemSelectionChanged, this,
        &RelDemoWidget::OnEquationListSelectionChanged
    );
}

bool RelDemoWidget::SplitStatement(
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

    // 名称必须是合法标识符（字母/数字/下划线，且不以数字开头）。
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

bool RelDemoWidget::IsValidIdentifier(const QString &name)
{
    if (name.isEmpty())
    {
        return false;
    }

    // 名称必须是合法标识符（字母/数字/下划线，且不以数字开头）。
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

QString RelDemoWidget::ReplaceIdentifierToken(
    const QString &content, const QString &old_name, const QString &new_name
)
{
    if (content.isEmpty() || old_name.isEmpty() || new_name.isEmpty())
    {
        return content;
    }

    // token 级（词边界）替换：只替换完整标识符，不误伤字符串字面量里
    // 的单词、以及 old_name 作为其它标识符前缀的情况（如 old_name="a"，
    // "abc" 中的 "a" 不替换）。
    const QString pattern = QString("\\b%1\\b").arg(QRegularExpression::escape(old_name));
    const QRegularExpression re(pattern);
    QString result = content;
    return result.replace(re, new_name);
}

void RelDemoWidget::OnInsertEquation()
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

    if (equation_manager_->IsEquationExist(name_std))
    {
        QMessageBox::warning(
            this, "Duplicate Equation",
            "An equation with this name already exists: " + name
        );
        return;
    }

    try
    {
        equation_manager_->AddEquation(name_std, expr.toStdString());
        equation_manager_->Update();
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

    // 选中新插入的方程，立即显示其 DataFrame。
    const QList<QListWidgetItem *> items = equation_list_->findItems(
        name, Qt::MatchExactly
    );
    if (!items.isEmpty())
    {
        equation_list_->setCurrentItem(items.first());
    }
}

void RelDemoWidget::OnRedefineEquation()
{
    const QString current_name = CurrentSelectedEquationName();
    if (current_name.isEmpty())
    {
        return;
    }

    const Equation *equation =
        equation_manager_->GetEquation(current_name.toStdString());
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
        // 重定义公式：EditSingleEquation 会重建依赖边并级联失效依赖者，
        // Update() 按拓扑序链式重算。
        equation_manager_->EditSingleEquation(
            group_id, name_std, trimmed.toStdString()
        );
        equation_manager_->Update();
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

void RelDemoWidget::OnRenameEquation()
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
        return;  // 未变更
    }

    if (equation_manager_->IsEquationExist(trimmed_new.toStdString()))
    {
        QMessageBox::warning(
            this, "Duplicate Equation",
            "An equation with this name already exists: " + trimmed_new
        );
        return;
    }

    const Equation *equation =
        equation_manager_->GetEquation(current_name.toStdString());
    if (!equation)
    {
        return;
    }

    const EquationGroupId group_id = equation->group_id();

    try
    {
        // 重命名 = 改写整个 group statement 文本（token 级替换旧名->新名）
        // 后交给 EditEquationGroup：它删除旧方程、添加新方程、重写所有
        // 依赖者的引用，并级联失效，Update() 触发链式重算。
        const std::string new_statement = ReplaceIdentifierToken(
            QString::fromStdString(equation_manager_->GetEquationGroup(group_id)->statement()),
            current_name, trimmed_new
        ).toStdString();

        equation_manager_->EditEquationGroup(group_id, new_statement);
        equation_manager_->Update();
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

void RelDemoWidget::RefreshEquationList()
{
    equation_list_->blockSignals(true);
    equation_list_->clear();

    const std::vector<std::string> names = equation_manager_->GetEquationNames();
    for (const std::string &name : names)
    {
        const Equation *equation = equation_manager_->GetEquation(name);
        QString item_text = QString::fromStdString(name);
        if (equation && equation->status() == ResultStatus::kSuccess)
        {
            item_text += QString("  [%1 row(s)]")
                             .arg(static_cast<qlonglong>(
                                 equation->GetValue().AsRel().rows()
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

void RelDemoWidget::OnEquationListSelectionChanged()
{
    const QListWidgetItem *item = equation_list_->currentItem();
    if (!item)
    {
        data_frame_view_->Clear();
        redefine_button_->setEnabled(false);
        rename_button_->setEnabled(false);
        return;
    }

    // 列表项文本格式: "name  [N row(s)]" -- 取空格前部分作为方程名。
    const QString text = item->text();
    const QString name = text.section(' ', 0, 0);
    redefine_button_->setEnabled(true);
    rename_button_->setEnabled(true);
    ShowEquation(name);
}

QString RelDemoWidget::CurrentSelectedEquationName() const
{
    const QListWidgetItem *item = equation_list_->currentItem();
    if (!item)
    {
        return QString();
    }
    // 列表项文本格式: "name  [N row(s)]" -- 取空格前部分作为方程名。
    return item->text().section(' ', 0, 0);
}

void RelDemoWidget::SelectEquationByName(const QString &name)
{
    const QList<QListWidgetItem *> items =
        equation_list_->findItems(name, Qt::MatchExactly);
    if (!items.isEmpty())
    {
        equation_list_->setCurrentItem(items.first());
    }
}

void RelDemoWidget::ShowEquation(const QString &equation_name)
{
    const std::string name_std = equation_name.toStdString();
    const Equation *equation = equation_manager_->GetEquation(name_std);
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
    data_frame_view_->SetEquationValue(value);

    if (value.IsRelValue())
    {
        const rel::Value &rel_value = value.AsRel();
        status_label_->setText(
            QString("%1  |  type=%2  |  shape=[%3]  |  rows=%4")
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
    else
    {
        status_label_->setText(
            QString("%1: only REL values are supported in this demo")
                .arg(equation_name)
        );
    }
}

} // namespace rel_demo
} // namespace xequation
