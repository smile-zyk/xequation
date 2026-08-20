#ifndef XDATASET_DATA_SERIES_H_
#define XDATASET_DATA_SERIES_H_

#include <Eigen/Dense>
#include <unsupported/Eigen/CXX11/Tensor>

#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "detail/storage.h"
#include "data_shape.h"
#include "measurement.h"
#include "unit.h"
#include "xdataset_predefine.h"

namespace xdataset {

class DataSeries;

class XDATASET_API DataSeries {
public:
    class RowView;
    class ConstRowView;

    class iterator {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef RowView value_type;
        typedef std::ptrdiff_t difference_type;
        typedef void pointer;
        typedef RowView reference;

        iterator() : owner_(nullptr), idx_(0) {}
        iterator(DataSeries* owner, Index idx) : owner_(owner), idx_(idx) {}

        reference operator*() const;

        iterator& operator++() {
            ++idx_;
            return *this;
        }

        iterator operator++(int) {
            iterator tmp(*this);
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& rhs) const {
            return owner_ == rhs.owner_ && idx_ == rhs.idx_;
        }

        bool operator!=(const iterator& rhs) const { return !(*this == rhs); }

    private:
        DataSeries* owner_;
        Index idx_;
    };

    class const_iterator {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef ConstRowView value_type;
        typedef std::ptrdiff_t difference_type;
        typedef void pointer;
        typedef ConstRowView reference;

        const_iterator() : owner_(nullptr), idx_(0) {}
        const_iterator(const DataSeries* owner, Index idx) : owner_(owner), idx_(idx) {}

        reference operator*() const;

        const_iterator& operator++() {
            ++idx_;
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp(*this);
            ++(*this);
            return tmp;
        }

        bool operator==(const const_iterator& rhs) const {
            return owner_ == rhs.owner_ && idx_ == rhs.idx_;
        }

        bool operator!=(const const_iterator& rhs) const { return !(*this == rhs); }

    private:
        const DataSeries* owner_;
        Index idx_;
    };

    DataSeries();

    DataSeries(DataType dtype, const DataShape& shape);

    DataSeries(const DataSeries& other);

    DataSeries& operator=(const DataSeries& other);

    DataSeries(DataSeries&& other) noexcept;

    DataSeries& operator=(DataSeries&& other) noexcept;

    // -----------------------------------------------------------------------
    //  Factory: Create* -> pre-allocate rows (n = 0 -> no allocation)
    // -----------------------------------------------------------------------

    template <typename T>
    static DataSeries CreateScalar(std::size_t rows = 0,
                                   const Unit& u = Unit(),
                                   const T& fill_val = T()) {
        static_assert(IsSupported<T>::value, "unsupported type");
        DataSeries s(DataTypeOf<T>::tag, DataShape::Scalar());
        s.set_unit(u);
        s.resize(rows);
        if (rows > 0) s.fill(fill_val);
        return s;
    }

    template <typename T>
    static DataSeries CreateVector(Index width, std::size_t rows = 0,
                                   const Unit& u = Unit(),
                                   const T& fill_val = T()) {
        static_assert(IsSupported<T>::value, "unsupported type");
        DataSeries s(DataTypeOf<T>::tag, DataShape::Vector(width));
        s.set_unit(u);
        s.resize(rows);
        if (rows > 0) s.fill(fill_val);
        return s;
    }

    template <typename T>
    static DataSeries CreateMatrix(Index cell_rows, Index cell_cols,
                                   std::size_t n = 0,
                                   const Unit& u = Unit(),
                                   const T& fill_val = T()) {
        static_assert(IsSupported<T>::value, "unsupported type");
        DataSeries s(DataTypeOf<T>::tag, DataShape::Matrix(cell_rows, cell_cols));
        s.set_unit(u);
        s.resize(n);
        if (n > 0) s.fill(fill_val);
        return s;
    }

    // -----------------------------------------------------------------------
    //  Factory: Create*FromMemory -> raw-pointer bulk copy (not for string)
    // -----------------------------------------------------------------------

