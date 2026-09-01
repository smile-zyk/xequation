#pragma once
#include <string>
#include <unordered_set>
#include <vector>

#include "core/equation_manager.h"
#include "dataset.h"

namespace xequation
{
namespace rel_engine
{

// Dataset lifecycle -> ExternalInput anchors. Named datasets need a single
// anchor { ds }; the default dataset needs every accessible form of each
// DataArray (prefix may be omitted). Anchor ops never recompute; Invalidate*
// recompute never change anchors; SwitchDataset = move anchors + invalidate.
class DatasetExternalInputs
{
  public:
    explicit DatasetExternalInputs(EquationManager &manager) : manager_(manager) {}

    // Register a named dataset's anchor ({ ds_name }). Idempotent.
    void RegisterDataset(const std::string &ds_name);

    // Remove a named dataset's anchor. No-op if ds_name is not registered.
    void UnregisterDataset(const std::string &ds_name);

    // Register ds_name's default-domain symbols (clean-state only; no removal,
    // no recompute -- see SwitchDataset). Host must have called
    // rel::Environment::SetDefaultDataset(ds_name) first.
    void SetDefaultDataset(const std::string &ds_name);

    // Invalidate a named dataset; ds_name must be a registered anchor.
    void InvalidateDataset(const std::string &ds_name);

    // Invalidate all symbols of the current default domain.
    void InvalidateDefaultDataset();

    // Switch default to new_ds_name: register new symbols, remove
    // old-default-only anchors, invalidate new + old (downstream recomputed to
    // NameError instead of stale Success). Host must have called
    // rel::Environment::SetDefaultDataset(new_ds_name) first.
    void SwitchDataset(const std::string &new_ds_name);

    // Default-domain symbols of a dataset: full dotted paths ("sim.SP.Vout")
    // + bare short names unique in the dataset ("Vout"). No intermediate path
    // prefixes (blocks are not evaluable symbols).
    static std::vector<std::string> CollectDefaultDatasetSymbolNames(const xdataset::Dataset &ds);

  private:
    EquationManager &manager_;

    // Anchors currently registered for the default domain.
    std::unordered_set<std::string> default_symbols_;
};

} // namespace rel_engine
} // namespace xequation