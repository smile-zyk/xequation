#include "data_series.h"

#include <algorithm>
#include <complex>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace xdataset {

// =========================================================================
// DataSeries -- constructors & assignment operators
// =========================================================================

DataSeries::DataSeries()
    : data_type_(DataType::kReal),
      shape_(),
      storage_(make_storage(DataType::kReal, DataShape())),
      unit_() {}

DataSeries::DataSeries(DataType dtype, const DataShape& shape)
    : data_type_(dtype == DataType::kBoolean ? DataType::kInteger : dtype)
    , shape_(shape)
    , storage_(make_storage(dtype == DataType::kBoolean ? DataType::kInteger : dtype, shape))
    , unit_() {}

DataSeries::DataSeries(const DataSeries& other)
    : data_type_(other.data_type_), shape_(other.shape_),
      storage_(other.storage_->clone()), unit_(other.unit_) {}

DataSeries& DataSeries::operator=(const DataSeries& other) {
    if (this != &other) {
        data_type_ = other.data_type_;
        shape_ = other.shape_;
        storage_ = other.storage_->clone();
        unit_ = other.unit_;
    }
    return *this;
}

DataSeries::DataSeries(DataSeries&& other) noexcept
    : data_type_(other.data_type_),
      shape_(std::move(other.shape_)),
      storage_(std::move(other.storage_)),
      unit_(std::move(other.unit_)) {}

DataSeries& DataSeries::operator=(DataSeries&& other) noexcept {
    data_type_ = other.data_type_;
    shape_ = std::move(other.shape_);
    storage_ = std::move(other.storage_);
    unit_ = std::move(other.unit_);
    return *this;
}

// =========================================================================
// DataSeries -- unit
// =========================================================================

void DataSeries::set_unit(const std::string& s) {
    unit_ = Unit::parse(s);
}

/// In-place scale every element of \p ds by \p mult.  \p T must match the
/// series' stored dtype exactly (kReal -> double, kComplex -> complex<double>).
template <typename T>
void scale_by_multiplier(DataSeries& ds, double mult)
{
    const Index n = static_cast<Index>(ds.size());
    switch (ds.data_shape().kind())
    {
        case DataKind::kScalar:
            for (Index i = 0; i < n; ++i)
                ds.scalar_at<T>(i) = static_cast<T>(ds.scalar_at<T>(i) * mult);
            break;
        case DataKind::kVector:
            for (Index i = 0; i < n; ++i)
            {
                auto v = ds.vector_at<T>(i);
                for (Index j = 0; j < v.size(); ++j)
                    v(j) = static_cast<T>(v(j) * mult);
            }
            break;
        case DataKind::kMatrix:
            for (Index i = 0; i < n; ++i)
            {
                auto m = ds.matrix_at<T>(i);
                for (Index r = 0; r < m.rows(); ++r)
                    for (Index c = 0; c < m.cols(); ++c)
                        m(r, c) = static_cast<T>(m(r, c) * mult);
            }
            break;
    }
}

void DataSeries::canonicalize() {
    // Strings are not numeric -- only update the unit tag, no value conversion.
    if (data_type_ == DataType::kString) {
        unit_ = unit_.canonicalized();
        return;
    }

    const double mult = unit_.multiplier();
    const Unit target = unit_.canonicalized();

    if (mult == 1.0) {
        unit_ = target;
        return;
    }

    // Integer storage cannot hold scaled values (int * 1e9 would truncate);
    // promote to real first -- same semantics as Measurement::canonicalized().
    if (data_type_ == DataType::kInteger) {
        DataSeries real = promoted_data_type(DataType::kReal);
        real.canonicalize();             // now kReal -- scales in place above
        *this = std::move(real);
        return;
    }

    if (data_type_ == DataType::kReal)
        scale_by_multiplier<double>(*this, mult);
    else  // kComplex
        scale_by_multiplier<std::complex<double>>(*this, mult);

    unit_ = target;
}

DataSeries DataSeries::canonicalized() const {
    DataSeries result(*this);
    result.canonicalize();
    return result;
}

bool DataSeries::is_canonicalized() const {
    return unit_.is_canonical();
}

