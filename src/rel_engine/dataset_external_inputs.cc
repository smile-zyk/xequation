#include "dataset_external_inputs.h"

#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "environment.h"

namespace xequation
{
namespace rel_engine
{

namespace
{

// Deduplicate preserving order (same as rel_parser's Dedupe; std::set keeps
// determinism by lexicographic order)
std::vector<std::string> Dedupe(std::vector<std::string> &&names)
{
    std::set<std::string> seen;
    std::vector<std::string> result;
    for (auto &n : names)
    {
        if (seen.insert(n).second)
        {
            result.push_back(std::move(n));
        }
    }
    return result;
}

// Look up a named dataset; empty or unknown name returns nullptr
xdataset::Dataset *FindDatasetOrNull(const std::string &name)
{
    if (name.empty())
    {
        return nullptr;
    }
    return rel::Environment::FindDataset(name);
}

// Register a set of names as external inputs (already-registered ones are
// skipped automatically)
void AddExternalInputs(EquationManager &manager, const std::vector<std::string> &names)
{
    for (const auto &n : names)
    {
        manager.AddExternalInput(n);
    }
}

} // namespace

std::vector<std::string> DatasetExternalInputs::CollectDefaultDatasetSymbolNames(
    const xdataset::Dataset &ds)
{
    std::vector<std::string> names;
    std::vector<std::string> bare_names;

    for (const auto &block_path : ds.GetAllBlockPaths())
    {
        std::string dotted = block_path;
        for (auto &ch : dotted)
        {
            if (ch == '/')
            {
                ch = '.';
            }
        }
        for (const auto &v : ds.GetDataArrayNames(block_path))
        {
            names.push_back(dotted + "." + v);  // "sim.SP.Vout"
            bare_names.push_back(v);
        }
    }

    // Bare short names, only when unique within this ds (e.g. "Vout")
    for (const auto &bare : bare_names)
    {
        if (ds.HasUniqueDataArray(bare))
        {
            names.push_back(bare);
        }
    }

    return Dedupe(std::move(names));
}

// ---------------------------------------------------------------------------
//  DatasetExternalInputs - anchor management (does NOT trigger recompute)
// ---------------------------------------------------------------------------

void DatasetExternalInputs::RegisterDataset(const std::string &ds_name)
{
    if (ds_name.empty())
    {
        return;
    }
    // Named dataset: every access carries the name (ds.xxx / ds..var both
    // start with ds), so a single anchor { ds } invalidates the whole domain.
    manager_.AddExternalInput(ds_name);
}

void DatasetExternalInputs::UnregisterDataset(const std::string &ds_name)
{
    manager_.RemoveExternalInput(ds_name);
}

void DatasetExternalInputs::SetDefaultDataset(const std::string &ds_name)
{
    // "Set default" in a clean state: only register this ds's default-domain
    // symbols (idempotent). Removing old anchors / triggering recompute is the
    // responsibility of SwitchDataset.
    xdataset::Dataset *ds = FindDatasetOrNull(ds_name);
    std::vector<std::string> symbols =
        ds ? CollectDefaultDatasetSymbolNames(*ds) : std::vector<std::string>{};

    AddExternalInputs(manager_, symbols);
    default_symbols_ = std::unordered_set<std::string>(symbols.begin(), symbols.end());
}

// ---------------------------------------------------------------------------
//  DatasetExternalInputs - invalidate / recompute (does NOT change anchors)
// ---------------------------------------------------------------------------

void DatasetExternalInputs::InvalidateDataset(const std::string &ds_name)
{
    manager_.InvalidateExternalInputs({ds_name});
}

void DatasetExternalInputs::InvalidateDefaultDataset()
{
    std::vector<std::string> names(default_symbols_.begin(), default_symbols_.end());
    if (!names.empty())
    {
        manager_.InvalidateExternalInputs(names);
    }
}

// ---------------------------------------------------------------------------
//  DatasetExternalInputs - switch default (move anchors + auto invalidate)
// ---------------------------------------------------------------------------

void DatasetExternalInputs::SwitchDataset(const std::string &new_ds_name)
{
    // Symbols of the new default domain
    xdataset::Dataset *new_ds = FindDatasetOrNull(new_ds_name);
    std::vector<std::string> new_symbols =
        new_ds ? CollectDefaultDatasetSymbolNames(*new_ds) : std::vector<std::string>{};
    std::unordered_set<std::string> new_set(new_symbols.begin(), new_symbols.end());

    // Old-default-only symbols (in the current anchor set but not in the new
    // set; their values disappear)
    std::vector<std::string> old_unique;
    for (const auto &sym : default_symbols_)
    {
        if (new_set.count(sym) == 0)
        {
            old_unique.push_back(sym);
        }
    }

    // Ordering matters: invalidate first (graph nodes still exist, dirt
    // propagates downstream along edges), then register the new set, then
    // invalidate the new set, and finally remove the old anchors (their
    // downstream has already been recomputed, so no stale values remain).
    if (!old_unique.empty())
    {
        manager_.InvalidateExternalInputs(old_unique);
    }

    // Register new default symbols (already-registered ones skipped)
    AddExternalInputs(manager_, new_symbols);

    // Invalidate all new default symbols (data / resolution target may change)
    if (!new_symbols.empty())
    {
        manager_.InvalidateExternalInputs(new_symbols);
    }

    // Remove old-default-only anchors last (their downstream was recomputed
    // to NameError in step 1)
    for (const auto &sym : old_unique)
    {
        manager_.RemoveExternalInput(sym);
    }

    default_symbols_ = std::move(new_set);
}

} // namespace rel_engine
} // namespace xequation