    template <typename T>
    static typename std::enable_if<!std::is_same<T, std::string>::value, DataSeries>::type
    CreateScalarFromMemory(const T* values, std::size_t len,
                           const Unit& u = Unit()) {
        static_assert(IsSupported<T>::value, "unsupported type");
        if (len > 0 && values == nullptr)
            throw std::invalid_argument("values pointer must not be null when len > 0");
        DataSeries s(DataTypeOf<T>::tag, DataShape::Scalar());
        s.set_unit(u);
        s.resize(len);
        std::copy(values, values + len, s.mutable_contiguous_data<T>());
        return s;
    }

    template <typename T>
    static typename std::enable_if<!std::is_same<T, std::string>::value, DataSeries>::type
    CreateVectorFromMemory(Index width, const T* values, std::size_t len,
                           const Unit& u = Unit()) {
        if (width < 0)
            throw std::invalid_argument("vector width must be non-negative");
        if (len > 0 && values == nullptr)
            throw std::invalid_argument("values pointer must not be null when len > 0");
        const std::size_t w = static_cast<std::size_t>(width);
        if (w == 0) {
            if (len != 0)
                throw std::invalid_argument("vector width 0 requires len = 0");
            DataSeries s(DataTypeOf<T>::tag, DataShape::Vector(width));
            s.set_unit(u);
            return s;
        }
        if (len % w != 0)
            throw std::invalid_argument("vector flat data length must be a multiple of width");
        DataSeries s(DataTypeOf<T>::tag, DataShape::Vector(width));
        s.set_unit(u);
        s.resize(len / w);
        std::copy(values, values + len, s.mutable_contiguous_data<T>());
        return s;
    }

    template <typename T>
    static typename std::enable_if<!std::is_same<T, std::string>::value, DataSeries>::type
    CreateMatrixFromMemory(Index cell_rows, Index cell_cols, const T* values, std::size_t len,
                           const Unit& u = Unit()) {
        if (cell_rows < 0 || cell_cols < 0)
            throw std::invalid_argument("matrix shape must be non-negative");
        if (len > 0 && values == nullptr)
            throw std::invalid_argument("values pointer must not be null when len > 0");
        const std::size_t elems = static_cast<std::size_t>(cell_rows) *
                                  static_cast<std::size_t>(cell_cols);
        if (elems == 0) {
            if (len != 0)
                throw std::invalid_argument("zero-sized matrix cells require len = 0");
            DataSeries s(DataTypeOf<T>::tag, DataShape::Matrix(cell_rows, cell_cols));
            s.set_unit(u);
            return s;
        }
        if (len % elems != 0)
            throw std::invalid_argument("matrix flat data length must be a multiple of cell_rows * cell_cols");
        DataSeries s(DataTypeOf<T>::tag, DataShape::Matrix(cell_rows, cell_cols));
        s.set_unit(u);
        s.resize(len / elems);
        std::copy(values, values + len, s.mutable_contiguous_data<T>());
        return s;
    }

    // -----------------------------------------------------------------------
    //  Factory: Create*FromVector -> flat vector-based
    // -----------------------------------------------------------------------

    template <typename T>
    static DataSeries CreateScalarFromVector(const std::vector<T>& values,
                                             const Unit& u = Unit()) {
        return CreateScalarFromMemory<T>(
            values.empty() ? static_cast<const T*>(nullptr) : &values[0],
            values.size(), u);
    }

    static DataSeries CreateScalarFromVector(const std::vector<std::string>& values,
                                             const Unit& u = Unit());

    template <typename T>
    static typename std::enable_if<!std::is_same<T, std::string>::value, DataSeries>::type
    CreateVectorFromVector(Index width, const std::vector<T>& values,
                           const Unit& u = Unit()) {
        return CreateVectorFromMemory<T>(width,
            values.empty() ? static_cast<const T*>(nullptr) : &values[0],
            values.size(), u);
    }

    static DataSeries CreateVectorFromVector(Index width, const std::vector<std::string>& values,
                                             const Unit& u = Unit());

