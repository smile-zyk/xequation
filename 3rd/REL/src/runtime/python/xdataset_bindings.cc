// =============================================================================
//  xdataset_bindings.cc — Unit / Measurement / DataSeries / DataArray /
//  Block / Dataset bindings for the embedded `rel` module.
//
//  numpy interop is implemented via the buffer protocol (def_buffer), so
//  `np.asarray(x)` works without requiring numpy headers at compile time.
//  Zero-copy is used for real / integer / complex data; string and boolean
//  buffers are not exposed (raise TypeError), matching PYTHON.md §5.1.
// =============================================================================

#include "python_common.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <complex>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "block.h"
#include "data_array.h"
#include "data_frame.h"
#include "data_series.h"
#include "dataset.h"
#include "dimension_spec.h"
#include "measurement.h"
#include "multi_dimension_spec.h"
#include "multi_index_selector.h"
#include "unit.h"

namespace rel {
namespace python {

using namespace xdataset;

// ---- small string helpers ----------------------------------------------

const char* kind_str(DataKind k)
{
    switch (k)
    {
        case DataKind::kScalar:  return "scalar";
        case DataKind::kVector:  return "vector";
        case DataKind::kMatrix:  return "matrix";
    }
    return "scalar";
}

const char* type_str(DataType t)
{
    switch (t)
    {
        case DataType::kReal:    return "real";
        case DataType::kInteger: return "integer";
        case DataType::kComplex: return "complex";
        case DataType::kString:  return "string";
        case DataType::kBoolean: return "boolean";
    }
    return "real";
}

namespace {

// ---- buffer protocol (numpy interop, no numpy headers) -----------------

/// Format string for the given numeric dtype (numpy-compatible).
std::string buffer_format(DataType t, ssize_t& itemsize)
{
    switch (t)
    {
        case DataType::kReal:
            itemsize = sizeof(double);
            return "d";
        case DataType::kInteger:
            itemsize = sizeof(int);
            return "i";
        case DataType::kComplex:
            itemsize = sizeof(std::complex<double>);
            return "Zd";
        default:
            throw pybind11::type_error(
                "only real / integer / complex data is buffer-exportable");
    }
}

const void* series_ptr(const DataSeries& ds)
{
    switch (ds.data_type())
    {
        case DataType::kReal:    return ds.contiguous_data<double>();
        case DataType::kInteger: return ds.contiguous_data<int>();
        case DataType::kComplex: return ds.contiguous_data<std::complex<double>>();
        default: break;
    }
    throw pybind11::type_error("unsupported dtype for buffer export");
}

}  // anonymous namespace

// These are shared with rel_bindings.cc (declared in python_common.h).

pybind11::buffer_info make_series_buffer(const DataSeries& ds)
{
    ssize_t itemsize = 0;
    std::string format = buffer_format(ds.data_type(), itemsize);
    const void* ptr = series_ptr(ds);

    const DataShape shape = ds.data_shape();
    const Index n = static_cast<Index>(ds.size());
    std::vector<ssize_t> shape_v, strides;

    switch (ds.data_kind())
    {
        case DataKind::kScalar:
            shape_v = { n };
            strides = { itemsize };
            break;
        case DataKind::kVector:
        {
            Index w = shape[0];
            shape_v = { n, w };
            strides = { w * itemsize, itemsize };
            break;
        }
        case DataKind::kMatrix:
        {
            Index r = shape[0], c = shape[1];
            shape_v = { n, r, c };
            strides = { r * c * itemsize, c * itemsize, itemsize };
            break;
        }
    }

    return pybind11::buffer_info(const_cast<void*>(ptr), itemsize, format,
                           static_cast<ssize_t>(shape_v.size()),
                           shape_v, strides, /*readonly=*/true);
}

pybind11::buffer_info make_measurement_buffer(const Measurement& m)
{
    // scalar boolean: exported as a 1-byte numpy bool (0-d).
    if (m.data_type() == DataType::kBoolean)
    {
        const bool* p = &boost::get<bool>(m.storage());
        std::vector<ssize_t> empty_shape, empty_strides;
        return pybind11::buffer_info(const_cast<bool*>(p), sizeof(bool), "?",
                               0, empty_shape, empty_strides, /*readonly=*/true);
    }

    ssize_t itemsize = 0;
    std::string format = buffer_format(m.data_type(), itemsize);

    const void* ptr = nullptr;
    switch (m.data_kind())
    {
        case DataKind::kScalar:
            switch (m.data_type())
            {
                case DataType::kReal:    ptr = &boost::get<double>(m.storage()); break;
                case DataType::kInteger: ptr = &boost::get<int>(m.storage()); break;
                case DataType::kComplex: ptr = &boost::get<std::complex<double>>(m.storage()); break;
                default: break;
            }
            break;
        case DataKind::kVector:
            switch (m.data_type())
            {
                case DataType::kReal:    ptr = m.as_vector<double>().data(); break;
                case DataType::kInteger: ptr = m.as_vector<int>().data(); break;
                case DataType::kComplex: ptr = m.as_vector<std::complex<double>>().data(); break;
                default: break;
            }
            break;
        case DataKind::kMatrix:
            switch (m.data_type())
            {
                case DataType::kReal:    ptr = m.as_matrix<double>().data(); break;
                case DataType::kInteger: ptr = m.as_matrix<int>().data(); break;
                case DataType::kComplex: ptr = m.as_matrix<std::complex<double>>().data(); break;
                default: break;
            }
            break;
    }

    if (!ptr)
        throw pybind11::type_error("unsupported Measurement dtype for buffer export");

    const DataShape shape = m.shape();
    std::vector<ssize_t> shape_v, strides;

    // A Measurement is a SINGLE cell; export it as one row so the first
    // buffer dimension is always the row count (same convention as
    // make_series_buffer).  This keeps the shape self-describing:
    //   scalar cell -> 0-d
    //   vector cell -> (1, w)
    //   matrix cell -> (1, r, c)
    // so a vector cell round-tripping through numpy (1, w) is NOT confused
    // with an N-row scalar series (n,).
    switch (m.data_kind())
    {
        case DataKind::kScalar:
            break;  // 0-d
        case DataKind::kVector:
        {
            Index w = shape[0];
            shape_v = { 1, w };
            strides = { w * itemsize, itemsize };
            break;
        }
        case DataKind::kMatrix:
        {
            Index r = shape[0], c = shape[1];
            shape_v = { 1, r, c };
            strides = { r * c * itemsize, c * itemsize, itemsize };
            break;
        }
    }

    return pybind11::buffer_info(const_cast<void*>(ptr), itemsize, format,
                           static_cast<ssize_t>(shape_v.size()),
                           shape_v, strides, /*readonly=*/true);
}

// ---- ndarray / list -> DataSeries / Measurement ------------------------

/// Read a buffer (numpy array, memoryview, ...) into a DataSeries.  The
/// buffer protocol is the "唯一入口" (only entry point) from numpy.
DataSeries dataseries_from_buffer(pybind11::handle obj)
{
    pybind11::buffer buf = pybind11::reinterpret_borrow<pybind11::buffer>(obj);
    pybind11::buffer_info info = buf.request();

    const std::string fmt = info.format;
    const ssize_t ndim = info.ndim;

    auto dispatch = [&](DataKind kind) -> DataSeries
    {
        if (kind == DataKind::kScalar)
        {
            // 1-d -> scalar series (N rows)
            const Index n = info.shape.empty() ? 0 : static_cast<Index>(info.shape[0]);
            if (fmt == "d")
                return DataSeries::CreateScalarFromMemory<double>(
                    static_cast<const double*>(info.ptr), static_cast<std::size_t>(n));
            if (fmt == "i")
                return DataSeries::CreateScalarFromMemory<int>(
                    static_cast<const int*>(info.ptr), static_cast<std::size_t>(n));
            if (fmt == "Zd")
                return DataSeries::CreateScalarFromMemory<std::complex<double>>(
                    static_cast<const std::complex<double>*>(info.ptr), static_cast<std::size_t>(n));
            if (fmt == "?")
            {
                // boolean: copy into an int series (0/1)
                DataSeries s = DataSeries::CreateScalar<int>(static_cast<std::size_t>(n));
                const bool* b = static_cast<const bool*>(info.ptr);
                for (Index i = 0; i < n; ++i) s.scalar_at<int>(i) = b[i] ? 1 : 0;
                return s;
            }
            throw pybind11::type_error("unsupported buffer format: " + fmt);
        }
        if (kind == DataKind::kVector)
        {
            const Index n = static_cast<Index>(info.shape[0]);
            const Index w = static_cast<Index>(info.shape[1]);
            if (fmt == "d")
                return DataSeries::CreateVectorFromMemory<double>(w,
                    static_cast<const double*>(info.ptr), static_cast<std::size_t>(n * w));
            if (fmt == "i")
                return DataSeries::CreateVectorFromMemory<int>(w,
                    static_cast<const int*>(info.ptr), static_cast<std::size_t>(n * w));
            if (fmt == "Zd")
                return DataSeries::CreateVectorFromMemory<std::complex<double>>(w,
                    static_cast<const std::complex<double>*>(info.ptr), static_cast<std::size_t>(n * w));
            throw pybind11::type_error("unsupported buffer format: " + fmt);
        }
        // matrix
        const Index n = static_cast<Index>(info.shape[0]);
        const Index r = static_cast<Index>(info.shape[1]);
        const Index c = static_cast<Index>(info.shape[2]);
        if (fmt == "d")
            return DataSeries::CreateMatrixFromMemory<double>(r, c,
                static_cast<const double*>(info.ptr), static_cast<std::size_t>(n * r * c));
        if (fmt == "i")
            return DataSeries::CreateMatrixFromMemory<int>(r, c,
                static_cast<const int*>(info.ptr), static_cast<std::size_t>(n * r * c));
        if (fmt == "Zd")
            return DataSeries::CreateMatrixFromMemory<std::complex<double>>(r, c,
                static_cast<const std::complex<double>*>(info.ptr), static_cast<std::size_t>(n * r * c));
        throw pybind11::type_error("unsupported buffer format: " + fmt);
    };

    if (ndim == 1)
        return dispatch(DataKind::kScalar);
    if (ndim == 2)
        return dispatch(DataKind::kVector);
    if (ndim == 3)
        return dispatch(DataKind::kMatrix);
    throw pybind11::type_error(
        "from_array expects a 1-d, 2-d or 3-d numeric array");
}

/// Convert a Python object into a Measurement (scalar literal, buffer, or
/// nested numeric list).
Measurement measurement_from_py(pybind11::handle obj, const Unit& unit)
{
    // Scalars
    if (pybind11::isinstance<pybind11::bool_>(obj))
        return Measurement::Boolean(pybind11::cast<bool>(obj));
    if (pybind11::isinstance<pybind11::int_>(obj))
    {
        long long v = pybind11::cast<long long>(obj);
        if (v < -2147483648LL || v > 2147483647LL)
            throw std::overflow_error("integer out of int32 range");
        return Measurement::Integer(static_cast<int>(v), unit);
    }
    if (pybind11::isinstance<pybind11::float_>(obj))
        return Measurement::Real(pybind11::cast<double>(obj), unit);
    if (PyComplex_Check(obj.ptr()))
        return Measurement::Complex(pybind11::cast<std::complex<double>>(obj), unit);
    if (pybind11::isinstance<pybind11::str>(obj))
        return Measurement::String(pybind11::cast<std::string>(obj));

    // Buffer / array: 1-d -> vector, 2-d -> matrix (a Measurement is a
    // single cell, so 1-d means a vector cell, 2-d a matrix cell).
    if (pybind11::isinstance<pybind11::buffer>(obj))
    {
        pybind11::buffer_info info = pybind11::reinterpret_borrow<pybind11::buffer>(obj).request();
        const std::string fmt = info.format;

        if (info.ndim == 1)
        {
            const Index w = static_cast<Index>(info.shape[0]);
            if (fmt == "d")
            {
                VecXd v(w);
                std::memcpy(v.data(), info.ptr, sizeof(double) * w);
                return Measurement::Vector(v, unit);
            }
            if (fmt == "i")
            {
                VecXi v(w);
                std::memcpy(v.data(), info.ptr, sizeof(int) * w);
                return Measurement::Vector(v, unit);
            }
            if (fmt == "Zd")
            {
                VecXcd v(w);
                std::memcpy(v.data(), info.ptr, sizeof(std::complex<double>) * w);
                return Measurement::Vector(v, unit);
            }
            throw pybind11::type_error("unsupported buffer format for Measurement: " + fmt);
        }
        if (info.ndim == 2)
        {
            const Index r = static_cast<Index>(info.shape[0]);
            const Index c = static_cast<Index>(info.shape[1]);
            if (fmt == "d")
            {
                MatXd m(r, c);
                std::memcpy(m.data(), info.ptr, sizeof(double) * r * c);
                return Measurement::Matrix(m, unit);
            }
            if (fmt == "i")
            {
                MatXi m(r, c);
                std::memcpy(m.data(), info.ptr, sizeof(int) * r * c);
                return Measurement::Matrix(m, unit);
            }
            if (fmt == "Zd")
            {
                MatXcd m(r, c);
                std::memcpy(m.data(), info.ptr, sizeof(std::complex<double>) * r * c);
                return Measurement::Matrix(m, unit);
            }
            throw pybind11::type_error("unsupported buffer format for Measurement: " + fmt);
        }
        throw pybind11::type_error("Measurement accepts a 1-d or 2-d numeric array only");
    }

    // Nested list -> vector / matrix (numeric only for now).
    if (pybind11::isinstance<pybind11::sequence>(obj))
    {
        pybind11::list outer = pybind11::cast<pybind11::list>(obj);
        if (outer.empty())
            return Measurement::Vector(VecXd(), unit);

        bool is_matrix = pybind11::isinstance<pybind11::sequence>(outer[0]);
        if (is_matrix)
        {
            // matrix (list of lists)
            Index r = static_cast<Index>(outer.size());
            Index c = pybind11::isinstance<pybind11::sequence>(outer[0])
                ? static_cast<Index>(pybind11::len(outer[0])) : 0;
            MatXd mat(r, c);
            for (Index i = 0; i < r; ++i)
            {
                pybind11::list row = pybind11::cast<pybind11::list>(outer[i]);
                for (Index j = 0; j < c; ++j)
                    mat(i, j) = pybind11::cast<double>(row[j]);
            }
            return Measurement::Matrix(mat, unit);
        }
        // vector (flat list)
        Index w = static_cast<Index>(outer.size());
        VecXd vec(w);
        for (Index i = 0; i < w; ++i)
            vec(i) = pybind11::cast<double>(outer[i]);
        return Measurement::Vector(vec, unit);
    }

    throw pybind11::type_error("cannot convert object to Measurement");
}

// ---- string export (copy; variable-length strings cannot be zero-copy) ---

pybind11::object string_series_to_py(const DataSeries& ds)
{
    const Index n = static_cast<Index>(ds.size());
    pybind11::list outer;
    switch (ds.data_kind())
    {
        case DataKind::kScalar:
            for (Index i = 0; i < n; ++i)
                outer.append(ds.scalar_at<std::string>(i));
            break;
        case DataKind::kVector:
            for (Index i = 0; i < n; ++i)
            {
                const VecXs& row = ds.vector_at<std::string>(i);
                pybind11::list r;
                for (Index j = 0; j < row.dimension(0); ++j)
                    r.append(row(j));
                outer.append(r);
            }
            break;
        case DataKind::kMatrix:
            for (Index i = 0; i < n; ++i)
            {
                const MatXs& cell = ds.matrix_at<std::string>(i);
                pybind11::list rows;
                for (Index r = 0; r < cell.dimension(0); ++r)
                {
                    pybind11::list cols;
                    for (Index c = 0; c < cell.dimension(1); ++c)
                        cols.append(cell(r, c));
                    rows.append(cols);
                }
                outer.append(rows);
            }
            break;
    }
    return outer;
}

pybind11::object string_measurement_to_py(const Measurement& m)
{
    switch (m.data_kind())
    {
        case DataKind::kScalar:
            return pybind11::str(m.as_scalar<std::string>());
        case DataKind::kVector:
        {
            const VecXs& v = m.as_vector<std::string>();
            pybind11::list out;
            for (Index j = 0; j < v.dimension(0); ++j)
                out.append(v(j));
            return out;
        }
        case DataKind::kMatrix:
        {
            const MatXs& mx = m.as_matrix<std::string>();
            pybind11::list rows;
            for (Index r = 0; r < mx.dimension(0); ++r)
            {
                pybind11::list cols;
                for (Index c = 0; c < mx.dimension(1); ++c)
                    cols.append(mx(r, c));
                rows.append(cols);
            }
            return rows;
        }
    }
    return pybind11::none();
}

pybind11::object numpy_from_py(pybind11::object list_or_str)
{
    pybind11::object np = pybind11::module_::import("numpy");
    return np.attr("array")(list_or_str);
}

namespace {

// ---- selectors ---------------------------------------------------------

MultiIndexSelector selector_from_value(pybind11::handle v)
{
    if (v.is_none())
        return MultiIndexSelector::Any();
    if (pybind11::isinstance<pybind11::int_>(v))
        return MultiIndexSelector::Equal(pybind11::cast<Index>(v));
    if (pybind11::isinstance<MultiIndexSelector>(v))
        return pybind11::cast<MultiIndexSelector>(v);
    if (pybind11::isinstance<pybind11::sequence>(v))
    {
        std::vector<Index> idx;
        for (auto item : pybind11::cast<pybind11::sequence>(v))
            idx.push_back(pybind11::cast<Index>(item));
        return MultiIndexSelector::In(idx);
    }
    throw pybind11::type_error("selector must be int, list of ints, None, or MultiIndexSelector");
}

/// Parse positional selectors for DataArray::at / DataArray::select.
/// Accepts either a single list/tuple of selectors, or a sequence of selector
/// arguments; each selector is an int (Equal), a list of ints (In), None
/// (Any), or a MultiIndexSelector.  Mirrors the C++ positional API.
std::vector<MultiIndexSelector> parse_selectors(pybind11::args args)
{
    std::vector<MultiIndexSelector> sels;

    // A single list/tuple argument expands to the selector list.
    if (args.size() == 1 && pybind11::isinstance<pybind11::sequence>(args[0]) &&
        !pybind11::isinstance<pybind11::int_>(args[0]))
    {
        for (auto item : pybind11::cast<pybind11::sequence>(args[0]))
            sels.push_back(selector_from_value(item));
        return sels;
    }

    for (std::size_t i = 0; i < args.size(); ++i)
        sels.push_back(selector_from_value(args[i]));
    return sels;
}

// ---- group / leaf iteration result wrappers ---------------------------

struct PyDimGroup
{
    std::vector<Index> multi_index;
    Index flat_start = 0;
    Index flat_end = 0;
    Index size = 0;
};

struct PyLeafRow
{
    std::vector<Index> multi_index;
    std::vector<Index> dim_indices;
    Index row_flat = 0;
};

}  // namespace

// =========================================================================
//  register_xdataset_bindings
// =========================================================================

void register_xdataset_bindings(pybind11::module_& m)
{
    // ---- Unit ---------------------------------------------------------
    pybind11::class_<Unit>(m, "Unit")
        .def(pybind11::init<>())
        .def_static("parse", &Unit::parse)
        .def_property_readonly("multiplier", &Unit::multiplier)
        .def("has_dimension", &Unit::has_dimension)
        .def("same_dimension", &Unit::same_dimension)
        .def("__mul__", &Unit::operator*)
        .def("__truediv__", &Unit::operator/)
        .def("__pow__", &Unit::pow)
        .def("__eq__", [](const Unit& a, const Unit& b) { return a == b; })
        .def("__ne__", [](const Unit& a, const Unit& b) { return a != b; })
        .def("__str__", &Unit::to_string)
        .def("__repr__", &Unit::to_string);

    // ---- Measurement --------------------------------------------------
    pybind11::class_<Measurement>(m, "Measurement", pybind11::buffer_protocol())
        .def(pybind11::init([](pybind11::object v) { return measurement_from_py(v, Unit()); }))
        .def(pybind11::init([](pybind11::object v, const Unit& u) { return measurement_from_py(v, u); }))
        .def_static("real", [](double v, const Unit& u) { return Measurement::Real(v, u); },
                    pybind11::arg("value"), pybind11::arg("unit") = Unit())
        .def_static("integer", [](int v, const Unit& u) { return Measurement::Integer(v, u); },
                    pybind11::arg("value"), pybind11::arg("unit") = Unit())
        .def_static("complex", [](std::complex<double> v, const Unit& u) { return Measurement::Complex(v, u); },
                    pybind11::arg("value"), pybind11::arg("unit") = Unit())
        .def_static("string", &Measurement::String)
        .def_static("boolean", &Measurement::Boolean)
        .def_property_readonly("data_kind", [](const Measurement& mm) { return kind_str(mm.data_kind()); })
        .def_property_readonly("data_type", [](const Measurement& mm) { return type_str(mm.data_type()); })
        .def_property_readonly("unit", [](const Measurement& mm) { return mm.unit(); })
        .def_property_readonly("element_count", &Measurement::element_count)
        .def("element_at", static_cast<Measurement (Measurement::*)(Index) const>(&Measurement::element_at))
        .def("element_at", static_cast<Measurement (Measurement::*)(Index, Index) const>(&Measurement::element_at))
        .def_buffer(&make_measurement_buffer)
        .def("__array__", [](const Measurement& mm, pybind11::args, pybind11::kwargs) -> pybind11::object {
            if (mm.data_type() == DataType::kString)
                return numpy_from_py(string_measurement_to_py(mm));
            throw pybind11::type_error("numeric data uses the buffer protocol");
        })
        .def("__str__", &Measurement::to_string)
        .def("__repr__", &Measurement::to_string);

    // ---- DataSeries ---------------------------------------------------
    pybind11::class_<DataSeries>(m, "DataSeries", pybind11::buffer_protocol())
        .def_static("from_array", &dataseries_from_buffer)
        .def_property_readonly("size", [](const DataSeries& d) { return d.size(); })
        .def("__len__", [](const DataSeries& d) { return d.size(); })
        .def_property_readonly("unit", [](const DataSeries& d) { return d.unit(); })
        .def_property_readonly("data_type", [](const DataSeries& d) { return type_str(d.data_type()); })
        .def_property_readonly("data_kind", [](const DataSeries& d) { return kind_str(d.data_kind()); })
        .def("measurement_at", &DataSeries::measurement_at)
        .def("__getitem__", [](const DataSeries& d, Index i) { return d.measurement_at(i); })
        .def("iloc", &DataSeries::iloc)
        .def("canonicalized", &DataSeries::canonicalized)
        .def_buffer(&make_series_buffer)
        .def("__array__", [](const DataSeries& d, pybind11::args, pybind11::kwargs) -> pybind11::object {
            if (d.data_type() == DataType::kString)
                return numpy_from_py(string_series_to_py(d));
            throw pybind11::type_error("numeric data uses the buffer protocol");
        });

    // ---- dimension specs ----------------------------------------------
    pybind11::class_<RegularDim>(m, "RegularDim")
        .def(pybind11::init<std::size_t>())
        .def_readwrite("size", &RegularDim::size);

    pybind11::class_<RaggedDim>(m, "RaggedDim")
        .def(pybind11::init<std::vector<std::size_t>>())
        .def_readwrite("sizes", &RaggedDim::sizes);

    pybind11::class_<DimensionSpec>(m, "DimensionSpec")
        .def_static("Regular", &DimensionSpec::Regular)
        .def_static("Ragged", &DimensionSpec::Ragged)
        .def_property_readonly("is_regular", &DimensionSpec::is_regular)
        .def_property_readonly("is_ragged", &DimensionSpec::is_ragged)
        .def_property_readonly("regular_size", &DimensionSpec::regular_size)
        .def_property_readonly("ragged_sizes", &DimensionSpec::ragged_sizes);

    pybind11::class_<MultiDimensionSpec>(m, "MultiDimensionSpec")
        .def_property_readonly("rank", &MultiDimensionSpec::rank)
        .def("to_string", &MultiDimensionSpec::to_string)
        .def("__str__", &MultiDimensionSpec::to_string)
        .def("__repr__", &MultiDimensionSpec::to_string);

    // ---- group / leaf wrappers ----------------------------------------
    pybind11::class_<PyDimGroup>(m, "DimGroup")
        .def_readonly("multi_index", &PyDimGroup::multi_index)
        .def_readonly("flat_start", &PyDimGroup::flat_start)
        .def_readonly("flat_end", &PyDimGroup::flat_end)
        .def_readonly("size", &PyDimGroup::size);

    pybind11::class_<PyLeafRow>(m, "LeafRow")
        .def_readonly("multi_index", &PyLeafRow::multi_index)
        .def_readonly("dim_indices", &PyLeafRow::dim_indices)
        .def_readonly("row_flat", &PyLeafRow::row_flat);

    // ---- DataArray ----------------------------------------------------
    pybind11::class_<DataArray>(m, "DataArray", pybind11::buffer_protocol())
        .def_property_readonly("data_kind", [](const DataArray& d) {
            return d.data_kind() == DataArrayKind::kDependent ? "dependent" : "independent";
        }) 
        .def_property_readonly("indep_names", &DataArray::indep_names)
        .def_property_readonly("data", [](const DataArray& d) { return d.data(); })
        .def("indep_data", static_cast<DataSeries (DataArray::*)(Index) const>(&DataArray::indep_data))
        .def("indep_data", static_cast<DataSeries (DataArray::*)(const std::string&) const>(&DataArray::indep_data))
        .def("set_data", static_cast<void (DataArray::*)(DataSeries)>(&DataArray::set_data))
        .def("set_data", static_cast<void (DataArray::*)(Index, Measurement)>(&DataArray::set_data))
        .def("set_indep_data", static_cast<void (DataArray::*)(DataSeries)>(&DataArray::set_indep_data))
        .def("set_indep_data", static_cast<void (DataArray::*)(Index, DataSeries)>(&DataArray::set_indep_data))
        .def("set_indep_data", static_cast<void (DataArray::*)(const std::string&, DataSeries)>(&DataArray::set_indep_data))
        .def("set_indep_data", static_cast<void (DataArray::*)(Index, Index, Measurement)>(&DataArray::set_indep_data))
        .def("set_indep_data", static_cast<void (DataArray::*)(const std::string&, Index, Measurement)>(&DataArray::set_indep_data))
        .def("clone", &DataArray::clone)
        .def("canonicalized", [](const DataArray& d) {
            // Same semantics as Value::canonicalized() for DataArray-backed
            // values: canonicalize the self data, keep indep dims untouched.
            DataArray result(d);
            result.set_data(d.data().canonicalized());
            return result;
        })
        .def_property_readonly("rank", [](const DataArray& d) { return d.multi_dimension_spec().rank(); })
        .def_property_readonly("flat_size", [](const DataArray& d) { return d.multi_dimension_spec().compute_cell_count(); })
        .def("group_count_at_dim", [](const DataArray& d, Index dim_idx) {
            return d.multi_dimension_spec().group_count_at_dim(dim_idx);
        })
        .def("groups_at_dim", [](const DataArray& d, Index dim_idx) {
            std::vector<PyDimGroup> out;
            d.multi_dimension_spec().for_each_group_at_dim(dim_idx,
                [&](const MultiDimensionSpec::DimGroup& g) {
                    PyDimGroup pg;
                    pg.multi_index = g.multi_index;
                    pg.flat_start = g.flat_start;
                    pg.flat_end = g.flat_end;
                    pg.size = g.flat_end - g.flat_start;
                    out.push_back(std::move(pg));
                });
            return out;
        })
        .def("leaves", [](const DataArray& d, Index start, Index end) {
            std::vector<PyLeafRow> out;
            d.multi_dimension_spec().for_each_leaf_row(
                [&](const MultiDimensionSpec::LeafRow& lr) {
                    PyLeafRow row;
                    row.multi_index = lr.multi_index;
                    row.dim_indices = lr.dimension_row_indices;
                    row.row_flat = lr.row_flat;
                    out.push_back(std::move(row));
                }, start, end);
            return out;
        })
        .def("all_leaves", [](const DataArray& d) {
            std::vector<PyLeafRow> out;
            d.multi_dimension_spec().for_each_leaf_row(
                [&](const MultiDimensionSpec::LeafRow& lr) {
                    PyLeafRow row;
                    row.multi_index = lr.multi_index;
                    row.dim_indices = lr.dimension_row_indices;
                    row.row_flat = lr.row_flat;
                    out.push_back(std::move(row));
                });
            return out;
        })
        .def("at", [](const DataArray& d, pybind11::args args) {
            return d.at(parse_selectors(args));
        })
        .def("select", [](const DataArray& d, pybind11::args args) {
            return d.select(parse_selectors(args));
        })
        .def_property_readonly("multi_dimension_spec",
            [](const DataArray& d) { return d.multi_dimension_spec(); })
        .def_buffer([](const DataArray& d) -> pybind11::buffer_info {
            // Zero-copy into the internal self DataSeries (last entry).
            const DataSeries& self = d.datas().rbegin()->second;
            return make_series_buffer(self);
        })
        .def("__array__", [](const DataArray& d, pybind11::args, pybind11::kwargs) -> pybind11::object {
            if (d.data().data_type() == DataType::kString)
                return numpy_from_py(string_series_to_py(d.datas().rbegin()->second));
            throw pybind11::type_error("numeric data uses the buffer protocol");
        });

    // ---- Block --------------------------------------------------------
    pybind11::class_<IndependentSpec>(m, "IndependentSpec")
        .def_property_readonly("name", [](const IndependentSpec& s) { return s.name; })
        .def_property_readonly("data", [](const IndependentSpec& s) { return s.data; })
        .def_property_readonly("dimension", [](const IndependentSpec& s) { return s.dimension; });

    pybind11::class_<DependentSpec>(m, "DependentSpec")
        .def_property_readonly("name", [](const DependentSpec& s) { return s.name; })
        .def_property_readonly("data", [](const DependentSpec& s) { return s.data; });

    pybind11::class_<BlockCreateInfo>(m, "BlockCreateInfo")
        .def(pybind11::init([](pybind11::list independents, pybind11::list dependents) {
            BlockCreateInfo info;
            for (auto item : independents)
            {
                pybind11::tuple t = pybind11::cast<pybind11::tuple>(item);
                std::string name = pybind11::cast<std::string>(t[0]);
                DataSeries data = pybind11::cast<DataSeries>(t[1]);
                if (pybind11::len(t) < 3)
                    throw pybind11::value_error(
                        "independent spec needs a dimension (RegularDim / RaggedDim)");

                pybind11::handle d = t[2];
                DimensionSpec dim = [&]() -> DimensionSpec {
                    if (pybind11::isinstance<RegularDim>(d))
                        return DimensionSpec::Regular(pybind11::cast<RegularDim>(d).size);
                    if (pybind11::isinstance<RaggedDim>(d))
                        return DimensionSpec::Ragged(pybind11::cast<RaggedDim>(d).sizes);
                    return pybind11::cast<DimensionSpec>(d);
                }();

                IndependentSpec spec{std::move(name), std::move(data), std::move(dim)};
                info.independent_specs.push_back(std::move(spec));
            }
            for (auto item : dependents)
            {
                pybind11::tuple t = pybind11::cast<pybind11::tuple>(item);
                DependentSpec spec;
                spec.name = pybind11::cast<std::string>(t[0]);
                spec.data = pybind11::cast<DataSeries>(t[1]);
                info.dependent_specs.push_back(std::move(spec));
            }
            return info;
        }), pybind11::arg("independents"), pybind11::arg("dependents"));

    pybind11::class_<Block>(m, "Block")
        .def_property("name",
            [](const Block& b) { return b.name(); },
            [](Block& b, std::string n) { b.set_name(std::move(n)); })
        .def("dependents", &Block::dependents)
        .def("independents", &Block::independents)
        .def("independent_spec", [](const Block& b, const std::string& n) { return b.independent_spec(n); })
        .def("dependent_spec", [](const Block& b, const std::string& n) { return b.dependent_spec(n); })
        .def("GetOrCreateDataArray", [](const Block& b, const std::string& n) {
            return DataArray(b.GetOrCreateDataArray(n));
        })
        .def("GetOrCreateDataFrame",
            [](const Block& b) -> const DataFrame& { return b.GetOrCreateDataFrame(); },
            pybind11::return_value_policy::reference_internal);

    // ---- DataFrame ----------------------------------------------------
    pybind11::class_<DataFrame>(m, "DataFrame")
        .def_property_readonly("row_count", &DataFrame::row_count)
        .def("to_string", &DataFrame::to_string, pybind11::arg("max_display_rows") = 32)
        .def("__str__", [](const DataFrame& f) { return f.to_string(); })
        .def("__repr__", [](const DataFrame& f) { return f.to_string(); });

    // ---- Dataset ------------------------------------------------------
    pybind11::class_<Dataset>(m, "Dataset")
        .def(pybind11::init<>())
        .def(pybind11::init<std::string>())
        .def_property("name",
            [](const Dataset& d) { return d.name(); },
            [](Dataset& d, std::string n) { d.set_name(std::move(n)); })
        .def_property_readonly("block_count", &Dataset::block_count)
        .def("IsLeaf", &Dataset::IsLeaf)
        .def("Exists", &Dataset::Exists)
        .def("HasUniqueDataArray", &Dataset::HasUniqueDataArray)
        .def("GetBlock", [](Dataset& d, const std::string& p) -> Block& { return d.GetBlock(p); },
             pybind11::return_value_policy::reference_internal)
        .def("AddBlock", [](Dataset& d, const std::string& p, BlockCreateInfo info) -> Block& {
             return d.AddBlock(p, std::move(info));
             }, pybind11::return_value_policy::reference_internal)
        .def("RemoveBlock", &Dataset::RemoveBlock)
        .def("RemoveGroup", &Dataset::RemoveGroup)
        .def("GetDataArray", [](Dataset& d, const std::string& block_path, const std::string& name) {
             return DataArray(d.GetDataArray(block_path, name));
        })
        .def("GetDataArray", [](Dataset& d, const std::string& name) {
             return DataArray(d.GetDataArray(name));
        })
        .def("GetDataArrayNames", &Dataset::GetDataArrayNames)
        .def("GetBlockNames", &Dataset::GetBlockNames, pybind11::arg("group_path") = "")
        .def("GetGroupNames", &Dataset::GetGroupNames, pybind11::arg("group_path") = "")
        .def("GetAllBlockPaths", &Dataset::GetAllBlockPaths);

    // ---- MultiIndexSelector -------------------------------------------
    pybind11::class_<MultiIndexSelector>(m, "MultiIndexSelector")
        .def_static("Any", &MultiIndexSelector::Any)
        .def_static("Equal", &MultiIndexSelector::Equal)
        .def_static("In", &MultiIndexSelector::In);
}

}  // namespace python
}  // namespace rel