DataSeries DataSeries::promoted_data_type(DataType target) const {
    if (data_type_ == target) return *this;

    // Only one-directional: int -> real -> complex.  String / Boolean cannot promote.
    auto can_promote = [](DataType from, DataType to) -> bool {
        if (from == DataType::kInteger && to == DataType::kReal)    return true;
        if (from == DataType::kInteger && to == DataType::kComplex) return true;
        if (from == DataType::kReal    && to == DataType::kComplex) return true;
        return false;
    };
    if (!can_promote(data_type_, target))
        throw std::invalid_argument("promoted_data_type: cannot promote from " +
            std::string(DataTypeToString(data_type_)) + " to " +
            std::string(DataTypeToString(target)));

    // Build a new DataSeries with target dtype, copy rows with promotion
    DataSeries out(target, shape_);
    out.unit_ = unit_;
    out.resize(size());

    std::size_t n = size();
    for (std::size_t i = 0; i < n; ++i) {
        Index idx = static_cast<Index>(i);
        if (shape_.kind() == DataKind::kScalar) {
            if (target == DataType::kReal)
                out.scalar_at<double>(idx) = static_cast<double>(scalar_at<int>(idx));
            else
                out.scalar_at<std::complex<double>>(idx) = static_cast<std::complex<double>>(
                    data_type_ == DataType::kInteger ? static_cast<double>(scalar_at<int>(idx))
                                                     : scalar_at<double>(idx));
        } else if (shape_.kind() == DataKind::kVector) {
            if (target == DataType::kReal)
                out.vector_at<double>(idx) = vector_at<int>(idx).template cast<double>();
            else if (data_type_ == DataType::kInteger)
                out.vector_at<std::complex<double>>(idx) = vector_at<int>(idx).template cast<std::complex<double>>();
            else
                out.vector_at<std::complex<double>>(idx) = vector_at<double>(idx).template cast<std::complex<double>>();
        } else {
            if (target == DataType::kReal)
                out.matrix_at<double>(idx) = matrix_at<int>(idx).template cast<double>();
            else if (data_type_ == DataType::kInteger)
                out.matrix_at<std::complex<double>>(idx) = matrix_at<int>(idx).template cast<std::complex<double>>();
            else
                out.matrix_at<std::complex<double>>(idx) = matrix_at<double>(idx).template cast<std::complex<double>>();
        }
    }

    return out;
}

DataSeries DataSeries::as_logical() const {
    DataSeries out(DataType::kInteger, shape_);
    out.resize(size());
    std::size_t n = size();

    for (std::size_t i = 0; i < n; ++i) {
        Index idx = static_cast<Index>(i);
        if (shape_.kind() == DataKind::kScalar) {
            int val = 0;
            switch (data_type_) {
                case DataType::kReal:    val = (scalar_at<double>(idx) != 0.0) ? 1 : 0; break;
                case DataType::kInteger: val = (scalar_at<int>(idx) != 0) ? 1 : 0; break;
                case DataType::kComplex: val = (scalar_at<std::complex<double>>(idx) != std::complex<double>(0.0, 0.0)) ? 1 : 0; break;
                case DataType::kString:  val = scalar_at<std::string>(idx).empty() ? 0 : 1; break;
                default: throw std::invalid_argument("as_logical: unsupported dtype");
            }
            out.scalar_at<int>(idx) = val;
        } else if (shape_.kind() == DataKind::kVector) {
            Index w = shape_[0];
            auto out_v = out.vector_at<int>(idx);
            if (data_type_ == DataType::kInteger) {
                for (Index j = 0; j < w; ++j)
                    out_v(j) = (vector_at<int>(idx)(j) != 0) ? 1 : 0;
            } else if (data_type_ == DataType::kReal) {
                for (Index j = 0; j < w; ++j)
                    out_v(j) = (vector_at<double>(idx)(j) != 0.0) ? 1 : 0;
            } else if (data_type_ == DataType::kComplex) {
                for (Index j = 0; j < w; ++j)
                    out_v(j) = (vector_at<std::complex<double>>(idx)(j) != std::complex<double>(0.0, 0.0)) ? 1 : 0;
            } else {
                throw std::invalid_argument("as_logical: unsupported vector dtype");
            }
        } else {
            Index rows = shape_[0], cols = shape_[1];
            auto out_m = out.matrix_at<int>(idx);
            if (data_type_ == DataType::kInteger) {
                for (Index r = 0; r < rows; ++r)
                    for (Index c = 0; c < cols; ++c)
                        out_m(r, c) = (matrix_at<int>(idx)(r, c) != 0) ? 1 : 0;
            } else if (data_type_ == DataType::kReal) {
                for (Index r = 0; r < rows; ++r)
                    for (Index c = 0; c < cols; ++c)
                        out_m(r, c) = (matrix_at<double>(idx)(r, c) != 0.0) ? 1 : 0;
            } else if (data_type_ == DataType::kComplex) {
                for (Index r = 0; r < rows; ++r)
                    for (Index c = 0; c < cols; ++c)
                        out_m(r, c) = (matrix_at<std::complex<double>>(idx)(r, c) != std::complex<double>(0.0, 0.0)) ? 1 : 0;
            } else {
                throw std::invalid_argument("as_logical: unsupported matrix dtype");
            }
        }
    }
    return out;
}

// =========================================================================
// DataSeries -- slicing
// =========================================================================

DataSeries DataSeries::head(std::size_t n) const {
    std::size_t sz = size();
    return iloc(0, n < sz ? n : sz);
}

DataSeries DataSeries::tail(std::size_t n) const {
    std::size_t sz = size();
    return iloc(n < sz ? sz - n : 0, sz);
}

DataSeries DataSeries::iloc(std::size_t start, std::size_t end) const {
    if (start > end || end > size()) throw std::out_of_range("iloc out of range");
    DataSeries out(data_type_, shape_);
    out.unit_ = unit_;
    for (std::size_t i = start; i < end; ++i) out.append_from(*this, static_cast<Index>(i));
    return out;
}

// =========================================================================
// DataSeries -- append helpers (non-template overloads)
// =========================================================================

void DataSeries::append_vector(const VecXs& v) {
    if (v.dimension(0) != element_count()) throw std::bad_cast();
    vector_storage_string()->append(v);
}

