#pragma once

#include <QString>

#include "python_qt_wrapper.h"

class QtVariantProperty;
class QtVariantPropertyManager;

namespace xequation
{
namespace gui
{
namespace python
{

class PythonPropertyConverter
{
  public:
    virtual ~PythonPropertyConverter() = default;

    virtual bool CanConvert(pybind11::object obj) const = 0;

    virtual QtVariantProperty *
    CreateProperty(QtVariantPropertyManager *manager, const QString &name, pybind11::object obj);

    static QString GetTypeName(pybind11::object obj, bool qualified = false);
    static QString GetObjectStr(pybind11::object obj);

    PythonPropertyConverter(const PythonPropertyConverter &) = delete;
    PythonPropertyConverter &operator=(const PythonPropertyConverter &) = delete;
    PythonPropertyConverter(PythonPropertyConverter &&) = delete;
    PythonPropertyConverter &operator=(PythonPropertyConverter &&) = delete;

  protected:
    PythonPropertyConverter() = default;
};

class PythonPropertyConverterRegistry
{
  public:
    static PythonPropertyConverterRegistry &GetInstance();

    void RegisterConverter(std::unique_ptr<PythonPropertyConverter> converter, int priority = 0);

    void UnRegisterConverter(PythonPropertyConverter *converter);

    PythonPropertyConverter *FindConverter(pybind11::object obj);

    QtVariantProperty *CreateProperty(QtVariantPropertyManager *manager, const QString &name, pybind11::object obj);

    void Clear();

  private:
    PythonPropertyConverterRegistry() = default;
    ~PythonPropertyConverterRegistry() = default;

    PythonPropertyConverterRegistry(const PythonPropertyConverterRegistry &) = delete;
    PythonPropertyConverterRegistry &operator=(const PythonPropertyConverterRegistry &) = delete;
    PythonPropertyConverterRegistry(PythonPropertyConverterRegistry &&) = delete;
    PythonPropertyConverterRegistry &operator=(PythonPropertyConverterRegistry &&) = delete;

  private:
    struct ConverterEntry
    {
        std::unique_ptr<PythonPropertyConverter> converter;
        int priority;

        bool operator<(const ConverterEntry &other) const
        {
            return priority < other.priority;
        }
    };

    std::vector<ConverterEntry> converters_;
};

inline void RegisterPythonPropertyConverter(std::unique_ptr<PythonPropertyConverter> converter, int priority = 0)
{
    PythonPropertyConverterRegistry::GetInstance().RegisterConverter(std::move(converter), priority);
}

inline void UnRegisterPythonPropertyConverter(PythonPropertyConverter *converter)
{
    PythonPropertyConverterRegistry::GetInstance().UnRegisterConverter(converter);
}

inline QtVariantProperty *
CreatePythonProperty(QtVariantPropertyManager *manager, const QString &name, pybind11::object obj)
{
    return PythonPropertyConverterRegistry::GetInstance().CreateProperty(manager, name, obj);
}

inline PythonPropertyConverter *FindPythonPropertyConverter(pybind11::object obj)
{
    return PythonPropertyConverterRegistry::GetInstance().FindConverter(obj);
}

template <typename T>
class PythonPropertyConverterAutoRegister
{
  public:
    PythonPropertyConverterAutoRegister(int priority = 0)
    {
        RegisterPythonPropertyConverter(std::unique_ptr<T>(new T()), priority);
    }
};

#define REGISTER_PYTHON_PROPERTY_CONVERTER(ConverterClass, priority)                                                   \
    static xequation::gui::python::PythonPropertyConverterAutoRegister<ConverterClass>                                 \
    s_autoRegister_##ConverterClass(priority)

} // namespace python
} // namespace gui
} // namespace xequation