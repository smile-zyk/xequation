#pragma once

#include <QDialog>

class QFormLayout;

namespace xequation
{
class Equation;

namespace rel_demo
{

// =========================================================================
// EquationPropertyWindow -- 单个 Equation 的属性展示窗口。
//
// 展示内容分两部分：
//   1. Equation 元信息（参照 EquationBrowser）：Name / Expression / Type /
//      Status / Message / Dependencies / Dependents。
//   2. 引擎值信息：
//      - REL 引擎：参照 builtin_library 的 what(x) 输出，展示 Indep / Kind /
//        Dimension / Data Shape / Data Type / Unit，随后按需展示 Value 值。
//      - Python 引擎：除了 Equation 元信息，展示 Python 对象类型名（类名）。
//
// 若方程计算失败（status != success），Message 会以红色显示。
// =========================================================================

class EquationPropertyWindow : public QDialog
{
    Q_OBJECT

  public:
    explicit EquationPropertyWindow(const Equation *equation, QWidget *parent = nullptr);
    ~EquationPropertyWindow() override;

  private:
    void SetupUI();
    void AddRow(QFormLayout *form, const QString &field, const QString &value,
                bool red = false);

    const Equation *equation_ = nullptr;
};

} // namespace rel_demo
} // namespace xequation