void DataSeries::append_matrix(const MatXs& m) {
    if (m.dimension(0) != shape_[0] || m.dimension(1) != shape_[1]) throw std::bad_cast();
    matrix_storage_string()->append(m);
}

void DataSeries::append_from(const DataSeries& src, Index row) {
    const Index dst_row = static_cast<Index>(size());
    resize(size() + 1);
    assign_from(src, row, dst_row);
}

void DataSeries::assign_from(const DataSeries& src, Index src_row, Index dst_row) {
    if (src.data_type_ != data_type_ || src.shape_ != shape_) throw std::bad_cast();
    if (src_row < 0 || static_cast<std::size_t>(src_row) >= src.size() ||
        dst_row < 0 || static_cast<std::size_t>(dst_row) >= size()) throw std::out_of_range("row index out of range");

    // First non-dimensionless source sets the series' unit.
    if (!unit_.has_dimension() && src.unit_.has_dimension()) {
        if (data_type_ == DataType::kString)
            throw std::invalid_argument("string series cannot have a named unit");
        unit_ = src.unit_;
    } else if (unit_.has_dimension() && src.unit_.has_dimension() &&
               !src.unit_.same_dimension(unit_)) {
        throw std::invalid_argument(
            "unit mismatch: series has dimension [" + unit_.to_string() +
            "], source has [" + src.unit_.to_string() + "]");
    }

    if (shape_.kind() == DataKind::kScalar) {
        if (data_type_ == DataType::kReal) scalar_at<double>(dst_row) = src.scalar_at<double>(src_row);
        else if (data_type_ == DataType::kInteger) scalar_at<int>(dst_row) = src.scalar_at<int>(src_row);
        else if (data_type_ == DataType::kComplex) {
            scalar_at<std::complex<double> >(dst_row) = src.scalar_at<std::complex<double> >(src_row);
        } else {
            scalar_at<std::string>(dst_row) = src.scalar_at<std::string>(src_row);
        }
        return;
    }

    if (shape_.kind() == DataKind::kVector) {
        if (data_type_ == DataType::kReal) vector_at<double>(dst_row) = src.vector_at<double>(src_row);
        else if (data_type_ == DataType::kInteger) vector_at<int>(dst_row) = src.vector_at<int>(src_row);
        else if (data_type_ == DataType::kComplex) {
            vector_at<std::complex<double> >(dst_row) = src.vector_at<std::complex<double> >(src_row);
        } else {
            vector_at<std::string>(dst_row) = src.vector_at<std::string>(src_row);
        }
        return;
    }

    if (data_type_ == DataType::kReal) matrix_at<double>(dst_row) = src.matrix_at<double>(src_row);
    else if (data_type_ == DataType::kInteger) matrix_at<int>(dst_row) = src.matrix_at<int>(src_row);
    else if (data_type_ == DataType::kComplex) {
        matrix_at<std::complex<double> >(dst_row) = src.matrix_at<std::complex<double> >(src_row);
    } else {
        matrix_at<std::string>(dst_row) = src.matrix_at<std::string>(src_row);
    }
}

void DataSeries::append(const Measurement& m)
{
    // kBoolean Measurement is stored as int (0/1) inside an Integer DataSeries.
    const bool meas_is_bool = (m.data_type() == DataType::kBoolean);
    if (!meas_is_bool && (m.data_type() != data_type_ || m.shape() != shape_))
        throw std::bad_cast();
    if (meas_is_bool && (data_type_ != DataType::kInteger || m.shape() != shape_))
        throw std::bad_cast();

    // First non-dimensionless measurement sets the series' unit.
    if (!unit_.has_dimension() && m.unit().has_dimension()) {
        if (data_type_ == DataType::kString)
            throw std::invalid_argument("string series cannot have a named unit");
        unit_ = m.unit();
    } else if (unit_.has_dimension() && m.unit().has_dimension() &&
               !m.unit().same_dimension(unit_)) {
        throw std::invalid_argument(
            "unit mismatch: series has dimension [" + unit_.to_string() +
            "], measurement has [" + m.unit().to_string() + "]");
    }

    if (shape_.kind() == DataKind::kScalar) {
        if (meas_is_bool) append_scalar(static_cast<int>(boost::get<bool>(m.storage())));
        else if (data_type_ == DataType::kReal) append_scalar(boost::get<double>(m.storage()));
        else if (data_type_ == DataType::kInteger) append_scalar(boost::get<int>(m.storage()));
        else if (data_type_ == DataType::kComplex) append_scalar(boost::get<std::complex<double> >(m.storage()));
        else append_scalar(boost::get<std::string>(m.storage()));
        return;
    }

    if (shape_.kind() == DataKind::kVector) {
        if (data_type_ == DataType::kReal) append_vector<double>(boost::get<VecXd>(m.storage()));
        else if (data_type_ == DataType::kInteger) append_vector<int>(boost::get<VecXi>(m.storage()));
        else if (data_type_ == DataType::kComplex) append_vector<std::complex<double> >(boost::get<VecXcd>(m.storage()));
        else append_vector(boost::get<VecXs >(m.storage()));
        return;
    }

    if (data_type_ == DataType::kReal) append_matrix<double>(NumericMatrixTypes<double>::OwnedType(boost::get<MatXd>(m.storage())));
    else if (data_type_ == DataType::kInteger) append_matrix<int>(NumericMatrixTypes<int>::OwnedType(boost::get<MatXi>(m.storage())));
    else if (data_type_ == DataType::kComplex) append_matrix<std::complex<double> >(NumericMatrixTypes<std::complex<double> >::OwnedType(boost::get<MatXcd>(m.storage())));
    else append_matrix(boost::get<MatXs >(m.storage()));
}

