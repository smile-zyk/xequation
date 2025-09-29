#pragma once
#include <cstddef>
#include <pybind11/pytypes.h>
#include <string>
#include <unordered_map>

#include "py_common.h"
#include "core/expr_common.h"

namespace xexprengine
{
class PySymbolExtractor
{
  public:
    PySymbolExtractor() = default;
    ~PySymbolExtractor() = default;

    PySymbolExtractor(const PySymbolExtractor &) = delete;
    PySymbolExtractor(PySymbolExtractor &&) = delete;
    PySymbolExtractor &operator=(const PySymbolExtractor &) = delete;
    PySymbolExtractor &operator=(PySymbolExtractor &&) = delete;

    ParseResult Extract(const std::string& py_code);

    void ClearCache();

    void SetMaxCacheSize();

    size_t GetCacheSize();

  private:
    ParseResult Parse(const std::string& py_code);

    void VisitName(py::handle node, ParseResult& result);
    void VisitCall(py::handle node, ParseResult& result);
    void GenericVisit(py::handle node, ParseResult& result);
    void VisitNode(py::handle node, ParseResult& result);

    void CleanupCache();

  private:
    std::unordered_map<std::string, ParseResult> parse_result_cache_;
    size_t max_cache_size = 1000;
};
} // namespace xexprengine