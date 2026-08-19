#ifndef XDATASET_UNIT_DATA_H
#define XDATASET_UNIT_DATA_H

#include <cstddef>
#include <cstdint>
#include <string>

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

struct UnitData
{
    int8_t m   = 0;
    int8_t kg  = 0;
    int8_t s   = 0;
    int8_t A   = 0;
    int8_t K   = 0;
    int8_t mol = 0;
    int8_t cd  = 0;

    UnitData() {}

    UnitData(int8_t m_, int8_t kg_, int8_t s_, int8_t A_,
             int8_t K_, int8_t mol_, int8_t cd_)
        : m(m_), kg(kg_), s(s_), A(A_), K(K_), mol(mol_), cd(cd_) {}

    // ---- arithmetic ----------------------------------------------------

    UnitData operator*(const UnitData& o) const
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

    UnitData operator/(const UnitData& o) const
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

    UnitData inv() const
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

    UnitData pow(int n) const
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

    // ---- queries -------------------------------------------------------

    bool empty() const
    {
        return m == 0 && kg == 0 && s == 0 && A == 0
            && K == 0 && mol == 0 && cd == 0;
    }

    bool operator==(const UnitData& o) const
    {
        return m == o.m && kg == o.kg && s == o.s && A == o.A
            && K == o.K && mol == o.mol && cd == o.cd;
    }

    bool operator!=(const UnitData& o) const { return !(*this == o); }

    // ---- serialisation -------------------------------------------------
    //
    //  Produces a canonical key string for reverse-lookup maps.
    //  Examples:
    //    {0,0,-1,0,0,0,0}   → "sec^-1"
    //    {2,1,-3,-1,0,0,0}  → "kg*meter^2*sec^-3*A^-1"
    //    {1,0,0,0,0,0,0}    → "meter"
    //    {0,0,0,0,0,0,0}    → ""   (dimensionless)

    std::string key() const
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

}  // namespace xdataset

#endif  // XDATASET_UNIT_DATA_H