// =========================================================================
// DataSeries -- contiguous data
// =========================================================================

std::size_t DataSeries::contiguous_bytes() const {
    if (data_type_ == DataType::kReal) return contiguous_elements() * sizeof(double);
    if (data_type_ == DataType::kInteger) return contiguous_elements() * sizeof(int);
    if (data_type_ == DataType::kComplex) return contiguous_elements() * sizeof(std::complex<double>);
    throw std::runtime_error("string storage is not trivially-copyable");
}

// =========================================================================
// DataSeries -- measurement_at
// =========================================================================

Measurement DataSeries::measurement_at(Index i) const {
    if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("index out of range");

    if (shape_.kind() == DataKind::kScalar) {
        if (data_type_ == DataType::kReal) return Measurement::Real(scalar_at<double>(i), unit_);
        else if (data_type_ == DataType::kInteger) return Measurement::Integer(scalar_at<int>(i), unit_);
        else if (data_type_ == DataType::kComplex) return Measurement::Complex(scalar_at<std::complex<double> >(i), unit_);
        else return Measurement::String(scalar_at<std::string>(i));
    }

    if (shape_.kind() == DataKind::kVector) {
        if (data_type_ == DataType::kReal) return Measurement::Vector(vector_at<double>(i), unit_);
        else if (data_type_ == DataType::kInteger) return Measurement::Vector(vector_at<int>(i), unit_);
        else if (data_type_ == DataType::kComplex) return Measurement::Vector(vector_at<std::complex<double> >(i), unit_);
        else return Measurement::Vector(vector_at<std::string>(i));
    }

    if (data_type_ == DataType::kReal)
        return Measurement::Matrix(matrix_at<double>(i), unit_);
    if (data_type_ == DataType::kInteger)
        return Measurement::Matrix(matrix_at<int>(i), unit_);
    if (data_type_ == DataType::kComplex)
        return Measurement::Matrix(matrix_at<std::complex<double> >(i), unit_);
    return Measurement::Matrix(matrix_at<std::string>(i));
}

// =========================================================================
// DataSeries -- write_measurement_to_row (private static helper)
// =========================================================================
// DataSeries -- CreateFromMeasurements (batch factory)
// =========================================================================

namespace {

// ---- Scalar batch writers ----

template <typename T>
void write_scalar_batch(DataSeries& out, const std::vector<Measurement>& ms) {
    for (std::size_t i = 0; i < ms.size(); ++i)
        out.scalar_at<T>(static_cast<Index>(i)) = ms[i].as_scalar<T>();
}

// ---- Vector batch writers ----

template <typename T>
void write_vector_numeric_batch(DataSeries& out, const std::vector<Measurement>& ms) {
    for (std::size_t i = 0; i < ms.size(); ++i)
        out.vector_at<T>(static_cast<Index>(i)) = ms[i].as_vector<T>();
}

void write_vector_string_batch(DataSeries& out, const std::vector<Measurement>& ms) {
    for (std::size_t i = 0; i < ms.size(); ++i)
        out.vector_at<std::string>(static_cast<Index>(i)) = ms[i].as_vector<std::string>();
}

// ---- Matrix batch writers ----

template <typename T>
void write_matrix_numeric_batch(DataSeries& out, const std::vector<Measurement>& ms) {
    for (std::size_t i = 0; i < ms.size(); ++i)
        out.matrix_at<T>(static_cast<Index>(i)) = ms[i].as_matrix<T>();
}

void write_matrix_string_batch(DataSeries& out, const std::vector<Measurement>& ms) {
    for (std::size_t i = 0; i < ms.size(); ++i)
        out.matrix_at<std::string>(static_cast<Index>(i)) = ms[i].as_matrix<std::string>();
}

} // anonymous namespace

