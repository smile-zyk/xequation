#include "touchstone_io.h"

#include "block.h"
#include "data_array.h"
#include "data_series.h"
#include "dataset.h"
#include "dimension_spec.h"
#include "unit.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace xdataset
{

namespace
{

// =========================================================================
// Touchstone option-line parsing
// =========================================================================

struct TouchstoneOptions
{
    std::string freq_unit;   // "GHz", "MHz", "KHz", "Hz"
    char        param_type;  // 'S', 'Y', 'Z', 'G', 'H'
    char        format;      // 'M' (magnitude-angle), 'D' (dB-angle), 'R' (real-imag)
    double      resistance;  // reference impedance in ohms
    int         num_ports;   // inferred from data columns
};

/// Parse the option line: # GHz S MA R 50
TouchstoneOptions parse_option_line(const std::string& line)
{
    if (line.empty() || line[0] != '#')
        throw std::runtime_error("Touchstone: missing option line (must start with #)");

    TouchstoneOptions opts;
    std::istringstream ss(line.substr(1)); // skip '#'

    ss >> opts.freq_unit >> opts.param_type;

    std::string fmt_str;
    ss >> fmt_str;
    if (fmt_str.size() < 1)
        throw std::runtime_error("Touchstone: invalid format in option line");
    opts.format = fmt_str[0];

    // Parse R <value>
    std::string r_token;
    ss >> r_token;
    if (r_token != "R")
        throw std::runtime_error("Touchstone: expected 'R' token in option line, got: " + r_token);
    ss >> opts.resistance;

    opts.num_ports = 0; // will be inferred later
    return opts;
}

// =========================================================================
// Complex number conversion helpers
// =========================================================================

/// Convert magnitude (linear) + angle (degrees) to complex
std::complex<double> ma_to_complex(double mag, double ang_deg)
{
    double ang_rad = ang_deg * M_PI / 180.0;
    return std::complex<double>(mag * std::cos(ang_rad),
                                mag * std::sin(ang_rad));
}

/// Convert dB + angle (degrees) to complex
std::complex<double> db_to_complex(double db, double ang_deg)
{
    double mag = std::pow(10.0, db / 20.0);
    return ma_to_complex(mag, ang_deg);
}

/// Convert complex to magnitude (linear) + angle (degrees)
void complex_to_ma(std::complex<double> c, double& mag, double& ang_deg)
{
    mag    = std::abs(c);
    ang_deg = std::arg(c) * 180.0 / M_PI;
}

/// Convert complex to dB + angle (degrees)
void complex_to_db(std::complex<double> c, double& db, double& ang_deg)
{
    double mag = std::abs(c);
    db    = (mag > 0.0) ? 20.0 * std::log10(mag) : -999.0;
    ang_deg = std::arg(c) * 180.0 / M_PI;
}

// =========================================================================
// Data-parsing helpers
// =========================================================================

/// Strip leading/trailing whitespace
std::string trim(const std::string& s)
{
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/// Read all non-comment, non-empty lines from a file
std::vector<std::string> read_data_lines(const std::string& file_path,
                                          std::string& option_line)
{
    std::ifstream in(file_path.c_str());
    if (!in.is_open())
        throw std::runtime_error("Touchstone: cannot open file: " + file_path);

    std::vector<std::string> lines;
    std::string line;
    bool found_option = false;

    while (std::getline(in, line))
    {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;
        if (trimmed[0] == '!') continue; // comment

        if (!found_option && trimmed[0] == '#')
        {
            option_line = trimmed;
            found_option = true;
            continue;
        }

        lines.push_back(trimmed);
    }

    if (!found_option)
        throw std::runtime_error("Touchstone: no option line found in file");

    return lines;
}

/// Parse all numeric values from a data line into a vector of doubles
std::vector<double> parse_data_line(const std::string& line)
{
    std::vector<double> values;
    std::istringstream ss(line);
    double v;
    while (ss >> v)
        values.push_back(v);
    return values;
}

/// Map Touchstone frequency-unit string to Unit
Unit freq_unit_from(const std::string& s)
{
    if (s == "GHz") return Unit::parse("GHz");
    if (s == "MHz") return Unit::parse("MHz");
    if (s == "KHz") return Unit::parse("KHz");
    if (s == "Hz")  return Unit::parse("Hz");
    return Unit(); // unrecognised -- dimensionless
}

/// Infer number of ports from data columns.
/// Each data line has: freq + N*N*2 values (2 per S-param when format is RI, MA, or DB)
int infer_num_ports(int num_values_per_line)
{
    if (num_values_per_line < 3)
        throw std::runtime_error("Touchstone: too few columns in data");
    // num_values = 1 (freq) + N*N * 2
    int remaining = num_values_per_line - 1; // subtract freq
    if (remaining % 2 != 0)
        throw std::runtime_error("Touchstone: odd number of S-parameter columns");
    int num_complex = remaining / 2;
    // num_complex = N * N
    int n = static_cast<int>(std::sqrt(static_cast<double>(num_complex)));
    if (n * n != num_complex)
        throw std::runtime_error("Touchstone: cannot infer port count from " +
                                 std::to_string(num_complex) + " complex values");
    return n;
}

} // anonymous namespace

// =========================================================================
// TouchstoneDataArrayReader::Impl
// =========================================================================

class TouchstoneDataArrayReader::Impl
{
public:
    explicit Impl(const std::string& path)
        : file_path_(path)
    {}

    DataArray read()
    {
        std::string option_line;
        std::vector<std::string> data_lines = read_data_lines(file_path_, option_line);
        if (data_lines.empty())
            throw std::runtime_error("Touchstone: no data lines in file");

        TouchstoneOptions opts = parse_option_line(option_line);

        std::vector<std::vector<double>> all_rows;
        for (const auto& dl : data_lines)
        {
            auto vals = parse_data_line(dl);
            if (!vals.empty())
                all_rows.push_back(std::move(vals));
        }

        if (all_rows.empty())
            throw std::runtime_error("Touchstone: no numeric data found");

        int cols_per_row = static_cast<int>(all_rows[0].size());
        opts.num_ports = infer_num_ports(cols_per_row);
        int N = opts.num_ports;

        std::size_t num_rows = all_rows.size();

        // Extract frequency column
        std::vector<double> freq_values(num_rows);
        for (std::size_t i = 0; i < num_rows; ++i)
        {
            if (static_cast<int>(all_rows[i].size()) != cols_per_row)
                throw std::runtime_error("Touchstone: inconsistent column count on row " +
                                         std::to_string(i));
            freq_values[i] = all_rows[i][0];
        }

        // Build S-matrix flat data (row-major per frequency point).
        // Touchstone column order: S[1,1], S[2,1], ..., S[N,1], S[1,2], ..., S[N,N]
        std::size_t mat_elems = static_cast<std::size_t>(N * N);
        std::size_t total = num_rows * mat_elems;
        std::vector<std::complex<double>> s_flat(total);

        for (std::size_t i = 0; i < num_rows; ++i)
        {
            std::complex<double>* row_base = s_flat.data() + i * mat_elems;
            for (int col = 1; col <= N; ++col)
            {
                for (int row = 1; row <= N; ++row)
                {
                    int base_idx = 1 + ((col - 1) * N + (row - 1)) * 2;
                    double v1 = all_rows[i][base_idx];
                    double v2 = all_rows[i][base_idx + 1];

                    std::complex<double> val;
                    switch (opts.format)
                    {
                    case 'R': val = std::complex<double>(v1, v2); break;
                    case 'M': val = ma_to_complex(v1, v2);       break;
                    case 'D': val = db_to_complex(v1, v2);       break;
                    default:
                        throw std::runtime_error(
                            std::string("Touchstone: unknown format flag: ") + opts.format);
                    }
                    row_base[(row - 1) * N + (col - 1)] = val;
                }
            }
        }

        // Build DataArray: freq independent + S-matrix dependent
        DataArrayCreateInfo info;
        DataSeries freq_series = DataSeries::CreateScalarFromMemory<double>(
            freq_values.data(), freq_values.size());
        freq_series.set_unit(freq_unit_from(opts.freq_unit));
        info.datas["freq"] = std::move(freq_series);
        info.datas[DataArray::kSelf] = DataSeries::CreateMatrixFromMemory<std::complex<double>>(
            N, N, s_flat.data(), total);
        info.multi_dimension_spec.add_regular(num_rows);
        info.kind = DataArrayKind::kDependent;

        return DataArray(info);
    }

private:
    std::string file_path_;
};

TouchstoneDataArrayReader::TouchstoneDataArrayReader(const std::string& file_path)
    : impl_(new Impl(file_path))
{}

TouchstoneDataArrayReader::~TouchstoneDataArrayReader() = default;

DataArray TouchstoneDataArrayReader::Read()
{
    return impl_->read();
}

// =========================================================================
// TouchstoneDataArrayWriter::Impl
// =========================================================================

class TouchstoneDataArrayWriter::Impl
{
public:
    explicit Impl(const std::string& path)
        : file_path_(path)
    {}

    void write(const DataArray& array)
    {
        // Expect: 1 independent (freq), 1 dependent (S = NxN complex matrix)
        const auto& datas_map = array.datas();

        // Find freq key (any key != kSelf)
        std::string freq_key;
        for (const auto& kv : datas_map)
        {
            if (kv.first != DataArray::kSelf)
            {
                freq_key = kv.first;
                break;
            }
        }
        if (freq_key.empty())
            throw std::runtime_error(
                "Touchstone: DataArray must have an independent variable");

        const DataSeries& freq_data = datas_map.find(freq_key)->second;
        if (freq_data.data_kind() != DataKind::kScalar ||
            freq_data.data_type() != DataType::kReal)
            throw std::runtime_error(
                "Touchstone: independent variable must be real scalars");

        // Get S-matrix
        auto self_it = datas_map.find(DataArray::kSelf);
        if (self_it == datas_map.end())
            throw std::runtime_error(
                "Touchstone: DataArray has no self data");

        const DataSeries& s_data = self_it->second;
        if (s_data.data_kind() != DataKind::kMatrix ||
            s_data.data_type() != DataType::kComplex)
            throw std::runtime_error(
                "Touchstone: self data must be a complex matrix");

        const DataShape& shape = s_data.data_shape();
        if (shape.size() != 2 || shape[0] != shape[1])
            throw std::runtime_error(
                "Touchstone: S matrix must be square (NxN), got " +
                std::to_string(shape.empty() ? 0 : shape[0]) + "x" +
                std::to_string(shape.size() < 2 ? 0 : shape[1]));

        int N = shape[0];
        std::size_t num_rows = freq_data.size();

        std::ofstream out(file_path_.c_str());
        if (!out.is_open())
            throw std::runtime_error("Touchstone: cannot create file: " + file_path_);

        out << "# GHz S MA R 50\n";
        out << "! Touchstone file generated by xdataset\n";
        out << "! freq";
        for (int col = 1; col <= N; ++col)
            for (int row = 1; row <= N; ++row)
                out << "   S" << row << col;
        out << "\n";

        for (std::size_t i = 0; i < num_rows; ++i)
        {
            out << freq_data.scalar_at<double>(i);

            const auto& mat = s_data.matrix_at<std::complex<double>>(static_cast<Index>(i));
            for (int col = 0; col < N; ++col)
            {
                for (int row = 0; row < N; ++row)
                {
                    double mag, ang;
                    complex_to_ma(mat(row, col), mag, ang);
                    out << "   " << mag << "   " << ang;
                }
            }
            out << "\n";
        }

        out.close();
    }

private:
    std::string file_path_;
};

TouchstoneDataArrayWriter::TouchstoneDataArrayWriter(const std::string& file_path)
    : impl_(new Impl(file_path))
{}

TouchstoneDataArrayWriter::~TouchstoneDataArrayWriter() = default;

void TouchstoneDataArrayWriter::Write(const DataArray& array)
{
    impl_->write(array);
}

// =========================================================================
// TouchstoneReader (Dataset convenience) -- delegates to DataArray reader
// =========================================================================

class TouchstoneReader::Impl
{
public:
    explicit Impl(const std::string& path)
        : file_path_(path)
    {}

    Dataset read()
    {
        TouchstoneDataArrayReader da_reader(file_path_);
        DataArray da = da_reader.Read();

        // Build Dataset name from file name
        std::string ds_name = file_path_;
        {
            auto pos = ds_name.find_last_of("/\\");
            if (pos != std::string::npos)
                ds_name = ds_name.substr(pos + 1);
            pos = ds_name.rfind('.');
            if (pos != std::string::npos)
                ds_name = ds_name.substr(0, pos);
        }

        Dataset ds(ds_name);

        BlockCreateInfo info;
        const auto& datas_map = da.datas();

        // Extract independent (freq)
        for (const auto& kv : datas_map)
        {
            if (kv.first != DataArray::kSelf)
            {
                IndependentSpec is = {kv.first, kv.second,
                    DimensionSpec::Regular(kv.second.size())};
                info.independent_specs.push_back(std::move(is));
            }
        }

        // Extract dependent (S-matrix)
        auto self_it = datas_map.find(DataArray::kSelf);
        if (self_it != datas_map.end())
        {
            DependentSpec dep = {"S", self_it->second};
            info.dependent_specs.push_back(std::move(dep));
        }

        ds.AddBlock("SP", std::move(info));
        return ds;
    }

private:
    std::string file_path_;
};

TouchstoneReader::TouchstoneReader(const std::string& file_path)
    : impl_(new Impl(file_path))
{}

TouchstoneReader::~TouchstoneReader() = default;

Dataset TouchstoneReader::Read()
{
    return impl_->read();
}

} // namespace xdataset
