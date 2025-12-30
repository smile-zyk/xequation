#pragma once
#include "core/value.h"
#include "python_base.h"
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>


#include "core/value.h"

namespace xequation
{
namespace value_convert
{
inline std::vector<PyGILState_STATE> &gil_state_stack()
{
    static thread_local std::vector<PyGILState_STATE> stack;
    return stack;
}

inline void acquire_gil()
{
    if (Py_IsInitialized())
    {
        gil_state_stack().push_back(PyGILState_Ensure());
    }
}

inline void release_gil()
{
    auto &stack = gil_state_stack();
    if (!stack.empty() && Py_IsInitialized())
    {
        PyGILState_STATE st = stack.back();
        stack.pop_back();
        PyGILState_Release(st);
    }
}

template <typename T>
inline void RegisterCallbacksForType()
{
    Value::RegisterBeforeOperation<T>([](const std::type_info &) { acquire_gil(); });
    Value::RegisterAfterOperation<T>([](const std::type_info &) { release_gil(); });
}

inline void RegisterPybindValueCallbacksOnce()
{
    static std::atomic<bool> callbacks_registered{false};

    if (callbacks_registered.load(std::memory_order_acquire))
        return;

    if (!Py_IsInitialized())
        return;

    // Acquire GIL before construct; release after; acquire before destruct; release after.
    using V = xequation::Value;

    // Cover common pybind11 Python types
    RegisterCallbacksForType<pybind11::handle>();
    RegisterCallbacksForType<pybind11::object>();
    RegisterCallbacksForType<pybind11::int_>();
    RegisterCallbacksForType<pybind11::float_>();
    RegisterCallbacksForType<pybind11::bool_>();
    RegisterCallbacksForType<pybind11::str>();
    RegisterCallbacksForType<pybind11::list>();
    RegisterCallbacksForType<pybind11::tuple>();
    RegisterCallbacksForType<pybind11::dict>();
    RegisterCallbacksForType<pybind11::set>();
    RegisterCallbacksForType<pybind11::none>();

    callbacks_registered.store(true, std::memory_order_release);
}

class PyObjectConverter
{
  public:
    class TypeConverter
    {
      public:
        virtual ~TypeConverter() {}
        virtual bool CanConvert(const Value &value) const = 0;
        virtual pybind11::object Convert(const Value &value) const = 0;
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

        pybind11::object Convert(const Value &value) const override
        {
            return pybind11::cast(value.Cast<T>());
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

        pybind11::object Convert(const Value &value) const override
        {
            return pybind11::none();
        }
    };

    class ObjectConverter : public TypeConverter
    {
      public:
        ~ObjectConverter() {}
        bool CanConvert(const Value &value) const override
        {
            const std::type_info &type = value.Type();

            if (type == typeid(pybind11::object))
                return true;

            if (type == typeid(pybind11::handle))
                return true;

            if (type == typeid(pybind11::int_) || type == typeid(pybind11::float_) || type == typeid(pybind11::str) ||
                type == typeid(pybind11::list) || type == typeid(pybind11::dict) || type == typeid(pybind11::tuple) ||
                type == typeid(pybind11::bool_))
                return true;

            return false;
        }

        pybind11::object Convert(const Value &value) const override
        {
            const std::type_info &type = value.Type();

            if (type == typeid(pybind11::object))
            {
                return value.Cast<pybind11::object>();
            }

            if (type == typeid(pybind11::handle))
            {
                pybind11::handle h = value.Cast<pybind11::handle>();
                return pybind11::reinterpret_borrow<pybind11::object>(h);
            }

            if (type == typeid(pybind11::int_))
            {
                pybind11::int_ i = value.Cast<pybind11::int_>();
                return pybind11::object(i);
            }

            if (type == typeid(pybind11::float_))
            {
                pybind11::float_ f = value.Cast<pybind11::float_>();
                return pybind11::object(f);
            }

            if (type == typeid(pybind11::str))
            {
                pybind11::str s = value.Cast<pybind11::str>();
                return pybind11::object(s);
            }

            if (type == typeid(pybind11::list))
            {
                pybind11::list l = value.Cast<pybind11::list>();
                return pybind11::object(l);
            }

            if (type == typeid(pybind11::dict))
            {
                pybind11::dict d = value.Cast<pybind11::dict>();
                return pybind11::object(d);
            }

            if (type == typeid(pybind11::tuple))
            {
                pybind11::tuple t = value.Cast<pybind11::tuple>();
                return pybind11::object(t);
            }

            if (type == typeid(pybind11::bool_))
            {
                pybind11::bool_ b = value.Cast<pybind11::bool_>();
                return pybind11::object(b);
            }

            return pybind11::none();
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
            pybind11::object obj = pybind11::reinterpret_borrow<pybind11::object>(src);

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
                    pybind11::object obj = converter->Convert(src);
                    return obj.release();
                }
            }

            return pybind11::none().release();
        }
        catch (const std::exception &e)
        {
            pybind11::pybind11_fail("Failed to convert Value to Python object: " + std::string(e.what()));
            return nullptr;
        }
    }
};

} // namespace detail
} // namespace PYBIND11_NAMESPACE