DataSeries DataSeries::CreateFromMeasurements(const std::vector<Measurement>& measurements) {
    if (measurements.empty())
        return DataSeries(DataType::kReal, DataShape::Scalar());

    const Measurement& first = measurements[0];
    DataType  dtype = first.data_type();
    DataKind  kind  = first.data_kind();

    DataSeries out(dtype, first.shape());
    out.set_unit(first.unit());
    out.resize(measurements.size());

    // Dispatch (kind, dtype) ONCE, then write all rows in a tight loop.
    if (kind == DataKind::kScalar) {
        switch (dtype) {
            case DataType::kReal:    write_scalar_batch<double>(out, measurements);              break;
            case DataType::kInteger: write_scalar_batch<int>(out, measurements);                 break;
            case DataType::kComplex: write_scalar_batch<std::complex<double>>(out, measurements); break;
            case DataType::kString:  write_scalar_batch<std::string>(out, measurements);         break;
            default: throw std::invalid_argument("CreateFromMeasurements: unsupported scalar dtype");
        }
    } else if (kind == DataKind::kVector) {
        switch (dtype) {
            case DataType::kReal:    write_vector_numeric_batch<double>(out, measurements);              break;
            case DataType::kInteger: write_vector_numeric_batch<int>(out, measurements);                 break;
            case DataType::kComplex: write_vector_numeric_batch<std::complex<double>>(out, measurements); break;
            case DataType::kString:  write_vector_string_batch(out, measurements);                      break;
            default: throw std::invalid_argument("CreateFromMeasurements: unsupported vector dtype");
        }
    } else {
        switch (dtype) {
            case DataType::kReal:    write_matrix_numeric_batch<double>(out, measurements);              break;
            case DataType::kInteger: write_matrix_numeric_batch<int>(out, measurements);                 break;
            case DataType::kComplex: write_matrix_numeric_batch<std::complex<double>>(out, measurements); break;
            case DataType::kString:  write_matrix_string_batch(out, measurements);                      break;
            default: throw std::invalid_argument("CreateFromMeasurements: unsupported matrix dtype");
        }
    }

    return out;
}

// =========================================================================
// DataSeries -- at (public)
// =========================================================================

DataSeries DataSeries::at(const std::vector<Index>& selected) const {
    if (shape_.kind() == DataKind::kScalar)
        throw std::logic_error("at is invalid for scalar data");
    if (shape_.kind() != DataKind::kVector)
        throw std::invalid_argument("vector at requires vector data");
    return at_vector_impl(selected);
}

DataSeries DataSeries::at(
    const std::vector<Index>& selected_rows,
    const std::vector<Index>& selected_cols) const {
    if (shape_.kind() == DataKind::kScalar)
        throw std::logic_error("at is invalid for scalar data");
    if (shape_.kind() != DataKind::kMatrix)
        throw std::invalid_argument("matrix at requires matrix data");
    return at_matrix_impl(selected_rows, selected_cols);
}

// =========================================================================
// DataSeries -- at_vector_impl (private)
// =========================================================================

DataSeries DataSeries::at_vector_impl(const std::vector<Index>& selected) const {
    if (shape_.kind() == DataKind::kScalar) {
        throw std::logic_error("at is invalid for scalar data");
    }

    if (shape_.kind() == DataKind::kVector) {
        if (selected.size() == 1) {
            if (data_type_ == DataType::kReal) {
                DataSeries out(DataType::kReal, DataShape::Scalar());
                out.resize(size());
                for (std::size_t row = 0; row < size(); ++row) {
                    out.scalar_at<double>(static_cast<Index>(row)) = vector_at<double>(static_cast<Index>(row))(selected[0]);
                }
                return out;
            }
            if (data_type_ == DataType::kInteger) {
                DataSeries out(DataType::kInteger, DataShape::Scalar());
                out.resize(size());
                for (std::size_t row = 0; row < size(); ++row) {
                    out.scalar_at<int>(static_cast<Index>(row)) = vector_at<int>(static_cast<Index>(row))(selected[0]);
                }
                return out;
            }
            if (data_type_ == DataType::kComplex) {
                DataSeries out(DataType::kComplex, DataShape::Scalar());
                out.resize(size());
                for (std::size_t row = 0; row < size(); ++row) {
                    out.scalar_at<std::complex<double> >(static_cast<Index>(row)) =
                        vector_at<std::complex<double> >(static_cast<Index>(row))(selected[0]);
                }
                return out;
            }

            DataSeries out(DataType::kString, DataShape::Scalar());
            out.resize(size());
            for (std::size_t row = 0; row < size(); ++row) {
                out.scalar_at<std::string>(static_cast<Index>(row)) = vector_at<std::string>(static_cast<Index>(row))(selected[0]);
            }
            return out;
        }

        if (data_type_ == DataType::kReal) {
            return at_vector_numeric_impl<double>(selected);
        }
        if (data_type_ == DataType::kInteger) {
            return at_vector_numeric_impl<int>(selected);
        }
        if (data_type_ == DataType::kComplex) {
            return at_vector_numeric_impl<std::complex<double> >(selected);
        }
        return at_vector_string_impl(selected);
    }

    throw std::invalid_argument("vector at requires vector data");
}

// =========================================================================
// DataSeries -- at_matrix_impl (private)
// =========================================================================

