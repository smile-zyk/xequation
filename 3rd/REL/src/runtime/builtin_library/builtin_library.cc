// Builtin library: runtime introspection and DataArray utilities.
//
// datasets() / default_dataset() / variables() inspect the global Environment
// registries (datasets, variables via active env) and return the information
// as an Independent DataArray holding one String Scalar row per entry.
// what()/indep()/output() work on Values.

#include "builtin_library.h"
#include "environment.h"
#include "value.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <limits.h>
    #include <unistd.h>
#endif

namespace rel {
namespace builtin {
namespace {

std::string absolute_path(const std::string& rel)
{
#ifdef _WIN32
    char* full = _fullpath(nullptr, rel.c_str(), 0);
    if (!full)
        return rel;
    std::string result(full);
    std::free(full);
    return result;
#else
    char buf[PATH_MAX];
    if (!getcwd(buf, sizeof(buf)))
        return rel;
    return std::string(buf) + "/" + rel;
#endif
}

}  // namespace

Value Datasets()
{
    const std::vector<std::string> names = Environment::DatasetNames();
    std::vector<std::string> rows;
    rows.reserve(names.size());
    for (std::size_t i = 0; i < names.size(); ++i)
        rows.push_back(names[i]);
    return Value::ArrayString(rows);
}

Value DefaultDataset()
{
    std::vector<std::string> rows;
    xdataset::Dataset* ds = Environment::DefaultDataset();
    if (ds)
        rows.push_back(ds->name());
    else
        rows.push_back("NO DEFAULT DATASET");
    return Value::ArrayString(rows);
}

Value Variables()
{
    return Value::ArrayString({});
}

Value What(const Value& v)
{
    std::vector<std::string> rows;
    rows.reserve(5);

    std::ostringstream dep;
    dep << '[';
    const std::vector<std::string>& names = v.indep_names();
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        if (i > 0) dep << ", ";
        dep << names[i];
    }
    dep << ']';

    rows.push_back("Dependency: " + dep.str());
    rows.push_back("Kind: " + std::string(
        v.is_dependent() ? "Dependent" : "Independent"));
    rows.push_back("Dimension: " + v.dimension_spec().to_string());
    rows.push_back("Data Shape: " + v.data_shape().to_string());
    rows.push_back("Data Type: " + std::string(xdataset::DataTypeToString(v.data_type())));
    if (v.unit().has_dimension())
        rows.push_back("Unit: " + v.unit().to_string());

    return Value::ArrayString(rows);
}

Value Indep(const Value& da_val, const Value& sel_val)
{
    if (!da_val.is_data_array())
        throw std::runtime_error("indep: first argument must be a DataArray");

    const xdataset::DataArray& da = da_val.as_data_array();

    if (sel_val.is_measurement() &&
        sel_val.as_measurement().data_type() == xdataset::DataType::kInteger)
    {
        int index = sel_val.as_measurement().as_scalar<int>();
        return Value(da.indep(index));
    }

    if (sel_val.is_measurement() &&
        sel_val.as_measurement().data_type() == xdataset::DataType::kString)
    {
        const std::string& name = sel_val.as_measurement().as_scalar<std::string>();
        return Value(da.indep(name));
    }

    throw std::runtime_error(
        "indep: second argument must be an Integer (index) or String (name)");
}

Value Output(const Value& v, const Value& var_name_val)
{
    if (!v.is_data_array())
        throw std::runtime_error("output: first argument must be a DataArray");

    const xdataset::Measurement& name_m = var_name_val.as_measurement();
    if (name_m.data_type() != xdataset::DataType::kString)
        throw std::runtime_error(
            "output: second argument must be a String variable name");
    const std::string& var_name = name_m.as_scalar<std::string>();

    const xdataset::DataArray& da = v.as_data_array();
    const std::string file_path = absolute_path(var_name + ".csv");
    da.GetOrCreateDataFrame(var_name).WriteToCsv(file_path);

    return Value::String(file_path);
}

FunctionLibrary MakeLibrary()
{
    FunctionLibrary lib("builtin");

    lib.Add(Function("datasets", std::vector<FunctionParam>(),
        [](const Function::ArgMap&) { return Datasets(); }));
    lib.Add(Function("default_dataset", std::vector<FunctionParam>(),
        [](const Function::ArgMap&) { return DefaultDataset(); }));
    lib.Add(Function("variables", std::vector<FunctionParam>(),
        [](const Function::ArgMap&) { return Variables(); }));

    lib.Add(Function("what", std::vector<FunctionParam>{ Param("x") },
        [](const Function::ArgMap& args) { return What(args.at("x")); }));

    lib.Add(Function("indep",
        std::vector<FunctionParam>{
            Param("da"),
            Param("selector", Value::Integer(1)),
        },
        [](const Function::ArgMap& args) {
            return Indep(args.at("da"), args.at("selector"));
        }));

    lib.Add(Function("output",
        std::vector<FunctionParam>{
            Param("da"),
            Param("variable_name", Value::String("data")),
        },
        [](const Function::ArgMap& args) {
            return Output(args.at("da"), args.at("variable_name"));
        }));

    return lib;
}

}  // namespace builtin
}  // namespace rel
