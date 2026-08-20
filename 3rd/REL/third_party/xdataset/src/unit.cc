#include "unit.h"

#include <cmath>
#include <sstream>
#include <stdexcept>

#include "unit_registry.h"

namespace xdataset
{

// =========================================================================
//  UnitData implementation
// =========================================================================

UnitData::UnitData() {}

UnitData::UnitData(int8_t m_, int8_t kg_, int8_t s_, int8_t A_,
                   int8_t K_, int8_t mol_, int8_t cd_)
    : m(m_), kg(kg_), s(s_), A(A_), K(K_), mol(mol_), cd(cd_) {}

UnitData UnitData::operator*(const UnitData& o) const
{
    return {
        int8_t(m + o.m),
        int8_t(kg + o.kg),
        int8_t(s + o.s),
        int8_t(A + o.A),
        int8_t(K + o.K),
        int8_t(mol + o.mol),
        int8_t(cd + o.cd),
    };
}

UnitData UnitData::operator/(const UnitData& o) const
{
    return {
        int8_t(m - o.m),
        int8_t(kg - o.kg),
        int8_t(s - o.s),
        int8_t(A - o.A),
        int8_t(K - o.K),
        int8_t(mol - o.mol),
        int8_t(cd - o.cd),
    };
}

UnitData UnitData::inv() const
{
    return {
        int8_t(-m),
        int8_t(-kg),
        int8_t(-s),
        int8_t(-A),
        int8_t(-K),
        int8_t(-mol),
        int8_t(-cd),
    };
}

UnitData UnitData::pow(int n) const
{
    return {
        int8_t(m * n),
        int8_t(kg * n),
        int8_t(s * n),
        int8_t(A * n),
        int8_t(K * n),
        int8_t(mol * n),
        int8_t(cd * n),
    };
}

bool UnitData::empty() const
{
    return m == 0 && kg == 0 && s == 0 && A == 0
        && K == 0 && mol == 0 && cd == 0;
}

bool UnitData::operator==(const UnitData& o) const
{
    return m == o.m && kg == o.kg && s == o.s && A == o.A
        && K == o.K && mol == o.mol && cd == o.cd;
}

bool UnitData::operator!=(const UnitData& o) const { return !(*this == o); }

std::string UnitData::key() const
{
    struct Part { std::string name; int8_t exp; };
    const Part parts[] = {
        {"meter", m},
        {"kg",    kg},
        {"sec",   s},
        {"A",     A},
        {"K",     K},
        {"mol",   mol},
        {"cd",    cd},
    };
    std::string r;
    for (std::size_t i = 0; i < 7; ++i) {
        if (parts[i].exp == 0) continue;
        if (!r.empty()) r += '*';
        r += parts[i].name;
        if (parts[i].exp != 1) {
            r += '^';
            r += std::to_string(static_cast<int>(parts[i].exp));
        }
    }
    return r;
}

// =========================================================================
//  Construction
// =========================================================================

Unit::Unit() : mult_(1.0), dim_() {}

Unit::Unit(double mult, UnitData dim) : mult_(mult), dim_(dim) {}

// =========================================================================
//  Parse
// =========================================================================

Unit Unit::parse(const std::string& s)
{
    if (s.empty())
        throw std::invalid_argument("Empty unit string");

    UnitRegistry& reg = UnitRegistry::Instance();

    // 1) Greedy scale-prefix stripping (must run before type B to
    //    let "T"/"G"/"K" match as prefixes, not Tesla/Gauss/Kelvin).
    UnitRegistry::ScalePrefixMatch m = reg.try_strip_scale_prefix(s);
    if (m.found) {
        // remainder empty -> pure scale factor (e.g. "M", "k")
        if (m.remainder.empty())
            return Unit(m.factor, UnitData());

        // remainder in type A?
        const UnitData* dim = reg.lookup_base(m.remainder);
        if (dim)
            return Unit(m.factor, *dim);
    }

    // 2) Type B: predef exact match?
    const UnitRegistry::PredefEntry* predef = reg.lookup_predef(s);
    if (predef)
        return Unit(predef->mult, predef->dim);

    // 3) No prefix -> overall lookup in type A
    const UnitData* dim = reg.lookup_base(s);
    if (dim)
        return Unit(1.0, *dim);

    throw std::invalid_argument("Unrecognised unit string: '" + s + "'");
}

// =========================================================================
//  Queries
// =========================================================================

double Unit::multiplier() const { return mult_; }

bool Unit::is_canonical() const
{
    return mult_ == 1.0;
}

Unit Unit::canonicalized() const
{
    return Unit(1.0, dim_);
}

bool Unit::same_dimension(const Unit& other) const
{
    return dim_ == other.dim_;
}

bool Unit::has_dimension() const
{
    return !dim_.empty();
}

// =========================================================================
//  to_string
// =========================================================================

std::string Unit::to_string() const
{
    UnitRegistry& reg = UnitRegistry::Instance();
    Unit canonical = canonicalized();

    // Step 1: pure scale prefix (mult != 1, dim empty)?
    if (canonical.dim_.empty() && mult_ != 1.0) {
        const std::map<std::string, double>& scales = reg.scale_prefixes();
        for (std::map<std::string, double>::const_iterator it = scales.begin();
             it != scales.end(); ++it) {
            if (it->second == mult_) return it->first;
        }
        std::ostringstream oss;
        oss << mult_;
        return oss.str();
    }

    // Step 2: dimensionless canonical -> empty string
    if (canonical.dim_.empty()) return std::string();

    // Step 3: reverse lookup the canonical dim
    const std::string* base_name = reg.reverse_lookup(canonical.dim_);

    // Step 3b: if no exact match, try factorisation (e.g. A*W)
    std::string decomposed;
    if (!base_name)
        decomposed = reg.decompose(canonical.dim_);

    // Step 4: canonical -> just return the name, decomposition, or key
    if (mult_ == 1.0) {
        if (base_name) return *base_name;
        if (!decomposed.empty()) return decomposed;
        return canonical.dim_.key();
    }

    // Step 5: match multiplier to a scale prefix
    const std::map<std::string, double>& scales = reg.scale_prefixes();
    for (std::map<std::string, double>::const_iterator it = scales.begin();
         it != scales.end(); ++it) {
        if (it->second == mult_) {
            if (base_name)
                return it->first + *base_name;
            if (!decomposed.empty())
                return it->first + "(" + decomposed + ")";
            std::ostringstream oss;
            oss << mult_ << '*' << canonical.dim_.key();
            return oss.str();
        }
    }

    // Step 6: try type-B reverse lookup ({mult, dim} -> name)
    const std::string* predef_name =
        reg.reverse_predef_lookup(mult_, canonical.dim_);
    if (predef_name) return *predef_name;

    // Step 7: fallback
    std::string dim_str = canonical.dim_.key();
    if (dim_str.empty()) {
        std::ostringstream oss;
        oss << mult_;
        return oss.str();
    }
    std::ostringstream oss;
    oss << mult_ << '*' << dim_str;
    return oss.str();
}

// =========================================================================
//  best_display
// =========================================================================

UnitScale Unit::best_display(double value) const
{
    double mult = multiplier();
    Unit base_u = canonicalized();
    std::string base_str = base_u.to_string();

    // Don't auto-scale compound units (e.g. "m/s", "m^2").
    if (base_str.find_first_of("/*^") != std::string::npos)
        return {mult, base_str};

    UnitRegistry& reg = UnitRegistry::Instance();
    const std::map<std::string, double>& scales = reg.scale_prefixes();

    // Walk from largest to smallest factor to find the best display scale.
    double best_mult = 1.0;
    for (std::map<std::string, double>::const_reverse_iterator it = scales.rbegin();
         it != scales.rend(); ++it) {
        double m = it->second;
        if (m <= 0) continue;
        double absv = std::abs(value * mult / m);
        if (absv >= 1.0 && absv < 1000.0) {
            best_mult = m;
            break;
        }
    }

    if (best_mult == 1.0)
        return {1.0, to_string()};

    std::string display_unit = base_str;
    for (std::map<std::string, double>::const_iterator it = scales.begin();
         it != scales.end(); ++it) {
        if (it->second == best_mult) {
            display_unit = it->first + base_str;
            break;
        }
    }
    return {mult / best_mult, display_unit};
}

// =========================================================================
//  Arithmetic on dimensions (multiplier ignored, always 1.0)
// =========================================================================

Unit Unit::operator*(const Unit& other) const
{
    return Unit(1.0, dim_ * other.dim_);
}

Unit Unit::operator/(const Unit& other) const
{
    return Unit(1.0, dim_ / other.dim_);
}

Unit Unit::pow(int n) const
{
    return Unit(1.0, dim_.pow(n));
}

// =========================================================================
//  Comparison
// =========================================================================

bool Unit::operator==(const Unit& other) const
{
    return mult_ == other.mult_ && dim_ == other.dim_;
}

bool Unit::operator!=(const Unit& other) const
{
    return !(*this == other);
}

} // namespace xdataset
