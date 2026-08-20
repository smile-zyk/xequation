#pragma once
#include <string>
#include <vector>

#include <boost/compute/detail/lru_cache.hpp>

#include "core/equation_common.h"

namespace xequation
{
namespace rel_engine
{

// REL 语句解析器：仿照 python/PythonParser 的接口。
// REL 没有 Python AST 可用，这里用正则识别"赋值 vs 表达式"，
// 并用 rel::Environment 的全局注册表（函数/常量）过滤依赖标识符。
class RelParser
{
  public:
    RelParser() = default;
    ~RelParser() = default;

    RelParser(const RelParser &) = delete;
    RelParser &operator=(const RelParser &) = delete;

    // 按 ';' 或换行拆分语句
    std::vector<std::string> SplitStatements(const std::string &code);

    // 解析多条语句
    ParseResult ParseStatements(const std::string &code);

    // 解析单条语句（赋值 -> kVariable；否则 kExpression）
    ParseResult ParseSingleStatement(const std::string &code);

    // 解析单个表达式，提取依赖
    ParseResult ParseExpression(const std::string &code);

    size_t GetParseResultCacheSize() const { return parse_result_cache_.size(); }

  private:
    // 提取表达式中的标识符依赖（排除函数名/内置常量/自身）
    std::vector<std::string> ExtractDependencies(const std::string &expression,
                                                 const std::string &self_name = "") const;

    void EvictLRU();

  private:
    static constexpr size_t max_cache_size_ = 50;
    boost::compute::detail::lru_cache<std::string, ParseResult> parse_result_cache_{max_cache_size_};
};

} // namespace rel_engine
} // namespace xequation
