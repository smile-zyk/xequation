#pragma once
#include "core/value.h"
#include "py_base.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/value.h"

namespace xequation
{
namespace value_convert
{
class PyObjectConverter
{
  public:
    class TypeConverter
    {
      public:
        virtual ~TypeConverter() {}
        virtual bool CanConvert(const Value &value) const = 0;
        virtual py::object Convert(const Value &value) const = 0;
    };

    template <typename T>
    class NativeTypeConverter : public TypeConverter
    {
      public:
        ~NativeTypeConverter() {}
        bool CanConvert(const Value &value) const override
        {
            return value.Type() == typeid(T);
        }

        py::object Convert(const Value &value) const override
        {
            return py::cast(value.Cast<T>());
        }
    };

    class NoneConverter : public TypeConverter
    {
      public:
        ~NoneConverter() {}
        bool CanConvert(const Value &value) const override
        {
            return value.Type() == typeid(void);
        }

        py::object Convert(const Value &value) const override
        {
            return py::none();
        }
    };

    class ObjectConverter : public TypeConverter
    {
      public:
        ~ObjectConverter() {}
        bool CanConvert(const Value &value) const override
        {
            return value.Type() == typeid(py::object);
        }

        py::object Convert(const Value &value) const override
        {
            return value.Cast<py::object>();
        }
    };

    static void RegisterConverter(std::unique_ptr<TypeConverter> converter)
    {
        GetConverters().push_back(std::move(converter));
    }

    template <typename T>
    static void RegisterNativeConverter(std::vector<std::unique_ptr<TypeConverter>> &converters)
    {
        converters.push_back(std::unique_ptr<TypeConverter>(new NativeTypeConverter<T>()));
    }

    static std::vector<std::unique_ptr<TypeConverter>> &GetConverters()
    {
        static std::vector<std::unique_ptr<TypeConverter>> converters;
        static bool initialized = false;

        if (!initialized)
        {
            initialized = true;
            InitNativeConverter(converters);
        }
        return converters;
    }

  private:
    static void InitNativeConverter(std::vector<std::unique_ptr<TypeConverter>> &converters)
    {
        converters.push_back(std::unique_ptr<TypeConverter>(new NoneConverter()));
        converters.push_back(std::unique_ptr<TypeConverter>(new ObjectConverter()));
        
        RegisterNativeConverter<int>(converters);
        RegisterNativeConverter<double>(converters);
        RegisterNativeConverter<float>(converters);
        RegisterNativeConverter<std::string>(converters);
        RegisterNativeConverter<bool>(converters);
        RegisterNativeConverter<long>(converters);
        RegisterNativeConverter<long long>(converters);
        RegisterNativeConverter<unsigned int>(converters);
        RegisterNativeConverter<unsigned long>(converters);
        RegisterNativeConverter<unsigned long long>(converters);

        RegisterNativeConverter<std::vector<int>>(converters);
        RegisterNativeConverter<std::vector<float>>(converters);
        RegisterNativeConverter<std::vector<double>>(converters);
        RegisterNativeConverter<std::vector<std::string>>(converters);
        RegisterNativeConverter<std::map<std::string, std::string>>(converters);
        RegisterNativeConverter<std::unordered_map<std::string, std::string>>(converters);
    }
};
} // namespace value_convert
} // namespace xequation

std::ostream &operator<<(std::ostream &os, const py::object &obj);

namespace PYBIND11_NAMESPACE
{
namespace detail
{

template <>
struct type_caster<xequation::Value>
{
  public:
    PYBIND11_TYPE_CASTER(xequation::Value, _("Value"));

    bool load(handle src, bool convert)
    {
        if (!src || src.is_none())
        {
            return false;
        }
        try
        {
            pybind11::object obj;

            if (pybind11::isinstance<pybind11::object>(src))
            {
                obj = pybind11::cast<pybind11::object>(src);
            }
            else
            {
                obj = pybind11::reinterpret_borrow<pybind11::object>(src);
            }

            value = obj;
            return true;
        }
        catch (const std::exception &e)
        {
            return false;
        }
    }

    static handle cast(const xequation::Value &src, return_value_policy /* policy */, handle /* parent */)
    {
        try
        {
            for (const auto &converter : xequation::value_convert::PyObjectConverter::GetConverters())
            {
                if (converter->CanConvert(src))
                {
                    py::object obj = converter->Convert(src);
                    return obj.release();
                }
            }

            return py::none().release();
        }
        catch (const std::exception &e)
        {
            py::pybind11_fail("Failed to convert Value to Python object: " + std::string(e.what()));
            return nullptr;
        }
    }
};

} // namespace detail
} // namespace PYBIND11_NAMESPACE