DataSeries DataSeries::at_matrix_impl(
    const std::vector<Index>& selected_rows,
    const std::vector<Index>& selected_cols) const {
    if (shape_.kind() == DataKind::kScalar) {
        throw std::logic_error("at is invalid for scalar data");
    }

    if (shape_.kind() == DataKind::kVector) {
        throw std::invalid_argument("matrix at requires matrix data");
    }

    if (selected_rows.size() == 1 && selected_cols.size() == 1) {
        if (data_type_ == DataType::kReal) {
            DataSeries out(DataType::kReal, DataShape::Scalar());
            out.resize(size());
            for (std::size_t row = 0; row < size(); ++row) {
                out.scalar_at<double>(static_cast<Index>(row)) =
                    matrix_at<double>(static_cast<Index>(row))(selected_rows[0], selected_cols[0]);
            }
            return out;
        }
        if (data_type_ == DataType::kInteger) {
            DataSeries out(DataType::kInteger, DataShape::Scalar());
            out.resize(size());
            for (std::size_t row = 0; row < size(); ++row) {
                out.scalar_at<int>(static_cast<Index>(row)) =
                    matrix_at<int>(static_cast<Index>(row))(selected_rows[0], selected_cols[0]);
            }
            return out;
        }
        if (data_type_ == DataType::kComplex) {
            DataSeries out(DataType::kComplex, DataShape::Scalar());
            out.resize(size());
            for (std::size_t row = 0; row < size(); ++row) {
                out.scalar_at<std::complex<double> >(static_cast<Index>(row)) =
                    matrix_at<std::complex<double> >(static_cast<Index>(row))(selected_rows[0], selected_cols[0]);
            }
            return out;
        }

        DataSeries out(DataType::kString, DataShape::Scalar());
        out.resize(size());
        for (std::size_t row = 0; row < size(); ++row) {
            out.scalar_at<std::string>(static_cast<Index>(row)) =
                matrix_at<std::string>(static_cast<Index>(row))(selected_rows[0], selected_cols[0]);
        }
        return out;
    }

    if (selected_rows.size() == 1 || selected_cols.size() == 1) {
        const bool select_columns = selected_rows.size() == 1;
        const std::vector<Index>& remaining = select_columns ? selected_cols : selected_rows;

        if (data_type_ == DataType::kReal) {
            DataSeries out(DataType::kReal, DataShape::Vector(static_cast<Index>(remaining.size())));
            out.resize(size());
            for (std::size_t row = 0; row < size(); ++row) {
                auto out_vec = out.vector_at<double>(static_cast<Index>(row));
                for (std::size_t i = 0; i < remaining.size(); ++i) {
                    const Index r = select_columns ? selected_rows[0] : remaining[i];
                    const Index c = select_columns ? remaining[i] : selected_cols[0];
                    out_vec(static_cast<Index>(i)) = matrix_at<double>(static_cast<Index>(row))(r, c);
                }
            }
            return out;
        }
        if (data_type_ == DataType::kInteger) {
            DataSeries out(DataType::kInteger, DataShape::Vector(static_cast<Index>(remaining.size())));
            out.resize(size());
            for (std::size_t row = 0; row < size(); ++row) {
                auto out_vec = out.vector_at<int>(static_cast<Index>(row));
                for (std::size_t i = 0; i < remaining.size(); ++i) {
                    const Index r = select_columns ? selected_rows[0] : remaining[i];
                    const Index c = select_columns ? remaining[i] : selected_cols[0];
                    out_vec(static_cast<Index>(i)) = matrix_at<int>(static_cast<Index>(row))(r, c);
                }
            }
            return out;
        }
        if (data_type_ == DataType::kComplex) {
            DataSeries out(DataType::kComplex, DataShape::Vector(static_cast<Index>(remaining.size())));
            out.resize(size());
            for (std::size_t row = 0; row < size(); ++row) {
                auto out_vec = out.vector_at<std::complex<double> >(static_cast<Index>(row));
                for (std::size_t i = 0; i < remaining.size(); ++i) {
                    const Index r = select_columns ? selected_rows[0] : remaining[i];
                    const Index c = select_columns ? remaining[i] : selected_cols[0];
                    out_vec(static_cast<Index>(i)) = matrix_at<std::complex<double> >(static_cast<Index>(row))(r, c);
                }
            }
            return out;
        }

        DataSeries out(DataType::kString, DataShape::Vector(static_cast<Index>(remaining.size())));
        out.resize(size());
        for (std::size_t row = 0; row < size(); ++row) {
            auto& out_vec = out.vector_at<std::string>(static_cast<Index>(row));
            for (std::size_t i = 0; i < remaining.size(); ++i) {
                const Index r = select_columns ? selected_rows[0] : remaining[i];
                const Index c = select_columns ? remaining[i] : selected_cols[0];
                out_vec(static_cast<Index>(i)) = matrix_at<std::string>(static_cast<Index>(row))(r, c);
            }
        }
        return out;
    }

    if (data_type_ == DataType::kReal) {
        return at_matrix_numeric_impl<double>(selected_rows, selected_cols);
    }
    if (data_type_ == DataType::kInteger) {
        return at_matrix_numeric_impl<int>(selected_rows, selected_cols);
    }
    if (data_type_ == DataType::kComplex) {
        return at_matrix_numeric_impl<std::complex<double> >(selected_rows, selected_cols);
    }
    return at_matrix_string_impl(selected_rows, selected_cols);
}

// =========================================================================
// DataSeries -- at (private helpers)
// =========================================================================

DataSeries DataSeries::at_vector_string_impl(const std::vector<Index>& selected) const {
    DataSeries out(DataType::kString, DataShape::Vector(static_cast<Index>(selected.size())));
    out.resize(size());
    for (std::size_t row = 0; row < size(); ++row) {
        auto out_vec = out.vector_at<std::string>(static_cast<Index>(row));
        for (std::size_t i = 0; i < selected.size(); ++i) {
            out_vec(static_cast<Index>(i)) = vector_at<std::string>(static_cast<Index>(row))(selected[i]);
        }
    }
    out.unit_ = unit_;
    return out;
}

