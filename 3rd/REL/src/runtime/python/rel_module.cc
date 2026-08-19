// =============================================================================
//  rel_module.cc — the embedded `rel` module entry point + PEP 562 lazy
//  attribute lookup (functions and constants) via FunctionProxy.
// =============================================================================

#include "python_common.h"

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <boost/variant.hpp>

#include <exception>
#include <stdexcept>
#include <string>

#include "environment.h"

namespace rel {
namespace python {
namespace {

/// Lightweight callable that resolves a registered function by name on every
/// call (no caching), so re-registration / unregistration take effect
/// immediately and no pybind11::function is ever captured (PYTHON.md §4.3).
struct FunctionProxy
{
    std::string name;
};

pybind11::object call_function_proxy(const FunctionProxy& p,
                               pybind11::args args, pybind11::kwargs kwargs)
{
    // Copy the Function under the registry lock; invoke outside the lock.
    Function fn;
    if (!Environment::CopyFunction(p.name, fn))
        throw pybind11::attribute_error("function '" + p.name + "' is no longer registered");

    const std::vector<FunctionParam>& params = fn.params();

    Function::ArgMap named;
    // Positional arguments bind in declaration order.
    for (std::size_t i = 0; i < args.size(); ++i)
    {
        if (i >= params.size())
            throw pybind11::type_error("too many positional arguments for '" + p.name + "'");
        named[params[i].name] = from_python(args[i]);
    }
    // Keyword arguments bind by name.
    for (auto item : kwargs)
    {
        std::string key = pybind11::cast<std::string>(item.first);
        named[key] = from_python(item.second);
    }

    return to_python(fn.Invoke(named));
}

void register_exception_translators()
{
    pybind11::register_exception_translator([](std::exception_ptr p) {
        if (!p)
            return;
        try
        {
            std::rethrow_exception(p);
        }
        catch (const std::out_of_range& e)      { PyErr_SetString(PyExc_IndexError, e.what()); }
        catch (const std::overflow_error& e)    { PyErr_SetString(PyExc_OverflowError, e.what()); }
        catch (const std::invalid_argument& e)  { PyErr_SetString(PyExc_ValueError, e.what()); }
        catch (const std::domain_error& e)      { PyErr_SetString(PyExc_ValueError, e.what()); }
        catch (const std::length_error& e)      { PyErr_SetString(PyExc_ValueError, e.what()); }
        catch (const boost::bad_get& e)         { PyErr_SetString(PyExc_TypeError, e.what()); }
        catch (const std::bad_alloc& e)         { PyErr_SetString(PyExc_MemoryError, e.what()); }
        catch (const std::exception& e)         { PyErr_SetString(PyExc_RuntimeError, e.what()); }
    });
}

}  // namespace
}  // namespace python
}  // namespace rel

PYBIND11_EMBEDDED_MODULE(rel, m)
{
    using namespace rel::python;

    m.doc() = "REL runtime — embedded Python plugin bridge";

    register_exception_translators();
    register_xdataset_bindings(m);
    register_rel_bindings(m);

    pybind11::class_<FunctionProxy>(m, "FunctionProxy")
        .def(pybind11::init<std::string>())
        .def("__call__", &call_function_proxy);

    // PEP 562 lazy lookup: resolve registered functions, then builtin constants.
    m.def("__getattr__", [](const std::string& name) -> pybind11::object {
        if (rel::Environment::HasFunction(name))
            return pybind11::cast(FunctionProxy{name});

        const rel::Value* c = rel::Environment::FindConstant(name);
        if (c)
            return pybind11::cast(*c);

        throw pybind11::attribute_error("module 'rel' has no attribute '" + name + "'");
    });
}
