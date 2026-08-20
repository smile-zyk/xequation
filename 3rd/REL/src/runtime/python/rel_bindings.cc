// =============================================================================
//  rel_bindings.cc -- Value / Param / ComputedParam / register_function and the
//  Value<->Python conversion used by the callback shims.
// =============================================================================

#include "python_common.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <complex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "environment.h"
#include "data_array.h"
#include "measurement.h"
#include "unit.h"

namespace rel {
namespace python {
namespace {

using namespace xdataset;

/// A Python-side parameter descriptor (rel.Param / rel.ComputedParam).
struct PythonParam
{
    std::string name;
    bool has_default = false;
    rel::Value default_value;
    bool is_computed = false;
    pybind11::function computed;  // only when is_computed
};

// ---- list / tuple -> DataSeries (numeric, first-version) ---------------

DataSeries dataseries_from_sequence(pybind11::handle obj)
{
    pybind11::sequence seq = pybind11::cast<pybind11::sequence>(obj);
    const Index n = static_cast<Index>(pybind11::len(seq));
    if (n == 0)
        return DataSeries::CreateScalar<double>(0);

    bool is2d = pybind11::isinstance<pybind11::sequence>(seq[0]);
    if (is2d)
    {
        Index w = static_cast<Index>(pybind11::len(pybind11::cast<pybind11::sequence>(seq[0])));
        DataSeries ds = DataSeries::CreateVector<double>(w, static_cast<std::size_t>(n));
        for (Index i = 0; i < n; ++i)
        {
            pybind11::sequence row = pybind11::cast<pybind11::sequence>(seq[i]);
            for (Index j = 0; j < w; ++j)
                ds.vector_at<double>(i)(j) = pybind11::cast<double>(row[j]);
        }
        return ds;
    }

    DataSeries ds = DataSeries::CreateScalar<double>(static_cast<std::size_t>(n));
    for (Index i = 0; i < n; ++i)
        ds.scalar_at<double>(i) = pybind11::cast<double>(seq[i]);
    return ds;
}

}  // namespace

// =========================================================================
//  Value <-> Python conversion (shared with python_loader.cc)
// =========================================================================

pybind11::object to_python(const rel::Value& v)
{
    return pybind11::cast(v);
}

rel::Value from_python(pybind11::handle obj)
{
    // Passthrough of already-bound types.
    if (pybind11::isinstance<rel::Value>(obj))
        return pybind11::cast<rel::Value>(obj);
    if (pybind11::isinstance<Measurement>(obj))
        return rel::Value(pybind11::cast<Measurement>(obj));
    if (pybind11::isinstance<DataArray>(obj))
        return rel::Value(pybind11::cast<DataArray>(obj));

    // ndarray / buffer.
    if (pybind11::isinstance<pybind11::buffer>(obj))
    {
        pybind11::buffer_info info = pybind11::reinterpret_borrow<pybind11::buffer>(obj).request();
        if (info.ndim == 0)
        {
            if (info.format == "d")
                return rel::Value::Real(*static_cast<const double*>(info.ptr));
            if (info.format == "i")
                return rel::Value::Integer(*static_cast<const int*>(info.ptr));
            if (info.format == "Zd")
                return rel::Value::Complex(*static_cast<const std::complex<double>*>(info.ptr));
            throw pybind11::type_error("unsupported 0-d buffer dtype");
        }

        DataSeries ds = dataseries_from_buffer(obj);
        if (ds.size() == 1)
            return rel::Value(ds.measurement_at(0));
        return rel::Value(DataArray::CreateIndependent(std::move(ds)));
    }

    // Python scalars (order matters: bool before int, int before float).
    if (pybind11::isinstance<pybind11::bool_>(obj))
        return rel::Value::Boolean(pybind11::cast<bool>(obj));
    if (pybind11::isinstance<pybind11::int_>(obj))
    {
        long long v = pybind11::cast<long long>(obj);
        if (v < -2147483648LL || v > 2147483647LL)
            throw std::overflow_error("integer out of int32 range");
        return rel::Value::Integer(static_cast<int>(v));
    }
    if (pybind11::isinstance<pybind11::float_>(obj))
        return rel::Value::Real(pybind11::cast<double>(obj));
    if (PyComplex_Check(obj.ptr()))
        return rel::Value::Complex(pybind11::cast<std::complex<double>>(obj));
    if (pybind11::isinstance<pybind11::str>(obj))
        return rel::Value::String(pybind11::cast<std::string>(obj));

    // list / tuple of numbers.
    if (pybind11::isinstance<pybind11::sequence>(obj))
    {
        DataSeries ds = dataseries_from_sequence(obj);
        if (ds.size() == 1)
            return rel::Value(ds.measurement_at(0));
        return rel::Value(DataArray::CreateIndependent(std::move(ds)));
    }

    throw pybind11::type_error("cannot convert Python object to rel.Value");
}

// =========================================================================
//  register_rel_bindings
// =========================================================================

void register_rel_bindings(pybind11::module_& m)
{
    // ---- Value --------------------------------------------------------
    pybind11::class_<rel::Value>(m, "Value", pybind11::buffer_protocol())
        .def(pybind11::init<>())
        .def(pybind11::init<Measurement>())
        .def(pybind11::init<const DataArray&>())
        .def_static("real", &rel::Value::Real, pybind11::arg("value"), pybind11::arg("unit") = Unit())
        .def_static("integer", &rel::Value::Integer, pybind11::arg("value"), pybind11::arg("unit") = Unit())
        .def_static("complex", &rel::Value::Complex, pybind11::arg("value"), pybind11::arg("unit") = Unit())
        .def_static("string", &rel::Value::String)
        .def_static("boolean", &rel::Value::Boolean)
        .def_static("array_real", [](pybind11::sequence s, const Unit& u) {
            std::vector<double> v;
            for (auto item : s) v.push_back(pybind11::cast<double>(item));
            return rel::Value::ArrayReal(v, u);
        }, pybind11::arg("values"), pybind11::arg("unit") = Unit())
        .def_static("array_integer", [](pybind11::sequence s, const Unit& u) {
            std::vector<int> v;
            for (auto item : s) v.push_back(pybind11::cast<int>(item));
            return rel::Value::ArrayInteger(v, u);
        }, pybind11::arg("values"), pybind11::arg("unit") = Unit())
        .def_static("array_complex", [](pybind11::sequence s, const Unit& u) {
            std::vector<std::complex<double>> v;
            for (auto item : s) v.push_back(pybind11::cast<std::complex<double>>(item));
            return rel::Value::ArrayComplex(v, u);
        }, pybind11::arg("values"), pybind11::arg("unit") = Unit())
        .def_static("array_string", [](pybind11::sequence s) {
            std::vector<std::string> v;
            for (auto item : s) v.push_back(pybind11::cast<std::string>(item));
            return rel::Value::ArrayString(v);
        })
        // type queries
        .def("is_measurement", &rel::Value::is_measurement)
        .def("is_data_array", &rel::Value::is_data_array)
        .def("is_scalar", &rel::Value::is_scalar)
        .def("is_vector", &rel::Value::is_vector)
        .def("is_matrix", &rel::Value::is_matrix)
        .def("is_dependent", &rel::Value::is_dependent)
        .def("is_canonicalized", &rel::Value::is_canonicalized)
        // metadata
        .def_property_readonly("data_kind", [](const rel::Value& v) { return kind_str(v.data_kind()); })
        .def_property_readonly("data_type", [](const rel::Value& v) { return type_str(v.data_type()); })
        .def_property_readonly("unit", [](const rel::Value& v) { return v.unit(); })
        .def_property_readonly("rows", &rel::Value::rows)
        .def_property_readonly("indep_names", &rel::Value::indep_names)
        // downcast
        .def("as_measurement", [](const rel::Value& v) { return v.as_measurement(); })
        .def("as_data_array", [](const rel::Value& v) { return v.as_data_array(); })
        // data access
        .def("data", [](const rel::Value& v) { return v.data(); })
        .def("indep_data", [](const rel::Value& v, const std::string& name) { return v.indep_data(name); })
        .def("indep_data", [](const rel::Value& v, Index i) { return v.indep_data(i); })
        // in-place mutation
        .def("set_data", [](rel::Value& v, const Measurement& m) { v.set_data(m); })
        .def("set_data", [](rel::Value& v, const DataSeries& ds) { v.set_data(ds); })
        .def("set_data", [](rel::Value& v, Index row, const Measurement& m) { v.set_data(row, m); })
        .def("set_indep_data", [](rel::Value& v, const DataSeries& ds) { v.set_indep_data(ds); })
        .def("set_indep_data", [](rel::Value& v, Index idx, const DataSeries& ds) { v.set_indep_data(idx, ds); })
        .def("set_indep_data", [](rel::Value& v, const std::string& n, const DataSeries& ds) { v.set_indep_data(n, ds); })
        .def("set_indep_data", [](rel::Value& v, Index idx, Index row, const Measurement& m) { v.set_indep_data(idx, row, m); })
        .def("set_indep_data", [](rel::Value& v, const std::string& n, Index row, const Measurement& m) { v.set_indep_data(n, row, m); })
        // transform / display
        .def("canonicalized", &rel::Value::canonicalized)
        .def("clone", &rel::Value::clone)
        .def("format", &rel::Value::Format, pybind11::arg("name") = "data", pybind11::arg("max_rows") = 32)
        .def("to_string", &rel::Value::to_string)
        .def("data_frame", [](const rel::Value& v, const std::string& name) {
            return v.data_frame(name);
        }, pybind11::arg("name") = "data")
        .def("__str__", [](const rel::Value& v) { return v.to_string(); })
        .def("__repr__", [](const rel::Value& v) { return v.to_string(); })
        // ---- operators (delegate to rel::operation kernels) ------------
        // Parameters are pybind11::object so that plain Python numbers /
        // lists / strings are implicitly converted via from_python() (the
        // same conversion the plugin shims use).  This lets `v ** 2`,
        // `v + 1`, `1 + v` etc. work without pre-wrapping in rel.Value.
        .def("__add__",     [](pybind11::object a, pybind11::object b) { return from_python(a) + from_python(b); })
        .def("__sub__",     [](pybind11::object a, pybind11::object b) { return from_python(a) - from_python(b); })
        .def("__mul__",     [](pybind11::object a, pybind11::object b) { return from_python(a) * from_python(b); })
        .def("__truediv__", [](pybind11::object a, pybind11::object b) { return from_python(a) / from_python(b); })
        .def("__mod__",     [](pybind11::object a, pybind11::object b) { return from_python(a) % from_python(b); })
        .def("__pow__",     [](pybind11::object a, pybind11::object b) { return from_python(a).pow(from_python(b)); })
        .def("__eq__",      [](pybind11::object a, pybind11::object b) { return from_python(a) == from_python(b); })
        .def("__ne__",      [](pybind11::object a, pybind11::object b) { return from_python(a) != from_python(b); })
        .def("__lt__",      [](pybind11::object a, pybind11::object b) { return from_python(a) < from_python(b); })
        .def("__gt__",      [](pybind11::object a, pybind11::object b) { return from_python(a) > from_python(b); })
        .def("__le__",      [](pybind11::object a, pybind11::object b) { return from_python(a) <= from_python(b); })
        .def("__ge__",      [](pybind11::object a, pybind11::object b) { return from_python(a) >= from_python(b); })
        .def("__and__",     [](pybind11::object a, pybind11::object b) { return from_python(a) && from_python(b); })
        .def("__or__",      [](pybind11::object a, pybind11::object b) { return from_python(a) || from_python(b); })
        .def("__xor__",     [](pybind11::object a, pybind11::object b) { return from_python(a) ^ from_python(b); })
        .def("__lshift__",  [](pybind11::object a, pybind11::object b) { return from_python(a) << from_python(b); })
        .def("__rshift__",  [](pybind11::object a, pybind11::object b) { return from_python(a) >> from_python(b); })
        .def("__bitand__",  [](pybind11::object a, pybind11::object b) { return from_python(a) & from_python(b); })
        .def("__bitor__",   [](pybind11::object a, pybind11::object b) { return from_python(a) | from_python(b); })
        .def("__bitxor__",  [](pybind11::object a, pybind11::object b) { return from_python(a) ^ from_python(b); })
        .def("__neg__",     [](pybind11::object v) { return -from_python(v); })
        .def("__invert__",  [](pybind11::object v) { return ~from_python(v); })
        .def("__radd__",     [](pybind11::object a, pybind11::object b) { return from_python(b) + from_python(a); })
        .def("__rsub__",     [](pybind11::object a, pybind11::object b) { return from_python(b) - from_python(a); })
        .def("__rmul__",     [](pybind11::object a, pybind11::object b) { return from_python(b) * from_python(a); })
        .def("__rtruediv__", [](pybind11::object a, pybind11::object b) { return from_python(b) / from_python(a); })
        .def("__rmod__",     [](pybind11::object a, pybind11::object b) { return from_python(b) % from_python(a); })
        .def("__rpow__",     [](pybind11::object a, pybind11::object b) { return from_python(b).pow(from_python(a)); })
        .def("__rrshift__",  [](pybind11::object a, pybind11::object b) { return from_python(b) >> from_python(a); })
        .def("__rlshift__",  [](pybind11::object a, pybind11::object b) { return from_python(b) << from_python(a); })
        .def("__rand__",     [](pybind11::object a, pybind11::object b) { return from_python(b) && from_python(a); })
        .def("__ror__",      [](pybind11::object a, pybind11::object b) { return from_python(b) || from_python(a); })
        .def("__rxor__",     [](pybind11::object a, pybind11::object b) { return from_python(b) ^ from_python(a); })
        .def_buffer([](const rel::Value& v) -> pybind11::buffer_info {
            if (v.is_measurement())
                return make_measurement_buffer(v.as_measurement());
            return make_series_buffer(v.as_data_array().datas().rbegin()->second);
        })
        .def("__array__", [](const rel::Value& v, pybind11::args, pybind11::kwargs) -> pybind11::object {
            if (v.data_type() == DataType::kString)
            {
                if (v.is_measurement())
                    return numpy_from_py(string_measurement_to_py(v.as_measurement()));
                return numpy_from_py(string_series_to_py(v.as_data_array().datas().rbegin()->second));
            }
            throw pybind11::type_error("numeric data uses the buffer protocol");
        });

    // ---- Param / ComputedParam ----------------------------------------
    pybind11::class_<PythonParam>(m, "Param")
        .def(pybind11::init([](const std::string& name, pybind11::object default_) {
            PythonParam p;
            p.name = name;
            if (!default_.is_none())
            {
                p.has_default = true;
                p.default_value = from_python(default_);
            }
            return p;
        }), pybind11::arg("name"), pybind11::arg("default") = pybind11::none());

    m.def("ComputedParam", [](const std::string& name, pybind11::function fn) {
        PythonParam p;
        p.name = name;
        p.is_computed = true;
        p.computed = std::move(fn);
        return p;
    }, pybind11::arg("name"), pybind11::arg("fn"));

    // ---- register_function / unregister_function / function_names ------
    m.def("register_function",
        [](const std::string& name, pybind11::sequence params, pybind11::function fn) {
            std::vector<FunctionParam> fparams;
            for (auto item : params)
            {
                PythonParam p = pybind11::cast<PythonParam>(item);
                if (p.is_computed)
                {
                    std::string key = store_callback(p.computed);
                    fparams.push_back(FunctionParam(p.name, make_callback_shim(key)));
                }
                else if (p.has_default)
                {
                    fparams.push_back(FunctionParam(p.name, p.default_value));
                }
                else
                {
                    fparams.push_back(FunctionParam(p.name));
                }
            }
            register_python_function(name, std::move(fparams), std::move(fn));
        },
        pybind11::arg("name"), pybind11::arg("params"), pybind11::arg("fn"));

    m.def("unregister_function", [](const std::string& name) {
        return unregister_python_function(name);
    }, pybind11::arg("name"));

    m.def("function_names", []() {
        return Environment::FunctionNames();
    });
}

}  // namespace python
}  // namespace rel
