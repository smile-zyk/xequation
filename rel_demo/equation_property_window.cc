#include "equation_property_window.h"

#include "core/equation.h"
#include "core/equation_common.h"
#include "core/equation_value.h"

#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "xdataset_predefine.h"

#include <sstream>
#include <string>

namespace xequation
{
namespace rel_demo
{

namespace
{
/// 把 tsl::ordered_set<std::string> 渲染成 "[a, b, c]" 形式。
template <typename Set>
QString FormatSet(const Set &items)
{
    std::ostringstream oss;
    oss << '[';
    bool first = true;
    for (const auto &item : items)
    {
        if (!first)
        {
            oss << ", ";
        }
        oss << item;
        first = false;
    }
    oss << ']';
    return QString::fromStdString(oss.str());
}
} // namespace

EquationPropertyWindow::EquationPropertyWindow(const Equation *equation, QWidget *parent)
    : QDialog(parent),
      equation_(equation)
{
    // 无参构造时 equation 可能为空（异常兜底），此时显示空窗口。
    setWindowTitle(
        equation_ ? QString("Equation Property: %1")
                        .arg(QString::fromStdString(equation_->name()))
                  : QString("Equation Property")
    );
    setMinimumSize(460, 320);
    SetupUI();
}

EquationPropertyWindow::~EquationPropertyWindow() = default;

void EquationPropertyWindow::AddRow(
    QFormLayout *form, const QString &field, const QString &value, bool red
)
{
    QLabel *field_label = new QLabel(field, this);
    field_label->setStyleSheet("font-weight: bold;");

    QLabel *value_label = new QLabel(value, this);
    value_label->setWordWrap(true);
    value_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (red)
    {
        // 红色强调错误信息。
        value_label->setStyleSheet("color: #d32f2f; font-weight: bold;");
    }

    form->addRow(field_label, value_label);
}

void EquationPropertyWindow::SetupUI()
{
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout();

    if (!equation_)
    {
        QLabel *empty = new QLabel("No equation.", this);
        main_layout->addWidget(empty);
        return;
    }

    // =====================================================================
    // 1. Equation 元信息（参照 EquationBrowser）
    // =====================================================================
    AddRow(form, "Name", QString::fromStdString(equation_->name()));
    AddRow(form, "Expression", QString::fromStdString(equation_->content()));
    AddRow(form, "Type", QString::fromStdString(ItemTypeConverter::ToString(equation_->type())));
    AddRow(form, "Status", QString::fromStdString(ResultStatusConverter::ToString(equation_->status())));

    // Message 行：有内容才显示；计算失败时以红色突出，不额外新增重复的 Error 行。
    const std::string &message = equation_->message();
    if (!message.empty())
    {
        const bool has_error = equation_->status() != ResultStatus::kSuccess;
        AddRow(form, "Message", QString::fromStdString(message), has_error);
    }

    // 依赖 / 被依赖（表达式依赖图）。
    AddRow(form, "Dependencies", FormatSet(equation_->GetDependencies()));
    AddRow(form, "Dependents", FormatSet(equation_->GetDependents()));

    // =====================================================================
    // 2. 引擎值信息（计算失败时不展示值，避免误导）
    // =====================================================================
    const EquationValue &value = equation_->GetValue();

    if (equation_->status() == ResultStatus::kSuccess && value.IsRelValue())
    {
        // ---- REL 引擎：参照 builtin_library 的 what(x) 输出 -----------
        const rel::Value &rel_value = value.AsRel();

        AddRow(form, "Indep", FormatSet(rel_value.indep_names()));
        AddRow(
            form, "Kind",
            QString::fromStdString(rel_value.is_dependent() ? "Dependent" : "Independent")
        );
        AddRow(form, "Dimension", QString::fromStdString(rel_value.dimension_spec().to_string()));
        AddRow(form, "Data Shape", QString::fromStdString(rel_value.data_shape().to_string()));
        AddRow(
            form, "Data Type",
            QString::fromStdString(xdataset::DataTypeToString(rel_value.data_type()))
        );
        if (rel_value.unit().has_dimension())
        {
            AddRow(form, "Unit", QString::fromStdString(rel_value.unit().to_string()));
        }
        // 值本体不在此展示：已有 DataFrame 表格负责显示，避免复制冗长内容。
    }
    else if (equation_->status() == ResultStatus::kSuccess && value.IsPyObject())
    {
        // ---- Python 引擎：展示对象类型名（类名）--------------
        AddRow(form, "Python Type", QString::fromStdString(value.AsPyObject().TypeName()));
        // Python 值本体同样不在此展示，避免 repr 过长。
    }
    // 计算失败：值不可用（已在上方 Message 行红色显示错误），此处不展示。

    main_layout->addLayout(form);
    main_layout->addStretch();
}

} // namespace rel_demo
} // namespace xequation
