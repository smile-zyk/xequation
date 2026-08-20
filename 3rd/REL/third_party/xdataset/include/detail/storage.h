#ifndef XDATASET_DETAIL_STORAGE_H_
#define XDATASET_DETAIL_STORAGE_H_

#include <Eigen/Dense>
#include <unsupported/Eigen/CXX11/Tensor>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

#include "xdataset_predefine.h"

namespace xdataset {

// ---------------------------------------------------------------------------
//  Type-traits helpers
// ---------------------------------------------------------------------------

template <typename T>
struct NumericVectorTypes {
    typedef Vec<T>                   OwnedType;
    typedef VecMap<T>                MapType;
    typedef VecConstMap<T>           ConstMapType;
};

template <typename T>
struct NumericMatrixTypes {
    typedef Mat<T>                   OwnedType;
    typedef MatMap<T>                MapType;
    typedef MatConstMap<T>           ConstMapType;
};

// ---------------------------------------------------------------------------
//  Cell storage hierarchy (internal, not exposed to end users)
// ---------------------------------------------------------------------------

class SeriesStorage {
public:
    virtual ~SeriesStorage() {}
    virtual std::size_t size() const = 0;
    virtual void resize(std::size_t rows) = 0;
    virtual std::unique_ptr<SeriesStorage> clone() const = 0;
};

template <typename T>
class ScalarSeriesStorage : public SeriesStorage {
public:
    std::size_t size() const override { return values_.size(); }

    void resize(std::size_t rows) override {
        values_.resize(rows, T());
    }

    std::unique_ptr<SeriesStorage> clone() const override {
        return std::unique_ptr<SeriesStorage>(new ScalarSeriesStorage<T>(*this));
    }

    T& value(Index row) { return values_[static_cast<std::size_t>(row)]; }
    const T& value(Index row) const { return values_[static_cast<std::size_t>(row)]; }

    void append(const T& v) { values_.push_back(v); }

    T* data() { return values_.empty() ? nullptr : &values_[0]; }
    const T* data() const { return values_.empty() ? nullptr : &values_[0]; }

private:
    std::vector<T> values_;
};

template <typename T>
class VectorNumericSeriesStorage : public SeriesStorage {
public:
    typedef typename NumericVectorTypes<T>::OwnedType OwnedType;
    typedef typename NumericVectorTypes<T>::MapType MapType;
    typedef typename NumericVectorTypes<T>::ConstMapType ConstMapType;

    explicit VectorNumericSeriesStorage(Index width) : width_(width), rows_(0) {}

    std::size_t size() const override { return rows_; }

    void resize(std::size_t rows) override {
        rows_ = rows;
        values_.resize(static_cast<std::size_t>(width_) * rows_, T());
    }

    std::unique_ptr<SeriesStorage> clone() const override {
        return std::unique_ptr<SeriesStorage>(new VectorNumericSeriesStorage<T>(*this));
    }

    MapType value(Index row) {
        return MapType(ptr_for_row(row), width_);
    }

    ConstMapType value(Index row) const {
        return ConstMapType(ptr_for_row(row), width_);
    }

    void set(Index row, const OwnedType& v) {
        if (v.size() != width_) throw std::bad_cast();
        value(row) = v;
    }

    void append(const OwnedType& v) {
        if (v.size() != width_) throw std::bad_cast();
        values_.reserve(values_.size() + static_cast<std::size_t>(width_));
        for (Index i = 0; i < width_; ++i) values_.push_back(v(i));
        ++rows_;
    }

    T* data() { return values_.empty() ? nullptr : &values_[0]; }
    const T* data() const { return values_.empty() ? nullptr : &values_[0]; }

private:
    T* ptr_for_row(Index row) {
        return data() + static_cast<std::size_t>(width_) * static_cast<std::size_t>(row);
    }

    const T* ptr_for_row(Index row) const {
        return data() + static_cast<std::size_t>(width_) * static_cast<std::size_t>(row);
    }

    Index width_;
    std::size_t rows_;
    std::vector<T> values_;
};

class VectorStringSeriesStorage : public SeriesStorage {
public:
    explicit VectorStringSeriesStorage(Index width) : width_(width) {}

    std::size_t size() const override { return rows_.size(); }