    template <typename T>
    static typename std::enable_if<!std::is_same<T, std::string>::value, DataSeries>::type
    CreateMatrixFromVector(Index cell_rows, Index cell_cols, const std::vector<T>& values,
                           const Unit& u = Unit()) {
        return CreateMatrixFromMemory<T>(cell_rows, cell_cols,
            values.empty() ? static_cast<const T*>(nullptr) : &values[0],
            values.size(), u);
    }

    static DataSeries CreateMatrixFromVector(Index cell_rows, Index cell_cols,
                                             const std::vector<std::string>& values,
                                             const Unit& u = Unit());

    // -----------------------------------------------------------------------
    //  Factory: Create*FromNestedVector -> nested vector-of-vectors
    // -----------------------------------------------------------------------

    template <typename T>
    static typename std::enable_if<!std::is_same<T, std::string>::value, DataSeries>::type
    CreateVectorFromNestedVector(const std::vector<std::vector<T>>& rows,
                                 const Unit& u = Unit()) {
        if (rows.empty()) {
            DataSeries s(DataTypeOf<T>::tag, DataShape::Vector(0));
            s.set_unit(u);
            return s;
        }
        const Index width = static_cast<Index>(rows[0].size());
        DataSeries s(DataTypeOf<T>::tag, DataShape::Vector(width));
        s.set_unit(u);
        s.resize(rows.size());
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (static_cast<Index>(rows[i].size()) != width)
                throw std::invalid_argument("all vector rows must have the same width");
            for (Index j = 0; j < width; ++j)
                s.vector_at<T>(static_cast<Index>(i))(j) = rows[i][static_cast<std::size_t>(j)];
        }
        return s;
    }

    static DataSeries CreateVectorFromNestedVector(const std::vector<std::vector<std::string>>& rows,
                                                   const Unit& u = Unit());

    template <typename T>
    static typename std::enable_if<!std::is_same<T, std::string>::value, DataSeries>::type
    CreateMatrixFromNestedVector(const std::vector<std::vector<std::vector<T>>>& rows,
                                 const Unit& u = Unit()) {
        if (rows.empty()) {
            DataSeries s(DataTypeOf<T>::tag, DataShape::Matrix(0, 0));
            s.set_unit(u);
            return s;
        }
        const Index cell_rows = static_cast<Index>(rows[0].size());
        const Index cell_cols = cell_rows > 0 ? static_cast<Index>(rows[0][0].size()) : 0;
        DataSeries s(DataTypeOf<T>::tag, DataShape::Matrix(cell_rows, cell_cols));
        s.set_unit(u);
        s.resize(rows.size());
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (static_cast<Index>(rows[i].size()) != cell_rows)
                throw std::invalid_argument("all matrix rows must have the same shape");
            for (Index r = 0; r < cell_rows; ++r) {
                if (static_cast<Index>(rows[i][static_cast<std::size_t>(r)].size()) != cell_cols)
                    throw std::invalid_argument("all matrix rows must have the same shape");
                for (Index c = 0; c < cell_cols; ++c)
                    s.matrix_at<T>(static_cast<Index>(i))(r, c) =
                        rows[i][static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
            }
        }
        return s;
    }

    static DataSeries CreateMatrixFromNestedVector(
        const std::vector<std::vector<std::vector<std::string>>>& rows,
        const Unit& u = Unit());

    // -----------------------------------------------------------------------
    //  Factory: CreateFromMeasurements -> batch create from vector<Measurement>
    // -----------------------------------------------------------------------
    //
    //  Determines the output schema (kind/dtype/shape/unit) from the first
    //  element.  Dispatches the type ONCE, then writes all rows in a tight
    //  loop with zero per-row overhead.  An empty vector yields a default
    //  (kScalar, kReal) series.
    //
    static DataSeries CreateFromMeasurements(const std::vector<Measurement>& measurements);

    std::size_t size() const { return storage_->size(); }
    bool empty() const { return size() == 0; }

    DataKind data_kind() const { return shape_.kind(); }
    DataType data_type() const { return data_type_; }
    DataShape data_shape() const { return shape_; }
    const Unit& unit() const { return unit_; }

    //---- unit assignment & canonicalisation ----------------------------

