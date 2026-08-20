#pragma once

// =============================================================================
//  Shared declarations for the REL Python bridge.
//
//  This header is only compiled when BUILD_PYTHON=ON (see CMakeLists.txt), so
//  it may freely include pybind11.  It declares the pieces that are defined
//  across the four python/*.cc translation units and consumed by each other:
//
//    - register_xdataset_bindings()  (xdataset_bindings.cc)
//    - register_rel_bindings()       (rel_bindings.cc)
//    - to_python() / from_python()   (rel_bindings.cc) -- the Value<->Python
//      conversion used by the callback shims in python_loader.cc
// =============================================================================

#include <pybind11/pybind11.h>

#include <string>

#include "function.h"
#include "value.h"

namespace rel {
namespace python {

// ---- small helpers shared across translation units ---------------------

/// Render DataKind / DataType as the Python-visible lowercase strings.
const char* kind_str(xdataset::DataKind k);
const char* type_str(xdataset::DataType t);

/// Buffer protocol exporters (zero-copy for real/integer/complex).
pybind11::buffer_info make_series_buffer(const xdataset::DataSeries& ds);
pybind11::buffer_info make_measurement_buffer(const xdataset::Measurement& m);

/// ndarray / list -> DataSeries (buffer protocol entry point).
xdataset::DataSeries dataseries_from_buffer(pybind11::handle obj);

/// Python object -> Measurement (scalar literal, buffer, or nested list).
xdataset::Measurement measurement_from_py(pybind11::handle obj,
                                          const xdataset::Unit& unit);

/// String-typed DataSeries / Measurement -> Python list (copy).  Strings are
/// variable-length, so they cannot be zero-copy exported via the buffer
/// protocol; `__array__` returns these and lets numpy build a unicode/object
/// array.
pybind11::object string_series_to_py(const xdataset::DataSeries& ds);
pybind11::object string_measurement_to_py(const xdataset::Measurement& m);

/// Wrap a Python list / str into a numpy array.  numpy 2.x requires `__array__`
/// to return an actual ndarray (a bare list/str is rejected), so string-typed
/// data goes through np.array(...) here while numeric data keeps the zero-copy
/// buffer protocol.
pybind11::object numpy_from_py(pybind11::object list_or_str);

// ---- binding registration (the single embedded `rel` module) ------------

/// Register the C++ exception -> Python exception translators.  Idempotent.
void register_exception_translators();

/// Register Unit, Measurement, DataSeries, DataArray, Block, Dataset, etc.
void register_xdataset_bindings(pybind11::module_& m);

/// Register Value, Param/ComputedParam, register_function, and conversions.
void register_rel_bindings(pybind11::module_& m);

// ---- Value <-> Python conversion ---------------------------------------

/// Convert a rel::Value to a Python object (a rel.Value instance).
pybind11::object to_python(const rel::Value& v);

/// Convert a Python object to a rel::Value using the sec.8.3 conversion rules.
/// Throws pybind11::type_error when the object cannot be converted.
rel::Value from_python(pybind11::handle obj);

// ---- callback registry (implemented in python_loader.cc) ---------------

/// Store a Python callable under a freshly generated unique key and return
/// that key.  The callable is owned by the GIL-protected registry.
std::string store_callback(pybind11::function fn);

/// Build a NativeFunction shim that looks up `key` in the callback registry
/// and invokes it (acquiring the GIL).  Used both for registered function
/// bodies and computed parameter defaults.
NativeFunction make_callback_shim(const std::string& key);

/// Register a Python callable as a named REL function: stores `fn` in the
/// GIL-protected registry and registers a pure-C++ shim into Environment.
/// The shim captures only the lookup key (a std::string), never a pybind11::function.
void register_python_function(const std::string& name,
                              std::vector<FunctionParam> params,
                              pybind11::function fn);

/// Unregister a Python-registered function by name (removes the body callback
/// and the Environment entry).  Returns true when a Python function of that
/// name existed.
bool unregister_python_function(const std::string& name);

/// Drop every Python plugin state while the interpreter is still alive:
/// unregister Python-registered functions and clear the callback registry
/// (under the GIL).  Does NOT finalize the interpreter -- that is owned by
/// python_manager (python_manager::PyEnvManager::ShutdownPyEnv).
void CleanupPythonState();

}  // namespace python
}  // namespace rel
