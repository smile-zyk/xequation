#ifndef XDATASET_HDF5_IO_H
#define XDATASET_HDF5_IO_H

// =========================================================================
// HDF5 IO class declarations (private to the library build).
//
// These concrete format implementations are NOT part of the public API.
// They are instantiated only by the format factories (DatasetIO), whose
// definitions live in src/hdf5_io.cc.
// =========================================================================

#include "dataset_io.h"

#include <memory>
#include <string>

namespace xdataset
{

// =========================================================================
// Hdf5Writer -- write Dataset to an HDF5 file
// =========================================================================

class Hdf5Writer : public IDatasetWriter
{
public:
    explicit Hdf5Writer(const std::string& file_path);
    ~Hdf5Writer() override;

    void Write(const Dataset& dataset) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// =========================================================================
// Hdf5Reader -- read Dataset from an HDF5 file
// =========================================================================

class Hdf5Reader : public IDatasetReader
{
public:
    explicit Hdf5Reader(const std::string& file_path);
    ~Hdf5Reader() override;

    Dataset Read() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xdataset

#endif // XDATASET_HDF5_IO_INTERNAL_H