    void resize(std::size_t rows) override {
        if (rows <= rows_.size()) {
            rows_.resize(rows);
            return;
        }
        rows_.resize(rows, default_row());
    }

    std::unique_ptr<SeriesStorage> clone() const override {
        return std::unique_ptr<SeriesStorage>(new VectorStringSeriesStorage(*this));
    }

    VecXs& value(Index row) { return rows_[static_cast<std::size_t>(row)]; }
    const VecXs& value(Index row) const { return rows_[static_cast<std::size_t>(row)]; }

    void append(const VecXs& row) { rows_.push_back(row); }

private:
    VecXs default_row() const {
        VecXs out(width_);
        return out;
    }

    Index width_;
    std::vector<VecXs> rows_;
};

template <typename T>
class MatrixNumericSeriesStorage : public SeriesStorage {
public:
    typedef typename NumericMatrixTypes<T>::OwnedType OwnedType;
    typedef typename NumericMatrixTypes<T>::MapType MapType;
    typedef typename NumericMatrixTypes<T>::ConstMapType ConstMapType;

    MatrixNumericSeriesStorage(Index rows, Index cols)
        : cell_rows_(rows), cell_cols_(cols), row_count_(0) {}

    std::size_t size() const override { return row_count_; }

    void resize(std::size_t rows) override {
        row_count_ = rows;
        values_.resize(static_cast<std::size_t>(cell_rows_ * cell_cols_) * row_count_, T());
    }

    std::unique_ptr<SeriesStorage> clone() const override {
        return std::unique_ptr<SeriesStorage>(new MatrixNumericSeriesStorage<T>(*this));
    }

    MapType value(Index row) {
        return MapType(ptr_for_row(row), cell_rows_, cell_cols_);
    }

    ConstMapType value(Index row) const {
        return ConstMapType(ptr_for_row(row), cell_rows_, cell_cols_);
    }

    void set(Index row, const OwnedType& m) {
        if (m.rows() != cell_rows_ || m.cols() != cell_cols_) throw std::bad_cast();
        value(row) = m;
    }

    void append(const OwnedType& m) {
        if (m.rows() != cell_rows_ || m.cols() != cell_cols_) throw std::bad_cast();
        const std::size_t elems = static_cast<std::size_t>(cell_rows_ * cell_cols_);
        values_.reserve(values_.size() + elems);
        for (Index r = 0; r < cell_rows_; ++r) {
            for (Index c = 0; c < cell_cols_; ++c) {
                values_.push_back(m(r, c));
            }
        }
        ++row_count_;
    }

    T* data() { return values_.empty() ? nullptr : &values_[0]; }
    const T* data() const { return values_.empty() ? nullptr : &values_[0]; }

private:
    T* ptr_for_row(Index row) {
        return data() + static_cast<std::size_t>(cell_rows_ * cell_cols_) * static_cast<std::size_t>(row);
    }

    const T* ptr_for_row(Index row) const {
        return data() + static_cast<std::size_t>(cell_rows_ * cell_cols_) * static_cast<std::size_t>(row);
    }

    Index cell_rows_;
    Index cell_cols_;
    std::size_t row_count_;
    std::vector<T> values_;
};

class MatrixStringSeriesStorage : public SeriesStorage {
public:
    MatrixStringSeriesStorage(Index rows, Index cols) : cell_rows_(rows), cell_cols_(cols) {}

    std::size_t size() const override { return rows_.size(); }

    void resize(std::size_t rows) override {
        if (rows <= rows_.size()) {
            rows_.resize(rows);
            return;
        }
        rows_.resize(rows, default_row());
    }

    std::unique_ptr<SeriesStorage> clone() const override {
        return std::unique_ptr<SeriesStorage>(new MatrixStringSeriesStorage(*this));
    }

    MatXs& value(Index row) { return rows_[static_cast<std::size_t>(row)]; }
    const MatXs& value(Index row) const { return rows_[static_cast<std::size_t>(row)]; }

    void append(const MatXs& row) { rows_.push_back(row); }

private:
    MatXs default_row() const {
        MatXs out(cell_rows_, cell_cols_);
        return out;
    }

    Index cell_rows_;
    Index cell_cols_;
    std::vector<MatXs> rows_;
};

}  // namespace xdataset

#endif  // XDATASET_DETAIL_STORAGE_H_
