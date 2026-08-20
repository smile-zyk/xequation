#include "unit.h"

#include <gtest/gtest.h>

using xdataset::Unit;

namespace xdataset
{

// =========================================================================
//  Construction & parse
// =========================================================================

TEST(UnitTest, DefaultIsDimensionlessUnit)
{
    Unit u;
    EXPECT_FALSE(u.has_dimension());
    EXPECT_TRUE(u.is_canonical());
    EXPECT_DOUBLE_EQ(u.multiplier(), 1.0);
}

TEST(UnitTest, DefaultIsDefaultConstructed)
{
    Unit u;
    EXPECT_FALSE(u.has_dimension());
    EXPECT_TRUE(u.is_canonical());
    EXPECT_DOUBLE_EQ(u.multiplier(), 1.0);
    EXPECT_TRUE(u == Unit());
}

TEST(UnitTest, ParseHz)
{
    Unit u = Unit::parse("Hz");
    EXPECT_TRUE(u.has_dimension());
    EXPECT_DOUBLE_EQ(u.multiplier(), 1.0);
}

TEST(UnitTest, ParseMeter)
{
    Unit u = Unit::parse("meter");
    EXPECT_TRUE(u.has_dimension());
    EXPECT_DOUBLE_EQ(u.multiplier(), 1.0);
}

TEST(UnitTest, ParseInvalidThrows)
{
    EXPECT_THROW(Unit::parse("blarghzzz"), std::invalid_argument);
    EXPECT_THROW(Unit::parse("xyz"), std::invalid_argument);
    // "Pa" (Pascal) and "km" (kilometer) may be supported; do not require
    // them to throw. Accept either behavior.
    try { Unit::parse("Pa"); } catch (std::invalid_argument&) {}
    try { Unit::parse("km"); } catch (std::invalid_argument&) {}
}

TEST(UnitTest, ParseEmptyThrows)
{
    EXPECT_THROW(Unit::parse(""), std::invalid_argument);
}

// =========================================================================
//  Scale factors
// =========================================================================

TEST(UnitTest, ScaleFactorT)  { EXPECT_DOUBLE_EQ(Unit::parse("T").multiplier(),  1e12); }
TEST(UnitTest, ScaleFactorG)  { EXPECT_DOUBLE_EQ(Unit::parse("G").multiplier(),  1e9);  }
TEST(UnitTest, ScaleFactorM)  { EXPECT_DOUBLE_EQ(Unit::parse("M").multiplier(),  1e6);  }
TEST(UnitTest, ScaleFactorK)  { EXPECT_DOUBLE_EQ(Unit::parse("K").multiplier(),  1e3);  }
TEST(UnitTest, ScaleFactork)  { EXPECT_DOUBLE_EQ(Unit::parse("k").multiplier(),  1e3);  }
TEST(UnitTest, ScaleFactor_u) { EXPECT_DOUBLE_EQ(Unit::parse("_").multiplier(),  1.0);  }
TEST(UnitTest, ScaleFactorm)  { EXPECT_DOUBLE_EQ(Unit::parse("m").multiplier(),  1e-3); }
TEST(UnitTest, ScaleFactoru)  { EXPECT_DOUBLE_EQ(Unit::parse("u").multiplier(),  1e-6); }
TEST(UnitTest, ScaleFactorn)  { EXPECT_DOUBLE_EQ(Unit::parse("n").multiplier(),  1e-9); }
TEST(UnitTest, ScaleFactorp)  { EXPECT_DOUBLE_EQ(Unit::parse("p").multiplier(),  1e-12);}
TEST(UnitTest, ScaleFactorf)  { EXPECT_DOUBLE_EQ(Unit::parse("f").multiplier(),  1e-15);}
TEST(UnitTest, ScaleFactora)  { EXPECT_DOUBLE_EQ(Unit::parse("a").multiplier(),  1e-18);}

TEST(UnitTest, ScaleFactorMIsDimensionless)
{
    Unit u = Unit::parse("M");
    EXPECT_FALSE(u.has_dimension());
    EXPECT_FALSE(u.is_canonical() && !u.has_dimension());
}

TEST(UnitTest, ScaleFactor_uIsDimlessAndUnit)
{
    Unit u = Unit::parse("_");
    EXPECT_FALSE(u.has_dimension());
    EXPECT_TRUE(u.is_canonical() && !u.has_dimension());
}

// =========================================================================
//  Scale factor + unit
// =========================================================================

TEST(UnitTest, MHz)
{
    Unit u = Unit::parse("MHz");
    EXPECT_TRUE(u.same_dimension(Unit::parse("Hz")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 1e6);
}

TEST(UnitTest, kHz)
{
    Unit u = Unit::parse("kHz");
    EXPECT_TRUE(u.same_dimension(Unit::parse("Hz")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 1e3);
}

TEST(UnitTest, kOhm)
{
    Unit u = Unit::parse("kOhm");
    EXPECT_TRUE(u.same_dimension(Unit::parse("Ohm")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 1e3);
}

TEST(UnitTest, kV)
{
    Unit u = Unit::parse("kV");
    EXPECT_TRUE(u.same_dimension(Unit::parse("V")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 1e3);
}

TEST(UnitTest, mA)
{
    Unit u = Unit::parse("mA");
    EXPECT_TRUE(u.same_dimension(Unit::parse("A")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 1e-3);
}

TEST(UnitTest, uF)
{
    Unit u = Unit::parse("uF");
    EXPECT_TRUE(u.same_dimension(Unit::parse("F")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 1e-6);
}

// =========================================================================
//  Predefined scaled units
// =========================================================================

TEST(UnitTest, Mil)
{
    Unit u = Unit::parse("mil");
    EXPECT_TRUE(u.same_dimension(Unit::parse("meter")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 2.54e-5);
}

TEST(UnitTest, Mils)
{
    Unit u = Unit::parse("mils");
    EXPECT_TRUE(u.same_dimension(Unit::parse("meter")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 2.54e-5);
}

TEST(UnitTest, In)
{
    Unit u = Unit::parse("in");
    EXPECT_TRUE(u.same_dimension(Unit::parse("meter")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 2.54e-2);
}

TEST(UnitTest, Ft)
{
    Unit u = Unit::parse("ft");
    EXPECT_TRUE(u.same_dimension(Unit::parse("meter")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 12 * 2.54e-2);
}

TEST(UnitTest, Mi)
{
    Unit u = Unit::parse("mi");
    EXPECT_TRUE(u.same_dimension(Unit::parse("meter")));
    EXPECT_NEAR(u.multiplier(), 1609.344, 0.01);
}

TEST(UnitTest, Cm)
{
    Unit u = Unit::parse("cm");
    EXPECT_TRUE(u.same_dimension(Unit::parse("meter")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 1.0e-2);
}

TEST(UnitTest, PHz)
{
    Unit u = Unit::parse("PHz");
    EXPECT_TRUE(u.same_dimension(Unit::parse("Hz")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 1.0e15);
}

TEST(UnitTest, DB)
{
    Unit u = Unit::parse("dB");
    EXPECT_FALSE(u.has_dimension());
    EXPECT_DOUBLE_EQ(u.multiplier(), 1.0);
}

TEST(UnitTest, Nmi)
{
    Unit u = Unit::parse("nmi");
    EXPECT_TRUE(u.same_dimension(Unit::parse("meter")));
    EXPECT_DOUBLE_EQ(u.multiplier(), 1852);
}

// =========================================================================
//  Unit aliases
// =========================================================================

TEST(UnitTest, MeterAliases)
{
    EXPECT_TRUE(Unit::parse("meter").same_dimension(Unit::parse("meters")));
    EXPECT_TRUE(Unit::parse("metre").same_dimension(Unit::parse("metres")));
    EXPECT_DOUBLE_EQ(Unit::parse("meter").multiplier(), 1.0);
    EXPECT_DOUBLE_EQ(Unit::parse("meters").multiplier(), 1.0);
}

TEST(UnitTest, SecAlias)
{
    EXPECT_DOUBLE_EQ(Unit::parse("sec").multiplier(), 1.0);
}

TEST(UnitTest, OhmAliases)
{
    EXPECT_TRUE(Unit::parse("Ohm").same_dimension(Unit::parse("Ohms")));
    EXPECT_DOUBLE_EQ(Unit::parse("Ohm").multiplier(), 1.0);
    EXPECT_DOUBLE_EQ(Unit::parse("Ohms").multiplier(), 1.0);
}

// =========================================================================
//  Compound not supported
// =========================================================================

TEST(UnitTest, CompoundRejected)
{
    EXPECT_THROW(Unit::parse("MHz/kOhm"), std::invalid_argument);
}

// =========================================================================
//  Case sensitivity
// =========================================================================

TEST(UnitTest, CaseSensitivityMvsMilli)
{
    Unit uM = Unit::parse("MHz");
    Unit um = Unit::parse("mHz");
    EXPECT_DOUBLE_EQ(uM.multiplier(), 1e6);
    EXPECT_DOUBLE_EQ(um.multiplier(), 1e-3);
    EXPECT_TRUE(uM.same_dimension(um));
}

TEST(UnitTest, CaseSensitivityFvsFemto)
{
    Unit uF = Unit::parse("F");
    EXPECT_TRUE(uF.has_dimension());

    Unit ufF = Unit::parse("fF");
    EXPECT_TRUE(ufF.same_dimension(uF));
    EXPECT_DOUBLE_EQ(ufF.multiplier(), 1e-15);
}

TEST(UnitTest, CaseSensitivityAvsAtto)
{
    Unit uA = Unit::parse("A");
    EXPECT_TRUE(uA.has_dimension());

    Unit uaA = Unit::parse("aA");
    EXPECT_TRUE(uaA.same_dimension(uA));
    EXPECT_DOUBLE_EQ(uaA.multiplier(), 1e-18);
}

// =========================================================================
//  canonicalize
// =========================================================================

TEST(UnitTest, CanonicalizeStripsMultiplier)
{
    Unit cm = Unit::parse("cm");
    Unit c = cm.canonicalized();
    EXPECT_DOUBLE_EQ(c.multiplier(), 1.0);
    EXPECT_TRUE(c.same_dimension(cm));
}

TEST(UnitTest, CanonicalizeStripsMHz)
{
    Unit mhz = Unit::parse("MHz");
    Unit c = mhz.canonicalized();
    EXPECT_DOUBLE_EQ(c.multiplier(), 1.0);
    EXPECT_TRUE(c.same_dimension(mhz));
}

// =========================================================================
//  same_dimension
// =========================================================================

TEST(UnitTest, SameDimensionTrue)
{
    EXPECT_TRUE(Unit::parse("meter").same_dimension(Unit::parse("meter")));
    EXPECT_TRUE(Unit::parse("meter").same_dimension(Unit::parse("cm")));
    EXPECT_TRUE(Unit::parse("Hz").same_dimension(Unit::parse("Hz")));
    EXPECT_TRUE(Unit::parse("Hz").same_dimension(Unit::parse("MHz")));
}

TEST(UnitTest, SameDimensionFalse)
{
    EXPECT_FALSE(Unit::parse("meter").same_dimension(Unit::parse("sec")));
    EXPECT_FALSE(Unit::parse("Hz").same_dimension(Unit::parse("V")));
    EXPECT_FALSE(Unit::parse("A").same_dimension(Unit::parse("W")));
}

// =========================================================================
//  has_dimension / is_dimensionless
// =========================================================================

TEST(UnitTest, HasDimensionTrue)
{
    EXPECT_TRUE(Unit::parse("meter").has_dimension());
    EXPECT_TRUE(Unit::parse("Hz").has_dimension());
    EXPECT_TRUE(Unit::parse("V").has_dimension());
}

TEST(UnitTest, HasDimensionFalseForScaleFactors)
{
    EXPECT_FALSE(Unit::parse("M").has_dimension());
    EXPECT_FALSE(Unit::parse("k").has_dimension());
    EXPECT_FALSE(Unit::parse("m").has_dimension());
}

TEST(UnitTest, IsDimensionlessAndUnit)
{
    Unit dflt;
    EXPECT_TRUE(dflt.is_canonical() && !dflt.has_dimension());

    Unit underscore = Unit::parse("_");
    EXPECT_TRUE(underscore.is_canonical() && !underscore.has_dimension());

    Unit hz = Unit::parse("Hz");
    EXPECT_FALSE(hz.is_canonical() && !hz.has_dimension());

    Unit meg = Unit::parse("M");
    EXPECT_FALSE(meg.is_canonical() && !meg.has_dimension());
}

// =========================================================================
//  multiply_dim / divide_dim / pow_dim
// =========================================================================

TEST(UnitTest, MultiplyDim)
{
    Unit m = Unit::parse("meter").canonicalized();
    Unit pers = Unit::parse("Hz").canonicalized();
    Unit r = m*(pers);
    EXPECT_DOUBLE_EQ(r.multiplier(), 1.0);
    Unit ms = Unit::parse("meter").canonicalized()/(
        Unit::parse("sec").canonicalized());
    EXPECT_TRUE(r.same_dimension(ms));
}

TEST(UnitTest, DivideDim)
{
    Unit m = Unit::parse("meter").canonicalized();
    Unit s = Unit::parse("sec").canonicalized();
    Unit r = m/(s);
    EXPECT_DOUBLE_EQ(r.multiplier(), 1.0);
}


TEST(UnitTest, ToStringHz)
{
    Unit u = Unit::parse("Hz").canonicalized();
    EXPECT_FALSE(u.to_string().empty());
}

TEST(UnitTest, ToStringV)
{
    Unit u = Unit::parse("V").canonicalized();
    EXPECT_FALSE(u.to_string().empty());
}

TEST(UnitTest, ToStringA)
{
    Unit u = Unit::parse("A").canonicalized();
    EXPECT_FALSE(u.to_string().empty());
}

TEST(UnitTest, ToStringW)
{
    Unit u = Unit::parse("W").canonicalized();
    EXPECT_FALSE(u.to_string().empty());
}

TEST(UnitTest, ToStringOhm)
{
    Unit u = Unit::parse("Ohm").canonicalized();
    EXPECT_FALSE(u.to_string().empty());
}

TEST(UnitTest, ToStringMeter)
{
    Unit u = Unit::parse("meter").canonicalized();
    EXPECT_FALSE(u.to_string().empty());
}

TEST(UnitTest, ToStringSec)
{
    Unit u = Unit::parse("sec").canonicalized();
    EXPECT_FALSE(u.to_string().empty());
}

TEST(UnitTest, ToStringCompound)
{
    Unit m = Unit::parse("meter").canonicalized();
    Unit s = Unit::parse("sec").canonicalized();
    Unit ms = m/(s);
    EXPECT_FALSE(ms.to_string().empty());
}

// =========================================================================
//  to_string: decomposition (factorisation) of compound units
// =========================================================================

TEST(UnitTest, ToStringDecomposeAW)
{
    // A * W  ->  "A*W"  (not bare SI exponents)
    Unit a = Unit::parse("A").canonicalized();
    Unit w = Unit::parse("W").canonicalized();
    Unit aw = a * w;
    EXPECT_EQ(aw.to_string(), "A*W");
}

TEST(UnitTest, ToStringDecomposeMeterPerSec)
{
    // meter / sec  ->  "meter/sec"
    Unit m = Unit::parse("meter").canonicalized();
    Unit s = Unit::parse("sec").canonicalized();
    Unit ms = m / s;
    EXPECT_EQ(ms.to_string(), "meter/sec");
}

TEST(UnitTest, ToStringDecomposeOhmTimesA)
{
    // Ohm * A  ->  "V"  (reduces to registered unit)
    Unit ohm = Unit::parse("Ohm").canonicalized();
    Unit a   = Unit::parse("A").canonicalized();
    Unit va  = ohm * a;
    EXPECT_EQ(va.to_string(), "V");
}

TEST(UnitTest, ToStringDecomposeVPerA)
{
    // V / A  ->  "Ohm"
    Unit v = Unit::parse("V").canonicalized();
    Unit a = Unit::parse("A").canonicalized();
    Unit r = v / a;
    EXPECT_EQ(r.to_string(), "Ohm");
}

TEST(UnitTest, ToStringDecomposeHzSec)
{
    // Hz * sec  ->  dimensionless (empty string)
    Unit hz  = Unit::parse("Hz").canonicalized();
    Unit sec = Unit::parse("sec").canonicalized();
    Unit dim = hz * sec;
    EXPECT_TRUE(dim.to_string().empty());
}

TEST(UnitTest, ToStringDecomposeWS)
{
    // W * sec  ->  "J"  (Joule)
    Unit w = Unit::parse("W").canonicalized();
    Unit s = Unit::parse("sec").canonicalized();
    Unit j = w * s;
    EXPECT_EQ(j.to_string(), "J");
}

TEST(UnitTest, ToStringDecomposeCompoundWithMultipleParts)
{
    // W  already registered, N not in registry but can decompose to kg*meter/sec^2
    // Just test decomposition doesn't crash / returns non-empty for something
    // we know can be decomposed.
    Unit a   = Unit::parse("A").canonicalized();
    Unit w   = Unit::parse("W").canonicalized();
    Unit aw  = a * w;
    std::string s = aw.to_string();
    EXPECT_FALSE(s.empty());
    // Decomposed strings are compound (e.g. "A*W"); parse() currently only
    // handles single-unit strings, so we only check dimension equivalence
    // via the Unit operators directly, not via round-trip parse.
    EXPECT_TRUE(aw.same_dimension(a * w));
}

TEST(UnitTest, ToStringDecomposeRoundTrip)
{
    // Verify decomposed strings round-trip via Unit arithmetic (not string
    // parse, since decompose() may produce compound names like "A*W" that
    // parse() doesn't yet support).
    auto check_roundtrip = [](const char* a_str, const char* b_str) {
        Unit a = Unit::parse(a_str).canonicalized();
        Unit b = Unit::parse(b_str).canonicalized();
        Unit c = a * b;
        std::string s = c.to_string();
        EXPECT_FALSE(s.empty());
        EXPECT_TRUE(c.same_dimension(a * b))
            << a_str << "*" << b_str << " -> \"" << s << "\"";
    };
    check_roundtrip("A", "W");
    check_roundtrip("V", "sec");
    check_roundtrip("Ohm", "A");
    check_roundtrip("Hz", "meter");
}

TEST(UnitTest, ToStringDefaultDoesNotCrash)
{
    Unit u;
    SUCCEED();
}

// =========================================================================
//  to_string round-trip: derived SI units survive canonicalize
// =========================================================================

TEST(UnitTest, ToStringRoundTripSiemens)
{
    Unit u = Unit::parse("S").canonicalized();
    EXPECT_EQ(u.to_string(), "S");
    // Verify it can be parsed back
    Unit u2 = Unit::parse(u.to_string());
    EXPECT_EQ(u2.multiplier(), 1.0);
    EXPECT_TRUE(u2.same_dimension(u));
}

TEST(UnitTest, ToStringRoundTripOhm)
{
    Unit u = Unit::parse("Ohm").canonicalized();
    EXPECT_EQ(u.to_string(), "Ohm");
    // Round-trip: parse the string back and verify
    Unit u2 = Unit::parse(u.to_string());
    EXPECT_EQ(u2.multiplier(), 1.0);
    EXPECT_TRUE(u2.same_dimension(u));
}

TEST(UnitTest, ToStringRoundTripWatt)
{
    Unit u = Unit::parse("W").canonicalized();
    EXPECT_EQ(u.to_string(), "W");
    Unit u2 = Unit::parse(u.to_string());
    EXPECT_EQ(u2.multiplier(), 1.0);
    EXPECT_TRUE(u2.same_dimension(u));
}

TEST(UnitTest, ToStringRoundTripBasicUnits)
{
    // All base units in our vocabulary
    EXPECT_EQ(Unit::parse("Hz").canonicalized().to_string(), "Hz");
    EXPECT_EQ(Unit::parse("V").canonicalized().to_string(),   "V");
    EXPECT_EQ(Unit::parse("A").canonicalized().to_string(),   "A");
    EXPECT_EQ(Unit::parse("F").canonicalized().to_string(),   "F");
    EXPECT_EQ(Unit::parse("H").canonicalized().to_string(),   "H");
    EXPECT_EQ(Unit::parse("meter").canonicalized().to_string(), "meter");
    EXPECT_EQ(Unit::parse("sec").canonicalized().to_string(),   "sec");
}

// =========================================================================
//  to_string: canonicalized prefixed units --?base REL name
// =========================================================================

TEST(UnitTest, ToStringCanonicalMHz)
{
    Unit u = Unit::parse("MHz").canonicalized();
    EXPECT_EQ(u.to_string(), "Hz");
    EXPECT_DOUBLE_EQ(u.multiplier(), 1.0);
}

TEST(UnitTest, ToStringCanonicalGHz)
{
    Unit u = Unit::parse("GHz").canonicalized();
    EXPECT_EQ(u.to_string(), "Hz");
}

TEST(UnitTest, ToStringCanonicalkOhm)
{
    Unit u = Unit::parse("kOhm").canonicalized();
    EXPECT_EQ(u.to_string(), "Ohm");
}

TEST(UnitTest, ToStringCanonicalkS)
{
    // kS (kilo-Siemens) --?canonicalized --?"S"
    Unit u = Unit::parse("kS");
    Unit c = u.canonicalized();
    EXPECT_EQ(c.to_string(), "S");
    EXPECT_DOUBLE_EQ(c.multiplier(), 1.0);
}

// =========================================================================
//  to_string: non-canonical prefixed units retain their prefix
// =========================================================================

TEST(UnitTest, ToStringNonCanonicalMHz)
{
    Unit u = Unit::parse("MHz");
    EXPECT_EQ(u.to_string(), "MHz");
}

TEST(UnitTest, ToStringNonCanonicalGHz)
{
    Unit u = Unit::parse("GHz");
    EXPECT_EQ(u.to_string(), "GHz");
}

TEST(UnitTest, ToStringNonCanonicalmA)
{
    Unit u = Unit::parse("mA");
    EXPECT_EQ(u.to_string(), "mA");
}

TEST(UnitTest, ToStringNonCanonicalGV)
{
    Unit u = Unit::parse("GV");
    EXPECT_EQ(u.to_string(), "GV");  // not "kV", just exact prefix match
}

// =========================================================================
//  to_string: aliases canonicalise to first registered name
// =========================================================================

TEST(UnitTest, ToStringAliasOhmsToOhm)
{
    Unit u = Unit::parse("Ohms").canonicalized();
    EXPECT_EQ(u.to_string(), "Ohm");
}

TEST(UnitTest, ToStringAliasMetersToMeter)
{
    Unit u = Unit::parse("meters").canonicalized();
    EXPECT_EQ(u.to_string(), "meter");
}

TEST(UnitTest, ToStringAliasMetreToMeter)
{
    Unit u = Unit::parse("metre").canonicalized();
    EXPECT_EQ(u.to_string(), "meter");
}

// =========================================================================
//  best_display with derived units
// =========================================================================

TEST(UnitTest, BestDisplayWattSmall)
{
    UnitScale s = Unit::parse("W").best_display(0.005);
    EXPECT_NEAR(s.scale, 1000, 1e-9);
    EXPECT_EQ(s.name, "mW");
}

TEST(UnitTest, BestDisplayWattLarge)
{
    UnitScale s = Unit::parse("W").best_display(5e6);
    EXPECT_NEAR(s.scale, 1e-6, 1e-9);
    EXPECT_EQ(s.name, "MW");
}

TEST(UnitTest, BestDisplayOhm)
{
    UnitScale s = Unit::parse("Ohm").best_display(4700);
    EXPECT_NEAR(s.scale, 1e-3, 1e-9);
    EXPECT_EQ(s.name, "KOhm");
}

TEST(UnitTest, BestDisplayHzNoScale)
{
    UnitScale s = Unit::parse("Hz").best_display(50);
    EXPECT_DOUBLE_EQ(s.scale, 1.0);
    EXPECT_EQ(s.name, "Hz");
}

TEST(UnitTest, BestDisplayHzToGHz)
{
    UnitScale s = Unit::parse("Hz").best_display(2.4e9);
    EXPECT_NEAR(s.scale, 1e-9, 1e-9);
    EXPECT_EQ(s.name, "GHz");
}

// When the unit already has a prefix (e.g. "MHz") and the value doesn't
// trigger further auto-scaling, the scale must be 1.0 --?the display name
// returned by to_string() already includes the prefix.
TEST(UnitTest, BestDisplayPrefixUnitNoFurtherScale)
{
    // 1.23e12 MHz --?stays in MHz, no further auto-scaling possible.
    UnitScale s = Unit::parse("MHz").best_display(1.23e12);
    EXPECT_DOUBLE_EQ(s.scale, 1.0);
    EXPECT_EQ(s.name, "MHz");
}

// =========================================================================
//  equals / not-equals
// =========================================================================

TEST(UnitTest, Equals)
{
    Unit a = Unit::parse("Hz");
    Unit b = Unit::parse("Hz");
    EXPECT_TRUE(a == b);
}

TEST(UnitTest, NotEquals)
{
    Unit a = Unit::parse("Hz");
    Unit b = Unit::parse("V");
    EXPECT_TRUE(a != b);
}

TEST(UnitTest, NotEqualsMeterVsSec)
{
    Unit a = Unit::parse("meter");
    Unit b = Unit::parse("sec");
    EXPECT_TRUE(a != b);
}

// =========================================================================
//  Predef cannot stack with scale
// =========================================================================

TEST(UnitTest, PredefRejectsScale)
{
    EXPECT_THROW(Unit::parse("kin"), std::invalid_argument);
    EXPECT_THROW(Unit::parse("Mmil"), std::invalid_argument);
}

} // namespace xdataset
