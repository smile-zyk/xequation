#pragma once

#include <QMetaType>
#include <QWidget>

#include "measurement.h"

class QCheckBox;
class QComboBox;
class QSpinBox;

namespace xresults
{
namespace gui
{

// =========================================================================
// FormatOptionsWidget -- an embeddable editor for xdataset::FormatOptions.
//
// A plain QWidget (no OK / Cancel, no dialog chrome) over the four fields of
// FormatOptions:
//
//   number_format       Full / Scientific / Engineering / Hex / Octal / Binary
//   complex_format      Real-Imaginary / Mag-Deg / dB-Deg / Mag-Rad / dB-Rad
//   significant_digits  1..17 (xdataset clamps to the same range)
//   show_unit           whether the unit suffix is appended
//
// The widget owns no global state: it holds a local copy of the options and
// reports every edit through FormatOptionsChanged(), so an embedder decides
// what to do with the value (apply live, stash until a Save button, ...).  It
// can therefore be dropped into a settings page, a dock, a side panel or --
// as DataFrameTabWidget does -- wrapped in a QDialog when a modal prompt is
// wanted.
//
// The significant_digits and show_unit rows are disabled while a positional
// base (Hex / Octal / Binary) is selected: those modes are exact integer
// representations, so xdataset ignores both fields.
// =========================================================================

class FormatOptionsWidget : public QWidget
{
    Q_OBJECT
  public:
    /// Start from the xdataset defaults (FormatOptions()).
    explicit FormatOptionsWidget(QWidget *parent = nullptr);

    /// Start from a specific set of options.
    FormatOptionsWidget(const xdataset::FormatOptions &initial,
                        QWidget *parent = nullptr);

    /// The options as currently shown by the widgets.
    xdataset::FormatOptions format_options() const;

    /// Show @p options without emitting FormatOptionsChanged (a programmatic
    /// update, not a user edit).
    void SetFormatOptions(const xdataset::FormatOptions &options);

  signals:
    /// Emitted whenever the user changes a field.  Never emitted by
    /// SetFormatOptions().
    void FormatOptionsChanged(const xdataset::FormatOptions &options);

  private:
    /// Build the widgets + layout (shared by both constructors).
    void SetupUI();

    /// Wire the editor widgets to the change handling.
    void SetupConnections();

    /// Reflect @p options into the widgets without emitting.
    void Load(const xdataset::FormatOptions &options);

    /// Enable / disable the rows a positional base ignores.
    void UpdateDependentRows();

    /// A user edit landed: refresh row enablement and publish the value.
    void OnEditorChanged();

    QComboBox *number_format_combo_ = nullptr;
    QComboBox *complex_format_combo_ = nullptr;
    QSpinBox *significant_digits_spin_ = nullptr;
    QCheckBox *show_unit_check_ = nullptr;
    QWidget *digits_row_ = nullptr;   ///< label + spin box, for enable/disable
    QWidget *unit_row_ = nullptr;     ///< checkbox, for enable/disable

    /// True while Load() is writing the widgets, so the resulting
    /// currentIndexChanged / valueChanged callbacks stay silent.
    bool loading_ = false;
};

} // namespace gui
} // namespace xresults

// Allow FormatOptionsWidget::FormatOptionsChanged to cross a queued connection
// (it is a plain copyable value, so the only missing piece is the type id).
Q_DECLARE_METATYPE(xdataset::FormatOptions)
