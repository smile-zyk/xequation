#ifndef XDATASET_TOUCHSTONE_IO_H
#define XDATASET_TOUCHSTONE_IO_H

// =========================================================================
// Touchstone IO class declarations (private to the library build).
//
// These concrete format implementations are NOT part of the public API.
// They are instantiated only by the format factories (DataArrayIO, DatasetIO),
// so their definitions must be visible to src/data_array_io.cc and
// src/hdf5_io.cc as well as src/touchstone_io.cc.
// =========================================================================

#include "data_array_io.h"
#include "dataset_io.h"

#include <memory>
#include <string>

namespace xdataset
{

// =========================================================================
// TouchstoneDataArrayWriter -- write a DataArray to a Touchstone (.sNp)
// =========================================================================

class TouchstoneDataArrayWriter : public IDataArrayWriter
{
public:
    explicit TouchstoneDataArrayWriter(const std::string& file_path);
    ~TouchstoneDataArrayWriter() override;

    void Write(const DataArray& array) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// =========================================================================
// TouchstoneDataArrayReader -- read a DataArray from a Touchstone (.sNp)
// =========================================================================

class TouchstoneDataArrayReader : public IDataArrayReader
{
public:
    explicit TouchstoneDataArrayReader(const std::string& file_path);
    ~TouchstoneDataArrayReader() override;

    DataArray Read() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// =========================================================================
// TouchstoneReader -- read a Dataset from a Touchstone (.sNp) file
// =========================================================================
//
// Convenience: wraps the DataArray reader and packages the result into a
// Dataset with a "SP" block.
// =========================================================================

class TouchstoneReader : public IDatasetReader
{
public:
    explicit TouchstoneReader(const std::string& file_path);
    ~TouchstoneReader() override;

    Dataset Read() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xdataset

#endif // XDATASET_TOUCHSTONE_IO_INTERNAL_H
