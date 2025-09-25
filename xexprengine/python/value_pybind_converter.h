#pragma once
#include "value.h"
#include <pybind11/cast.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <vector>

namespace xexprengine
{
namespace value_convert
{
class PyObjectConverter
{
  public:
    virtual ~PyObjectConverter();
    virtual bool CanConvert(const Value &value) const = 0;
    virtual pybind11::object Convert(const Value &value) const = 0;
};

template <typename T>
class NativePyObjectConverter : public PyObjectConverter
{
  public:
    ~NativePyObjectConverter();
    bool CanConvert(const Value &value) const override
    {
        return value.Type() == typeid(T);
    }

    pybind11::object Convert(const Value &value) const override
    {
        return pybind11::cast(value.Cast<T>());
    }
};

} // namespace value_convert
} // namespace xexprengine

std::ostream &operator<<(std::ostream &os, const pybind11::object &obj);

namespace PYBIND11_NAMESPACE
{
namespace detail
{

template <>
struct type_caster<xexprengine::Value>
{
  public:
    PYBIND11_TYPE_CASTER(xexprengine::Value, _("Value"));

    bool load(handle src, bool convert)
    {
        if (!src)
        {
            return false;
        }
        try
        {
            value = src;
            return true;
        }
        catch (const std::exception &e)
        {
            return false;
        }
    }

    static handle cast(const xexprengine::Value &src, return_value_policy /* policy */, handle /* parent */) 
    {
        try 
        {
            for (const auto& converter : GetConverters()) 
            {
                if (converter.CanConvert(src)) 
                {
                    pybind11::object obj = converter.Convert(src);
                    return obj.release();
                }
            }
            
            return pybind11::none().release();
        }
        catch (const std::exception& e)
        {
            pybind11::pybind11_fail("Failed to convert Value to Python object: " + std::string(e.what()));
            return nullptr;
        }
    }

  private:
    static std::vector<xexprengine::value_convert::PyObjectConverter> &GetConverters()
    {
        static std::vector<xexprengine::value_convert::PyObjectConverter> converters;
        static bool initialized = false;

        if (!initialized)
        {
            initialized = true;
            RegisterNativeConverters(converters);
        }

        return converters;
    }

    static void RegisterNativeConverters(std::vector<xexprengine::value_convert::PyObjectConverter> &converters)
    {
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<int>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<double>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<float>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<std::string>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<bool>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<long>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<long long>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<unsigned int>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<unsigned long>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<unsigned long long>());

        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<std::vector<int>>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<std::vector<double>>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<std::vector<std::string>>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<std::map<std::string, std::string>>());
        converters.push_back(xexprengine::value_convert::NativePyObjectConverter<std::unordered_map<std::string, std::string>>());
    }

  public:
    static void RegisterConverter(const xexprengine::value_convert::PyObjectConverter &converter)
    {
        GetConverters().push_back(converter);
    }
};

} // namespace detail
} // namespace PYBIND11_NAMESPACE

#define REGISTER_NATIVE_CONVERTER(Type)                                                                                \
    PYBIND11_NAMESPACE::detail::type_caster<xexprengine::Value>::RegisterConverter(                                    \
        std::make_unique<xexprengine::value_convert::NativeTypeConverter<Type>>()                                      \
    )

#define REGISTER_CUSTOM_CONVERTER(ConverterType)                                                                       \
    PYBIND11_NAMESPACE::detail::type_caster<xexprengine::Value>::RegisterConverter(std::make_unique<ConverterType>())