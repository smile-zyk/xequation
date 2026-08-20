#ifndef UNIT_H
#define UNIT_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "xdataset_predefine.h"

namespace xdataset
{

// =========================================================================
//  UnitData — 7 SI base-unit exponents as an 8-byte integer vector
// =========================================================================
//
//  7 dimensions, each an int8_t exponent:
//
//      m    - meter      (length)
//      kg   - kilogram   (mass)
//      s    - second     (time)
//      A    - ampere     (electric current)
//      K    - kelvin     (thermodynamic temperature)
//      mol  - mole       (amount of substance)
//      cd   - candela    (luminous intensity)
//
//  All-zeros  →  dimensionless.
//  sizeof(UnitData) == 7 bytes (8 with alignment padding).
//
//  Arithmetic follows standard dimensional analysis:
//    multiply  →  exponents add
//    divide    →  exponents subtract
//    inverse   →  exponents negate
//    power     →  exponents × n
// =========================================================================

struct XDATASET_API UnitData
{
    int8_t m   = 0;
    int8_t kg  = 0;
    int8_t s   = 0;
    int8_t A   = 0;
    int8_t K   = 0;
    int8_t mol = 0;
    int8_t cd  = 0;

    UnitData();
    UnitData(int8_t m_, int8_t kg_, int8_t s_, int8_t A_,
             int8_t K_, int8_t mol_, int8_t cd_);

    // ---- arithmetic ----------------------------------------------------

    UnitData operator*(const UnitData& o) const;
    UnitData operator/(const UnitData& o) const;
    UnitData inv() const;
    UnitData pow(int n) const;

    // ---- queries -------------------------------------------------------

    bool empty() const;

    bool operator==(const UnitData& o) const;
    bool operator!=(const UnitData& o) const;

    // ---- serialisation -------------------------------------------------
    //
    //  Produces a canonical key string for reverse-lookup maps.
    //  Examples:
    //    {0,0,-1,0,0,0,0}   → "sec^-1"
    //    {2,1,-3,-1,0,0,0}  → "kg*meter^2*sec^-3*A^-1"
    //    {1,0,0,0,0,0,0}    → "meter"
    //    {0,0,0,0,0,0,0}    → ""   (dimensionless)

    std::string key() const;

    // ---- hash support --------------------------------------------------

    struct Hash
    {
        std::size_t operator()(const UnitData& d) const
        {
            // Pack 7 int8_t fields into a uint64_t for hashing.
            std::uint64_t val =
                (static_cast<std::uint64_t>(static_cast<std::uint8_t>(d.m))   << 48) |
                (static_cast<std::uint64_t>(static_cast<std::uint8_t>(d.kg))  << 40) |
                (static_cast<std::uint64_t>(static_cast<std::uint8_t>(d.s))   << 32) |
                (static_cast<std::uint64_t>(static_cast<std::uint8_t>(d.A))   << 24) |
                (static_cast<std::uint64_t>(static_cast<std::uint8_t>(d.K))   << 16) |
                (static_cast<std::uint64_t>(static_cast<std::uint8_t>(d.mol)) << 8)  |
                (static_cast<std::uint64_t>(static_cast<std::uint8_t>(d.cd))  << 0);
            // Simple multiplicative hash.
            return static_cast<std::size_t>(val ^ (val >> 32));
        }
    };
};

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
