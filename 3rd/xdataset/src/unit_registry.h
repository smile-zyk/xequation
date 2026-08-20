#ifndef XDATASET_UNIT_REGISTRY_H
#define XDATASET_UNIT_REGISTRY_H

#include <map>
#include <string>

#include "unit.h"

namespace xdataset
{

// =========================================================================
//  UnitRegistry -- two-tier unit name registry + scale-prefix queries
// =========================================================================
//
//  Type A  (register_base / register_alias):
//    Register a base unit with its 7-SI-exponent vector.
//    All scale-prefix combinations are automatically supported:
//      register_base("Hz", {s^-1})  ->  Hz, kHz, MHz, GHz, mHz, ...
//
//  Type B  (register_predef):
//    Register a unit with a *fixed* multiplier.  Scale prefixes are
//    rejected for these entries:
//      register_predef("ft", 0.3048, {m})  ->  "ft" works, "kft" throws.
//
//  Reverse-lookup supports to_string() round-tripping:
//    canonical dim  ->  type-A name
//    {mult, dim}    ->  type-B name
// =========================================================================

class UnitRegistry
{
public:
    static UnitRegistry& Instance();

    struct PredefEntry
    {
        double   mult;
        UnitData dim;
    };

    // ---- type A: scalable base units -----------------------------------

    /// Register a base unit name for a given 7-SI dimension vector.
    /// After this call all scale-prefixed forms are automatically
    /// parseable:  "Hz", "kHz", "MHz", "GHz", ...
    void register_base(const std::string& name, const UnitData& dim);

    /// Register an alias for a previously registered base unit.
    void register_alias(const std::string& alias,
                        const std::string& base_name);

    // ---- type B: pre-defined, non-scalable units -----------------------

    /// Register a unit with a fixed multiplier.
    /// These units *cannot* be combined with a scale prefix.
    void register_predef(const std::string& name,
                         double mult,
                         const UnitData& dim);

    // ---- forward lookups (parse-time) ----------------------------------

    /// Look up a base unit name -> its UnitData.  Returns nullptr if unknown.
    const UnitData* lookup_base(const std::string& name) const;

    /// Look up a pre-defined unit name -> {mult, dim}.  Returns nullptr if unknown.
    const PredefEntry* lookup_predef(const std::string& name) const;

    // ---- reverse lookups (to_string-time) ------------------------------

    /// Given a canonical (multiplier-free) dimension, return the type-A
    /// base name, or nullptr if not registered.
    const std::string* reverse_lookup(const UnitData& dim) const;

    /// Decompose a compound dimension into a product / quotient of
    /// registered base-unit names.  Returns "" when no decomposition
    /// is possible (caller falls back to UnitData::key()).
    ///
    /// Examples:
    ///   {m:2,kg:1,s:-3,A:1}  -> "A*W"      (A * Watt)
    ///   {m:1,s:-1}           -> "m/sec"    (meter / second)
    std::string decompose(const UnitData& dim) const;

    /// Given a multiplier + dimension, return the type-B predef name,
    /// or nullptr if not registered.
    const std::string* reverse_predef_lookup(double mult,
                                             const UnitData& dim) const;

    // ---- scale-prefix queries (used by Unit::parse / to_string) --------

    /// Result of try_strip_scale_prefix.
    struct ScalePrefixMatch
    {
        bool        found;      ///< true if a prefix was matched
        double      factor;     ///< numeric multiplier (1e3, 1e-6, ...)
        std::string remainder;  ///< rest of the string after the prefix
    };

    /// Try to strip a known scale prefix from `s`.
    /// Returns {true, factor, "Hz"} for "MHz", {false,_,_} on no match.
    ScalePrefixMatch try_strip_scale_prefix(const std::string& s) const;

    // ---- scale-prefix table (used by Unit::to_string / best_display) ----

    /// Map of scale-prefix name -> numeric factor.
    const std::map<std::string, double>& scale_prefixes() const;

private:
    UnitRegistry();

    // type A
    std::map<std::string, UnitData>               base_map_;
    std::map<std::string, std::string>            alias_map_;

    // type B
    std::map<std::string, PredefEntry>            predef_map_;

    // scale prefixes: name -> factor  (populated in constructor)
    std::map<std::string, double>                 scale_map_;                          

    // reverse: dim.key() -> type-A canonical name
    // Populated lazily on first reverse_lookup() call.
    mutable std::map<std::string, std::string>    reverse_map_;
    mutable bool                                  reverse_built_ = false;
    void build_reverse_map() const;
};
}  // namespace xdataset

#endif  // XDATASET_UNIT_REGISTRY_H