    // Assign a unit to this series.  No value conversion is performed --
    // the existing numbers are simply tagged with the given unit.
    void set_unit(const Unit& u) {
        if (data_type_ == DataType::kString && u.has_dimension())
            throw std::invalid_argument("string series cannot have a named unit");
        unit_ = u;
    }

    void set_unit(const std::string& s);

    // Convert values in-place to canonical SI (multiplier absorbed,
    // unit = base_units).  Affine units (degC/degF) are handled via
    // units::convert.  Fast path when multiplier == 1 and non-affine.
    void canonicalize();

    // Return a canonicalised copy without modifying *this.
    DataSeries canonicalized() const;

    /// Promote the dtype: int -> real -> complex.  No-op if
    /// already at or above target.  String is not promotable.
    /// Returns a new DataSeries; does not modify *this.
    DataSeries promoted_data_type(DataType target) const;

    /// Convert to a logical (int 0/1) DataSeries.
    /// real/complex: 0.0 -> 0, any other value -> 1.
    /// string: non-empty -> 1, empty -> 0.
    /// Returns a new DataSeries with DataType::kInteger.
    DataSeries as_logical() const;

    /// True when the stored unit is already canonical (multiplier == 1, non-affine).
    bool is_canonicalized() const;

    //---------------------------------------------------------------------
    //  transform: apply a callback to every row and return a new DataSeries
    //---------------------------------------------------------------------
    //
    //  The callback receives a Measurement (the cell value with its unit)
    //  and returns a new Measurement.  The output dtype, kind, and shape
    //  are determined from the first row's result and must be consistent
    //  across all rows.  The output type may differ from the input type.
    //
    //  Internally, results are collected into a vector and then batch-written
    //  via CreateFromMeasurements, which dispatches the type ONCE and then
    //  writes all rows in a tight loop with zero per-row overhead.
    //
    //  Example:
    //    auto squared = series.transform([](const Measurement& m) {
    //        return Measurement(m.as_scalar<double>() * m.as_scalar<double>(), m.unit() * m.unit());
    //    });
    //

    template <typename Func>
    DataSeries transform(Func&& callback) const {
        const std::size_t n = size();
        if (n == 0) {
            return DataSeries(data_type_, shape_);
        }

        std::vector<Measurement> results;
        results.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            results.push_back(callback(measurement_at(static_cast<Index>(i))));
        }

