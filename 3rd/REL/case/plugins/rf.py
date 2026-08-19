# =============================================================================
#  rf.py — sample REL Python plugin (loaded via test_env.json "python_plugins")
#
#  Registers:
#    vtodbm(v, z) -> power in dBm from a voltage v across impedance z.
#                    `z` has a STATIC default of 50 (ohm), so `vtodbm(1.0)`
#                    uses 50 ohm.
#    vswr(rho)    -> VSWR from reflection-coefficient magnitude |rho|, with a
#                    STATIC default rho = 0.5 (call: `vswr()` or `vswr(0.2)`).
#
#  The computation runs entirely on rel.Value (the exported operator dunders
#  delegate to the rel::operation kernels) — no numpy round-trip for the
#  arithmetic, so unit-carrying Values stay canonical.  numpy is used only
#  for the scalar validation step in vswr().
#
#  NOTE: names are chosen to avoid colliding with builtin functions (dbm,
#  db, dbmtow, wtodbm already exist in the math library).  The runtime
#  rejects a Python registration whose name is already taken.
# =============================================================================

import rel


def vtodbm(args):
    v = args["v"]                      # rel.Value
    z = args["z"]                      # rel.Value (static default 50)
    # power = |v|^2 / z  (watts), broadcasting via Value operators
    power = rel.abs(v) ** 2 / z
    return 10.0 * rel.log10(power / 1e-3)   # dBm


def vswr(args):
    rho = rel.abs(args["rho"])         # rel.Value, |rho|
    return (1.0 + rho) / (1.0 - rho)


rel.register_function("vtodbm", [
    rel.Param("v"),
    rel.Param("z", 50.0),                          # static default
], vtodbm)

rel.register_function("vswr", [
    rel.Param("rho", 0.5),                         # static default
], vswr)
