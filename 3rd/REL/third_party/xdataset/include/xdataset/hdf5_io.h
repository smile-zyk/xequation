#ifndef XDATASET_HDF5_IO_H
#define XDATASET_HDF5_IO_H

#include "dataset_io.h"

#include <string>

namespace xdataset
{

// =========================================================================
// Hdf5Writer — write Dataset to an HDF5 file
// =========================================================================

class XDATASET_API Hdf5Writer : public IDatasetWriter
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
// Hdf5Reader — read Dataset from an HDF5 file
// =========================================================================

class XDATASET_API Hdf5Reader : public IDatasetReader
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

#endif // XDATASET_HDF5_IO_H
