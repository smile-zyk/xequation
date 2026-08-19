#ifndef UNIT_H
#define UNIT_H

#include <string>

#include "unit_data.h"

// XDATASET_API: DLL export/import on Windows, no-op elsewhere.
#ifndef XDATASET_API
  #ifdef _WIN32
    #ifdef XDATASET_BUILD_DLL
      #define XDATASET_API __declspec(dllexport)
    #else
      #define XDATASET_API __declspec(dllimport)
    #endif
  #else
    #define XDATASET_API
  #endif
#endif

namespace xdataset
{

// =========================================================================
//  Unit — a physical unit type backed by a 7-SI-exponent vector
// =========================================================================
//
//  Stores a physical unit as {multiplier, UnitData}.  Multiplier
//  carries scale factors (1e3 for "k", 1e-3 for "m", etc.) while
//  UnitData carries the 7 SI base-unit exponents.
//
//  Construction by users goes through `Unit::parse()` which validates
//  against the REL unit vocabulary registered in UnitRegistry.
// =========================================================================

/// Result of best_display: how to convert a raw value for display.
struct UnitScale {
    double scale;       ///< Multiply raw value by this to get display value.
    std::string name;   ///< Display-unit string (empty if dimensionless).
};

class XDATASET_API Unit
{
public:
    // ---- construction ---------------------------------------------------

    /// Default: dimensionless, multiplier 1.
    Unit();

    /// True when the unit is in canonical form (multiplier == 1).
    bool is_canonical() const;

    /// Return a canonicalised copy (multiplier absorbed, unit = base_units).
    Unit canonicalized() const;

    // ---- queries --------------------------------------------------------

    /// Physical multiplier (value scale factor).
    double multiplier() const;

    /// True when a and b represent the same physical dimension.
    bool same_dimension(const Unit& other) const;

    /// True when the physical dimension is empty (regardless of multiplier).
    bool has_dimension() const;

    // ---- string conversion ----------------------------------------------

    /// Human-readable string.  Tries REL vocabulary first, falls back to
    /// raw SI-exponent combination (e.g. "kg*m^2*s^-3*A^-1").
    std::string to_string() const;

    // ---- display -------------------------------------------------------

    /// Given a raw value in this unit, pick the best display scale.
    /// Returns {scale, display_unit_string}.  Display value = raw * scale.
    /// For example, 1e9 Hz → {1e-9, "GHz"}, 0.002 V → {1000, "mV"}.
    UnitScale best_display(double value) const;

    // ---- static factory ------------------------------------------------

    /// Parse a REL unit string.  Throws std::invalid_argument when the
    /// string is not in the REL vocabulary.
    static Unit parse(const std::string& s);

    // ---- arithmetic on dimensions (inputs must be canonical) ------------

    Unit operator*(const Unit& other) const;
    Unit operator/(const Unit& other) const;
    Unit pow(int n) const;

    // ---- comparison ----------------------------------------------------

    bool operator==(const Unit& other) const;
    bool operator!=(const Unit& other) const;

private:
    // Private constructor — only for internal use
    // by canonicalize / multiply_dim / etc.
    explicit Unit(double mult, UnitData dim);

    double   mult_ = 1.0;
    UnitData dim_;
};
} // namespace xdataset

#endif // UNIT_H
