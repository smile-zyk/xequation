#pragma once

#include <string>

namespace xresults
{
namespace gui
{

// =========================================================================
// Tag definitions (host / UI layer only -- the core treats `tag` as an
// opaque string and stores whatever the caller passes).
//
// Identity tags are a *display* concern: they drive how the GUI groups
// Equations / Expressions in the manager tree, which groups are user-facing
// and which are internal.  Nothing here is required by the core.
// =========================================================================

// Default identity tags.  `static` gives each TU its own copy (C++11: no
// inline variables, no ODR issue when this header is included widely).
static const char kEquationTagDefault[] = "Equation";
static const char kMarkerTagDefault[] = "Marker";
static const char kWatchTagDefault[] = "Watch";
static const char kGraphTagDefault[] = "Graph";

// Identity tag of the host's lazy "DataArray access" expressions (content =
// a dataset path such as `LNA.a.b.Id.i`).  They are internal / host-only:
// the GUI never lists them under a tag group and never offers direct editing.
static const char kDataArrayAccessTag[] = "DataArrayAccess";

/// True when @p tag is the reserved internal "DataArray access" tag.
inline bool IsDataArrayAccessTag(const std::string &tag)
{
    return tag == kDataArrayAccessTag;
}

} // namespace gui
} // namespace xresults
