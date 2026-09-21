#include "format_options_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace xresults
{
namespace gui
{

namespace
{
using xdataset::ComplexFormat;
using xdataset::NumberFormat;

/// The six NumberFormat values, in menu order.
const NumberFormat kNumberFormats[] = {
    NumberFormat::kFull,
    NumberFormat::kScientific,
    NumberFormat::kEngineering,
    NumberFormat::kHex,
    NumberFormat::kOctal,
    NumberFormat::kBinary,
};

/// The five ComplexFormat values, in menu order.
const ComplexFormat kComplexFormats[] = {
    ComplexFormat::kRealImaginary,
    ComplexFormat::kMagDegrees,
    ComplexFormat::kDbDegrees,
    ComplexFormat::kMagRadians,
    ComplexFormat::kDbRadians,
};

/// Hex / octal / binary are exact integer representations: xdataset ignores
/// significant_digits and never appends a unit in those modes.
bool IsPositionalBase(NumberFormat format)
{
    return format == NumberFormat::kHex || format == NumberFormat::kOctal
        || format == NumberFormat::kBinary;
}

int NumberFormatIndex(NumberFormat format)
{
    const int count =
        static_cast<int>(sizeof(kNumberFormats) / sizeof(kNumberFormats[0]));
    for (int i = 0; i < count; ++i)
    {
        if (kNumberFormats[i] == format)
        {
            return i;
        }
    }
    return 0;
}

int ComplexFormatIndex(ComplexFormat format)
{
    const int count =
        static_cast<int>(sizeof(kComplexFormats) / sizeof(kComplexFormats[0]));
    for (int i = 0; i < count; ++i)
    {
        if (kComplexFormats[i] == format)
        {
            return i;
        }
    }
    return 0;
}
} // namespace

FormatOptionsWidget::FormatOptionsWidget(QWidget *parent)
    : QWidget(parent)
{
    SetupUI();
    SetupConnections();
    Load(xdataset::FormatOptions());
}

FormatOptionsWidget::FormatOptionsWidget(
    const xdataset::FormatOptions &initial, QWidget *parent)
    : QWidget(parent)
{
    SetupUI();
    SetupConnections();
    Load(initial);
}

void FormatOptionsWidget::SetupUI()
{
    number_format_combo_ = new QComboBox(this);
    number_format_combo_->addItem(QStringLiteral("Full"));
    number_format_combo_->addItem(QStringLiteral("Scientific"));
    number_format_combo_->addItem(QStringLiteral("Engineering"));
    number_format_combo_->addItem(QStringLiteral("Hex"));
    number_format_combo_->addItem(QStringLiteral("Octal"));
    number_format_combo_->addItem(QStringLiteral("Binary"));
    number_format_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    complex_format_combo_ = new QComboBox(this);
    complex_format_combo_->addItem(QStringLiteral("Real / Imaginary"));
    complex_format_combo_->addItem(QStringLiteral("Magnitude / Degrees"));
    complex_format_combo_->addItem(QStringLiteral("dB / Degrees"));
    complex_format_combo_->addItem(QStringLiteral("Magnitude / Radians"));
    complex_format_combo_->addItem(QStringLiteral("dB / Radians"));
    complex_format_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    significant_digits_spin_ = new QSpinBox(this);
    // Same range xdataset clamps to (a double round-trips in 17 digits).
    significant_digits_spin_->setRange(1, 17);
    significant_digits_spin_->setToolTip(QStringLiteral(
        "How many significant digits Full / Scientific / Engineering keep.\n"
        "The budget is spent on the digits before the decimal point first;\n"
        "trailing zeros are never padded."));

    show_unit_check_ = new QCheckBox(QStringLiteral("Show unit"), this);
    show_unit_check_->setToolTip(QStringLiteral(
        "Append the unit suffix (and, in Engineering, its SI prefix).\n"
        "When off, the value is shown bare and is not scaled."));

    // Rows are wrapped in QWidgets so UpdateDependentRows() can disable a
    // label and its editor together (QFormLayout::setRowVisible does not exist
    // in Qt 5).
    digits_row_ = new QWidget(this);
    {
        auto *layout = new QHBoxLayout(digits_row_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(
            new QLabel(QStringLiteral("Significant digits"), digits_row_));
        layout->addWidget(significant_digits_spin_);
        layout->addStretch(1);
    }

    unit_row_ = new QWidget(this);
    {
        auto *layout = new QHBoxLayout(unit_row_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(show_unit_check_);
        layout->addStretch(1);
    }

    auto *form = new QFormLayout();
    form->addRow(QStringLiteral("Number format"), number_format_combo_);
    form->addRow(QStringLiteral("Complex format"), complex_format_combo_);
    form->addRow(digits_row_);
    form->addRow(unit_row_);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
}

void FormatOptionsWidget::SetupConnections()
{
    connect(number_format_combo_,
            static_cast<void (QComboBox::*)(int)>(
                &QComboBox::currentIndexChanged),
            this, [this](int) { OnEditorChanged(); });
    connect(complex_format_combo_,
            static_cast<void (QComboBox::*)(int)>(
                &QComboBox::currentIndexChanged),
            this, [this](int) { OnEditorChanged(); });
    connect(significant_digits_spin_,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, [this](int) { OnEditorChanged(); });
    connect(show_unit_check_, &QCheckBox::toggled,
            this, [this](bool) { OnEditorChanged(); });
}

void FormatOptionsWidget::OnEditorChanged()
{
    UpdateDependentRows();
    if (!loading_)
    {
        emit FormatOptionsChanged(format_options());
    }
}

void FormatOptionsWidget::Load(const xdataset::FormatOptions &options)
{
    // Writing the widgets fires their change callbacks; keep those silent so a
    // programmatic load is not reported as a user edit.
    loading_ = true;

    number_format_combo_->setCurrentIndex(
        NumberFormatIndex(options.number_format));
    complex_format_combo_->setCurrentIndex(
        ComplexFormatIndex(options.complex_format));
    significant_digits_spin_->setValue(options.significant_digits);
    show_unit_check_->setChecked(options.show_unit);
    UpdateDependentRows();

    loading_ = false;
}

void FormatOptionsWidget::SetFormatOptions(const xdataset::FormatOptions &options)
{
    Load(options);
}

void FormatOptionsWidget::UpdateDependentRows()
{
    const bool positional = IsPositionalBase(
        kNumberFormats[number_format_combo_->currentIndex()]);
    // A positional base is an exact integer: no digit budget, no unit.
    digits_row_->setEnabled(!positional);
    unit_row_->setEnabled(!positional);
}

xdataset::FormatOptions FormatOptionsWidget::format_options() const
{
    xdataset::FormatOptions options;
    options.number_format =
        kNumberFormats[number_format_combo_->currentIndex()];
    options.complex_format =
        kComplexFormats[complex_format_combo_->currentIndex()];
    options.significant_digits = significant_digits_spin_->value();
    options.show_unit = show_unit_check_->isChecked();
    return options;
}

} // namespace gui
} // namespace xresults