DataSeries DataSeries::at_matrix_string_impl(
    const std::vector<Index>& selected_rows,
    const std::vector<Index>& selected_cols) const {

    DataSeries out(DataType::kString,
                   {static_cast<Index>(selected_rows.size()),
                    static_cast<Index>(selected_cols.size())});
    out.resize(size());

    for (std::size_t row = 0; row < size(); ++row) {
        auto& out_mat = out.matrix_at<std::string>(static_cast<Index>(row));
        for (std::size_t r = 0; r < selected_rows.size(); ++r) {
            for (std::size_t c = 0; c < selected_cols.size(); ++c) {
                out_mat(static_cast<Index>(r), static_cast<Index>(c)) =
                    matrix_at<std::string>(static_cast<Index>(row))(selected_rows[r], selected_cols[c]);
            }
        }
    }
    out.unit_ = unit_;
    return out;
}

// =========================================================================
// DataSeries -- private: validate_schema / make_storage
// =========================================================================

void DataSeries::validate_schema(const DataShape& shape) {
    if (shape.size() > 2) {
        throw std::invalid_argument("data shape must have at most 2 dimensions");
    }
    if (shape.size() == 1 && shape[0] < 0) {
        throw std::invalid_argument("vector schema must have non-negative dimension");
    }
    if (shape.size() == 2 && (shape[0] < 0 || shape[1] < 0)) {
        throw std::invalid_argument("matrix schema must have non-negative dimensions");
    }
}

std::unique_ptr<SeriesStorage> DataSeries::make_storage(DataType dtype, const DataShape& shape) {
    validate_schema(shape);
    DataKind kind = shape.kind();
    if (kind == DataKind::kScalar) {
        if (dtype == DataType::kReal) return std::unique_ptr<SeriesStorage>(new ScalarSeriesStorage<double>());
        if (dtype == DataType::kInteger) return std::unique_ptr<SeriesStorage>(new ScalarSeriesStorage<int>());
        if (dtype == DataType::kComplex) return std::unique_ptr<SeriesStorage>(new ScalarSeriesStorage<std::complex<double> >());
        return std::unique_ptr<SeriesStorage>(new ScalarSeriesStorage<std::string>());
    }

    if (kind == DataKind::kVector) {
        if (dtype == DataType::kReal) return std::unique_ptr<SeriesStorage>(new VectorNumericSeriesStorage<double>(shape[0]));
        if (dtype == DataType::kInteger) return std::unique_ptr<SeriesStorage>(new VectorNumericSeriesStorage<int>(shape[0]));
        if (dtype == DataType::kComplex) return std::unique_ptr<SeriesStorage>(new VectorNumericSeriesStorage<std::complex<double> >(shape[0]));
        return std::unique_ptr<SeriesStorage>(new VectorStringSeriesStorage(shape[0]));
    }

    if (dtype == DataType::kReal) return std::unique_ptr<SeriesStorage>(new MatrixNumericSeriesStorage<double>(shape[0], shape[1]));
    if (dtype == DataType::kInteger) return std::unique_ptr<SeriesStorage>(new MatrixNumericSeriesStorage<int>(shape[0], shape[1]));
    if (dtype == DataType::kComplex) return std::unique_ptr<SeriesStorage>(new MatrixNumericSeriesStorage<std::complex<double> >(shape[0], shape[1]));
    return std::unique_ptr<SeriesStorage>(new MatrixStringSeriesStorage(shape[0], shape[1]));
}

// =========================================================================
// DataSeries -- private: string storage accessors
// =========================================================================

VectorStringSeriesStorage* DataSeries::vector_storage_string() {
    if (shape_.kind() != DataKind::kVector || data_type_ != DataType::kString) throw std::bad_cast();
    return static_cast<VectorStringSeriesStorage*>(storage_.get());
}

const VectorStringSeriesStorage* DataSeries::vector_storage_string() const {
    if (shape_.kind() != DataKind::kVector || data_type_ != DataType::kString) throw std::bad_cast();
    return static_cast<const VectorStringSeriesStorage*>(storage_.get());
}

MatrixStringSeriesStorage* DataSeries::matrix_storage_string() {
    if (shape_.kind() != DataKind::kMatrix || data_type_ != DataType::kString) throw std::bad_cast();
    return static_cast<MatrixStringSeriesStorage*>(storage_.get());
}

const MatrixStringSeriesStorage* DataSeries::matrix_storage_string() const {
    if (shape_.kind() != DataKind::kMatrix || data_type_ != DataType::kString) throw std::bad_cast();
    return static_cast<const MatrixStringSeriesStorage*>(storage_.get());
}

// =========================================================================
// DataSeries -- private: fill helpers (non-template overloads)
// =========================================================================

void DataSeries::fill_vector_row(Index row, const std::string& val, std::true_type) {
    VecXs& t = vector_at<std::string>(row);
    for (Index i = 0; i < t.dimension(0); ++i) t(i) = val;
}

