#pragma once

#include <string>

#include "core/equation_common.h"
#include "python_common.h"


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

    std::vector<std::string> SplitStatements(const std::string &code);
    ParseResult ParseSingleStatement(const std::string &code);
    ParseResult ParseMultipleStatements(const std::string &code);
    void ClearCache();
    void SetMaxCacheSize(size_t max_size);
    size_t GetCacheSize();

  private:
    void EvictLRU();

  private:
    pybind11::object parser_;
    struct CacheEntry
    {
        std::string key;
        ParseResult value;
        CacheEntry(const std::string& k, const ParseResult& v) : key(k), value(v) {}
    };

    std::list<CacheEntry> cache_list_;
    std::unordered_map<std::string, typename std::list<CacheEntry>::iterator> cache_map_;
    size_t max_cache_size_ = 1000;
};
} // namespace python
} // namespace xequation