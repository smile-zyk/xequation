# =============================================================================
#  snr.py — sample REL Python plugin (loaded via test_env.json "python_plugins")
#
#  Registers:
#    snr(signal, noise)  -> 20*log10(|signal| / |noise|)   [dB]
#
#  Works element-wise on scalars, vectors and matrices (numpy interop).
# =============================================================================

import numpy as np
import rel


def snr(args):
    signal = np.asarray(args["signal"])
    noise = np.asarray(args["noise"])
    return 20.0 * np.log10(np.abs(signal) / np.abs(noise))


rel.register_function("snr", [
    rel.Param("signal"),
    rel.Param("noise"),
], snr)
