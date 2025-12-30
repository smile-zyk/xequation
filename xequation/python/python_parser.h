#pragma once

#include <string>
#include <boost/compute/detail/lru_cache.hpp>

#include "python_common.h"
#include "core/equation_common.h"

namespace xequation
{
namespace python
{

class PythonParser
{
  public:
    PythonParser();
    ~PythonParser();

    PythonParser(const PythonParser &) = delete;
    PythonParser &operator=(const PythonParser &) = delete;

    ParseResult ParseStatements(const std::string &code);
    ParseResult ParseExpression(const std::string &code);
    std::vector<std::string> SplitStatements(const std::string &code);
    ParseResult ParseSingleStatement(const std::string &code);
    size_t GetParseResultCacheSize() const { return parse_result_cache_.size(); }

  private:
    void EvictLRU();

  private:
    pybind11::object parser_;
    static constexpr size_t max_cache_size_ = 50;
    boost::compute::detail::lru_cache<std::string, ParseResult> parse_result_cache_{max_cache_size_};
};
} // namespace python
} // namespace xequation