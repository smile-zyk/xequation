#include "data_array_io.h"
#include "data_array.h"
#include "touchstone_io.h"

#include <stdexcept>

namespace xdataset
{

/* static */
std::unique_ptr<IDataArrayWriter> DataArrayIO::CreateWriter(
    const std::string& format,
    const std::string& path)
{
    if (format == "touchstone" || format == "snp")
        return std::unique_ptr<IDataArrayWriter>(new TouchstoneDataArrayWriter(path));
    throw std::invalid_argument("DataArrayIO: unsupported format: " + format);
}

/* static */
std::unique_ptr<IDataArrayReader> DataArrayIO::CreateReader(
    const std::string& format,
    const std::string& path)
{
    if (format == "touchstone" || format == "snp")
        return std::unique_ptr<IDataArrayReader>(new TouchstoneDataArrayReader(path));
    throw std::invalid_argument("DataArrayIO: unsupported format: " + format);
}

/* static */
void DataArrayIO::Save(const DataArray& array,
                       const std::string& format,
                       const std::string& path)
{
    auto writer = CreateWriter(format, path);
    writer->Write(array);
}

/* static */
DataArray DataArrayIO::Load(const std::string& format,
                            const std::string& path)
{
    auto reader = CreateReader(format, path);
    return reader->Read();
}

} // namespace xdataset