void DataSeries::fill_matrix_row(Index row, const std::string& val, std::true_type) {
    MatXs& t = matrix_at<std::string>(row);
    for (Index r = 0; r < t.dimension(0); ++r) {
        for (Index c = 0; c < t.dimension(1); ++c) {
            t(r, c) = val;
        }
    }
}

// =========================================================================
// DataSeries -- static factories (non-template overloads)
// =========================================================================

DataSeries DataSeries::CreateScalarFromVector(const std::vector<std::string>& values,
                                             const Unit& u) {
    DataSeries s(DataType::kString, DataShape::Scalar());
    s.set_unit(u);
    s.resize(values.size());
    for (std::size_t i = 0; i < values.size(); ++i)
        s.scalar_at<std::string>(static_cast<Index>(i)) = values[i];
    return s;
}

DataSeries DataSeries::CreateVectorFromVector(Index width, const std::vector<std::string>& values,
                                             const Unit& u) {
    if (width < 0)
        throw std::invalid_argument("vector width must be non-negative");
    const std::size_t w = static_cast<std::size_t>(width);
    if (w == 0) {
        if (!values.empty())
            throw std::invalid_argument("vector width 0 requires empty data");
        DataSeries s(DataType::kString, DataShape::Vector(width));
        s.set_unit(u);
        return s;
    }
    if (values.size() % w != 0)
        throw std::invalid_argument("vector flat data length must be a multiple of width");
    DataSeries s(DataType::kString, DataShape::Vector(width));
    s.set_unit(u);
    s.resize(values.size() / w);
    for (std::size_t i = 0; i < s.size(); ++i)
        for (Index j = 0; j < width; ++j)
            s.vector_at<std::string>(static_cast<Index>(i))(j) = values[i * w + j];
    return s;
}

DataSeries DataSeries::CreateMatrixFromVector(Index cell_rows, Index cell_cols,
                                             const std::vector<std::string>& values,
                                             const Unit& u) {
    if (cell_rows < 0 || cell_cols < 0)
        throw std::invalid_argument("matrix shape must be non-negative");
    const std::size_t elems = static_cast<std::size_t>(cell_rows) *
                              static_cast<std::size_t>(cell_cols);
    if (elems == 0) {
        if (!values.empty())
            throw std::invalid_argument("zero-sized matrix cells require empty data");
        DataSeries s(DataType::kString, DataShape::Matrix(cell_rows, cell_cols));
        s.set_unit(u);
        return s;
    }
    if (values.size() % elems != 0)
        throw std::invalid_argument("matrix flat data length must be a multiple of cell_rows * cell_cols");
    DataSeries s(DataType::kString, DataShape::Matrix(cell_rows, cell_cols));
    s.set_unit(u);
    s.resize(values.size() / elems);
    for (std::size_t i = 0; i < s.size(); ++i)
        for (Index r = 0; r < cell_rows; ++r)
            for (Index c = 0; c < cell_cols; ++c)
                s.matrix_at<std::string>(static_cast<Index>(i))(r, c) =
                    values[i * elems + static_cast<std::size_t>(r) *
                     static_cast<std::size_t>(cell_cols) + static_cast<std::size_t>(c)];
    return s;
}

DataSeries DataSeries::CreateVectorFromNestedVector(const std::vector<std::vector<std::string>>& rows,
                                                   const Unit& u) {
    if (rows.empty()) {
        DataSeries s(DataType::kString, DataShape::Vector(0));
        s.set_unit(u);
        return s;
    }
    const Index width = static_cast<Index>(rows[0].size());
    DataSeries s(DataType::kString, DataShape::Vector(width));
    s.set_unit(u);
    s.resize(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (static_cast<Index>(rows[i].size()) != width)
            throw std::invalid_argument("all vector rows must have the same width");
        for (Index j = 0; j < width; ++j)
            s.vector_at<std::string>(static_cast<Index>(i))(j) = rows[i][static_cast<std::size_t>(j)];
    }
    return s;
}

DataSeries DataSeries::CreateMatrixFromNestedVector(
    const std::vector<std::vector<std::vector<std::string>>>& rows,
    const Unit& u) {
    if (rows.empty()) {
        DataSeries s(DataType::kString, DataShape::Matrix(0, 0));
        s.set_unit(u);
        return s;
    }
    const Index cell_rows = static_cast<Index>(rows[0].size());
    const Index cell_cols = cell_rows > 0 ? static_cast<Index>(rows[0][0].size()) : 0;
    DataSeries s(DataType::kString, DataShape::Matrix(cell_rows, cell_cols));
    s.set_unit(u);
    s.resize(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (static_cast<Index>(rows[i].size()) != cell_rows)
            throw std::invalid_argument("all matrix rows must have the same shape");
        for (Index r = 0; r < cell_rows; ++r) {
            if (static_cast<Index>(rows[i][static_cast<std::size_t>(r)].size()) != cell_cols)
                throw std::invalid_argument("all matrix rows must have the same shape");
            for (Index c = 0; c < cell_cols; ++c)
                s.matrix_at<std::string>(static_cast<Index>(i))(r, c) =
                    rows[i][static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
        }
    }
    return s;
}

}  // namespace xdataset