        return CreateFromMeasurements(results);
    }

    //---------------------------------------------------------------------

    Index element_count() const { return shape_.element_count(); }

    void resize(std::size_t n) { storage_->resize(n); }
    void clear() { storage_->resize(0); }

    DataSeries head(std::size_t n) const;

    DataSeries tail(std::size_t n) const;

    DataSeries iloc(std::size_t start, std::size_t end) const;

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, static_cast<Index>(size())); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, static_cast<Index>(size())); }

    RowView row(Index i);
    ConstRowView row(Index i) const;

    template <typename T>
    T& scalar_at(Index i) {
        if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("row index out of range");
        return scalar_storage<T>()->value(i);
    }

    template <typename T>
    const T& scalar_at(Index i) const {
        if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("row index out of range");
        return scalar_storage<T>()->value(i);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value,
                            typename NumericVectorTypes<T>::MapType>::type
    vector_at(Index i) {
        if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("row index out of range");
        return vector_storage_numeric<T>()->value(i);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value,
                            typename NumericVectorTypes<T>::ConstMapType>::type
    vector_at(Index i) const {
        if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("row index out of range");
        return vector_storage_numeric<T>()->value(i);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, std::string>::value, VecXs&>::type
    vector_at(Index i) {
        if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("row index out of range");
        return vector_storage_string()->value(i);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, std::string>::value, const VecXs&>::type
    vector_at(Index i) const {
        if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("row index out of range");
        return vector_storage_string()->value(i);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value,
                            typename NumericMatrixTypes<T>::MapType>::type
    matrix_at(Index i) {
        if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("row index out of range");
        return matrix_storage_numeric<T>()->value(i);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value,
                            typename NumericMatrixTypes<T>::ConstMapType>::type
    matrix_at(Index i) const {
        if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("row index out of range");
        return matrix_storage_numeric<T>()->value(i);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, std::string>::value, MatXs&>::type
    matrix_at(Index i) {
        if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("row index out of range");
        return matrix_storage_string()->value(i);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, std::string>::value, const MatXs&>::type
    matrix_at(Index i) const {
        if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("row index out of range");
        return matrix_storage_string()->value(i);
    }

    // ---- append helpers (internal) -----------------------------------

    template <typename T>
    void append_scalar(const T& val) {
        scalar_storage<T>()->append(val);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value, void>::type
    append_vector(const typename NumericVectorTypes<T>::OwnedType& v) {
        if (v.size() != element_count()) throw std::bad_cast();
        vector_storage_numeric<T>()->append(v);
    }

    void append_vector(const VecXs& v);

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value, void>::type
    append_matrix(const typename NumericMatrixTypes<T>::OwnedType& m) {
        if (m.rows() != shape_[0] || m.cols() != shape_[1]) throw std::bad_cast();
        matrix_storage_numeric<T>()->append(m);
    }

    void append_matrix(const MatXs& m);

    void append_from(const DataSeries& src, Index row);

    void assign_from(const DataSeries& src, Index src_row, Index dst_row);

    void append(const Measurement& m);

    template <typename T>
    void fill(const T& val) {
        if (data_type_ != DataTypeOf<T>::tag) throw std::bad_cast();
        for (std::size_t i = 0; i < size(); ++i) {
            if (shape_.kind() == DataKind::kScalar) {
                scalar_at<T>(static_cast<Index>(i)) = val;
            } else if (shape_.kind() == DataKind::kVector) {
                fill_vector_row(static_cast<Index>(i), val, std::is_same<T, std::string>());
            } else {
                fill_matrix_row(static_cast<Index>(i), val, std::is_same<T, std::string>());
            }
        }
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value, T*>::type mutable_contiguous_data() {
        if (data_type_ != DataTypeOf<T>::tag) throw std::bad_cast();
        if (shape_.kind() == DataKind::kScalar) return scalar_storage<T>()->data();
        if (shape_.kind() == DataKind::kVector) return vector_storage_numeric<T>()->data();
        return matrix_storage_numeric<T>()->data();
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value, const T*>::type contiguous_data() const {
        if (data_type_ != DataTypeOf<T>::tag) throw std::bad_cast();
        if (shape_.kind() == DataKind::kScalar) return scalar_storage<T>()->data();
        if (shape_.kind() == DataKind::kVector) return vector_storage_numeric<T>()->data();
        return matrix_storage_numeric<T>()->data();
    }

    std::size_t contiguous_elements() const {
        return size() * static_cast<std::size_t>(element_count());
    }

    bool is_trivially_copyable() const {
        return data_type_ != DataType::kString;
    }

    std::size_t contiguous_bytes() const;

    Measurement measurement_at(Index i) const;

    DataSeries at(const std::vector<Index>& selected) const;

    DataSeries at(
        const std::vector<Index>& selected_rows,
        const std::vector<Index>& selected_cols) const;

private:
    template <typename T>
    DataSeries at_vector_numeric_impl(const std::vector<Index>& selected) const {
        DataSeries out(DataTypeOf<T>::tag, DataShape::Vector(static_cast<Index>(selected.size())));
        out.resize(size());
        for (std::size_t row = 0; row < size(); ++row) {
            auto out_vec = out.vector_at<T>(static_cast<Index>(row));
            for (std::size_t i = 0; i < selected.size(); ++i) {
                out_vec(static_cast<Index>(i)) = vector_at<T>(static_cast<Index>(row))(selected[i]);
            }
        }
        out.unit_ = unit_;
        return out;
    }

    DataSeries at_vector_string_impl(const std::vector<Index>& selected) const;

    template <typename T>
    DataSeries at_matrix_numeric_impl(
        const std::vector<Index>& selected_rows,
        const std::vector<Index>& selected_cols) const {

        DataSeries out(DataTypeOf<T>::tag,
                       {static_cast<Index>(selected_rows.size()),
                        static_cast<Index>(selected_cols.size())});
        out.resize(size());

        for (std::size_t row = 0; row < size(); ++row) {
            auto out_mat = out.matrix_at<T>(static_cast<Index>(row));
            for (std::size_t r = 0; r < selected_rows.size(); ++r) {
                for (std::size_t c = 0; c < selected_cols.size(); ++c) {
                    out_mat(static_cast<Index>(r), static_cast<Index>(c)) =
                        matrix_at<T>(static_cast<Index>(row))(selected_rows[r], selected_cols[c]);
                }
            }
        }
        out.unit_ = unit_;
        return out;
    }

    DataSeries at_matrix_string_impl(
        const std::vector<Index>& selected_rows,
        const std::vector<Index>& selected_cols) const;

    DataSeries at_vector_impl(const std::vector<Index>& selected) const;
    DataSeries at_matrix_impl(
        const std::vector<Index>& selected_rows,
        const std::vector<Index>& selected_cols) const;

    static void validate_schema(const DataShape& shape);

    static std::unique_ptr<SeriesStorage> make_storage(DataType dtype, const DataShape& shape);

    template <typename T>
    ScalarSeriesStorage<T>* scalar_storage() {
        if (shape_.kind() != DataKind::kScalar || data_type_ != DataTypeOf<T>::tag) throw std::bad_cast();
        return static_cast<ScalarSeriesStorage<T>*>(storage_.get());
    }

    template <typename T>
    const ScalarSeriesStorage<T>* scalar_storage() const {
        if (shape_.kind() != DataKind::kScalar || data_type_ != DataTypeOf<T>::tag) throw std::bad_cast();
        return static_cast<const ScalarSeriesStorage<T>*>(storage_.get());
    }

    template <typename T>
    VectorNumericSeriesStorage<T>* vector_storage_numeric() {
        if (shape_.kind() != DataKind::kVector || data_type_ != DataTypeOf<T>::tag || std::is_same<T, std::string>::value) throw std::bad_cast();
        return static_cast<VectorNumericSeriesStorage<T>*>(storage_.get());
    }

    template <typename T>
    const VectorNumericSeriesStorage<T>* vector_storage_numeric() const {
        if (shape_.kind() != DataKind::kVector || data_type_ != DataTypeOf<T>::tag || std::is_same<T, std::string>::value) throw std::bad_cast();
        return static_cast<const VectorNumericSeriesStorage<T>*>(storage_.get());
    }

    VectorStringSeriesStorage* vector_storage_string();

    const VectorStringSeriesStorage* vector_storage_string() const;

    template <typename T>
    MatrixNumericSeriesStorage<T>* matrix_storage_numeric() {
        if (shape_.kind() != DataKind::kMatrix || data_type_ != DataTypeOf<T>::tag || std::is_same<T, std::string>::value) throw std::bad_cast();
        return static_cast<MatrixNumericSeriesStorage<T>*>(storage_.get());
    }

    template <typename T>
    const MatrixNumericSeriesStorage<T>* matrix_storage_numeric() const {
        if (shape_.kind() != DataKind::kMatrix || data_type_ != DataTypeOf<T>::tag || std::is_same<T, std::string>::value) throw std::bad_cast();
        return static_cast<const MatrixNumericSeriesStorage<T>*>(storage_.get());
    }

    MatrixStringSeriesStorage* matrix_storage_string();

    const MatrixStringSeriesStorage* matrix_storage_string() const;

    template <typename T>
    void fill_vector_row(Index row, const T& val, std::false_type) {
        vector_at<T>(row).setConstant(val);
    }

    void fill_vector_row(Index row, const std::string& val, std::true_type);

    template <typename T>
    void fill_matrix_row(Index row, const T& val, std::false_type) {
        matrix_at<T>(row).setConstant(val);
    }

    void fill_matrix_row(Index row, const std::string& val, std::true_type);

    DataType data_type_;
    DataShape shape_;
    std::unique_ptr<SeriesStorage> storage_;
    Unit unit_;
};

class DataSeries::RowView {
public:
    RowView(DataSeries* owner, Index idx) : owner_(owner), idx_(idx) {}

    DataKind data_kind() const { return checked_owner()->data_kind(); }
    DataType data_type() const { return checked_owner()->data_type(); }
    DataShape data_shape() const { return checked_owner()->data_shape(); }
    Index index() const { return idx_; }

    template <typename T>
    T& scalar() {
        return checked_owner()->scalar_at<T>(idx_);
    }

    template <typename T>
    const T& scalar() const {
        return checked_owner()->scalar_at<T>(idx_);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value,
                            typename NumericVectorTypes<T>::MapType>::type
    vector() {
        return checked_owner()->vector_at<T>(idx_);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value,
                            typename NumericVectorTypes<T>::ConstMapType>::type
    vector() const {
        return checked_owner()->vector_at<T>(idx_);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, std::string>::value, VecXs&>::type
    vector() {
        return checked_owner()->vector_at<std::string>(idx_);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, std::string>::value, const VecXs&>::type
    vector() const {
        return checked_owner()->vector_at<std::string>(idx_);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value,
                            typename NumericMatrixTypes<T>::MapType>::type
    matrix() {
        return checked_owner()->matrix_at<T>(idx_);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value,
                            typename NumericMatrixTypes<T>::ConstMapType>::type
    matrix() const {
        return checked_owner()->matrix_at<T>(idx_);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, std::string>::value, MatXs&>::type
    matrix() {
        return checked_owner()->matrix_at<std::string>(idx_);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, std::string>::value, const MatXs&>::type
    matrix() const {
        return checked_owner()->matrix_at<std::string>(idx_);
    }

    Measurement to_measurement() const { return checked_owner()->measurement_at(idx_); }

private:
    DataSeries* checked_owner() const {
        if (!owner_ || idx_ < 0 || static_cast<std::size_t>(idx_) >= owner_->size()) throw std::out_of_range("row view is invalid");
        return owner_;
    }

    DataSeries* owner_;
    Index idx_;
};

class DataSeries::ConstRowView {
public:
    ConstRowView(const DataSeries* owner, Index idx) : owner_(owner), idx_(idx) {}

    DataKind data_kind() const { return checked_owner()->data_kind(); }
    DataType data_type() const { return checked_owner()->data_type(); }
    DataShape data_shape() const { return checked_owner()->data_shape(); }
    Index index() const { return idx_; }

    template <typename T>
    const T& scalar() const {
        return checked_owner()->scalar_at<T>(idx_);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value,
                            typename NumericVectorTypes<T>::ConstMapType>::type
    vector() const {
        return checked_owner()->vector_at<T>(idx_);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, std::string>::value, const VecXs&>::type
    vector() const {
        return checked_owner()->vector_at<std::string>(idx_);
    }

    template <typename T>
    typename std::enable_if<!std::is_same<T, std::string>::value,
                            typename NumericMatrixTypes<T>::ConstMapType>::type
    matrix() const {
        return checked_owner()->matrix_at<T>(idx_);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, std::string>::value, const MatXs&>::type
    matrix() const {
        return checked_owner()->matrix_at<std::string>(idx_);
    }

    Measurement to_measurement() const { return checked_owner()->measurement_at(idx_); }

private:
    const DataSeries* checked_owner() const {
        if (!owner_ || idx_ < 0 || static_cast<std::size_t>(idx_) >= owner_->size()) throw std::out_of_range("row view is invalid");
        return owner_;
    }

    const DataSeries* owner_;
    Index idx_;
};

inline DataSeries::RowView DataSeries::row(Index i) {
    if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("index out of range");
    return RowView(this, i);
}

inline DataSeries::ConstRowView DataSeries::row(Index i) const {
    if (i < 0 || static_cast<std::size_t>(i) >= size()) throw std::out_of_range("index out of range");
    return ConstRowView(this, i);
}

inline DataSeries::iterator::reference DataSeries::iterator::operator*() const {
    return RowView(owner_, idx_);
}

inline DataSeries::const_iterator::reference DataSeries::const_iterator::operator*() const {
    return ConstRowView(owner_, idx_);
}

}  // namespace xdataset

#endif  // XDATASET_DATA_SERIES_H_
