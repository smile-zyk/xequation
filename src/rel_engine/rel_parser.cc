#include "rel_parser.h"

#include <regex>
#include <set>

#include "environment.h"
#include "rel.h"

namespace xequation
{
namespace rel_engine
{

namespace
{
// 赋值语句：name = expr
const std::regex kAssignRegex(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+?)\s*$)");
// 标识符
const std::regex kIdentifierRegex(R"(\b[A-Za-z_][A-Za-z0-9_]*\b)");
} // namespace

void RelParser::EvictLRU()
{
    // lru_cache 自管理容量，无需手动驱逐
}

std::vector<std::string> RelParser::SplitStatements(const std::string &code)
{
    std::vector<std::string> statements;

    std::string current;
    for (char c : code)
    {
        if (c == ';' || c == '\n')
        {
            if (!current.empty())
            {
                statements.push_back(current);
                current.clear();
            }
        }
        else
        {
            current.push_back(c);
        }
    }
    if (!current.empty())
    {
        statements.push_back(current);
    }
    return statements;
}

std::vector<std::string> RelParser::ExtractDependencies(const std::string &expression,
                                                        const std::string &self_name) const
{
    std::set<std::string> deps;
    std::smatch match;
    std::string::const_iterator begin = expression.cbegin();
    const std::string::const_iterator end = expression.cend();

    while (std::regex_search(begin, end, match, kIdentifierRegex))
    {
        const std::string name = match[0].str();
        if (name != self_name &&                  // 排除自身（赋值左侧）
            !rel::Environment::HasFunction(name) &&   // 排除注册函数
            rel::Environment::FindConstant(name) == nullptr)  // 排除内置常量
        {
            deps.insert(name);
        }
        begin = match.suffix().first;
    }
    return std::vector<std::string>(deps.begin(), deps.end());
}

ParseResult RelParser::ParseSingleStatement(const std::string &code)
{
    ParseResult result;
    result.mode = ParseMode::kStatement;

    ParseResultItem item;
    std::smatch assign_match;

    if (std::regex_match(code, assign_match, kAssignRegex))
    {
        item.name = assign_match[1].str();
        item.content = assign_match[2].str();
        item.type = ItemType::kVariable;
        item.dependencies = ExtractDependencies(item.content, item.name);
        item.status = ResultStatus::kSuccess;
    }
    else
    {
        // 非赋值：先交给 rel::Parse 验证语法
        try
        {
            rel::Parse(code);
            item.name = "";
            item.content = code;
            item.type = ItemType::kExpression;
            item.dependencies = ExtractDependencies(code);
            item.status = ResultStatus::kSuccess;
        }
        catch (const std::exception &e)
        {
            item.name = "";
            item.content = code;
            item.type = ItemType::kError;
            item.message = e.what();
            item.status = ResultStatus::kSyntaxError;
        }
    }

    result.items.push_back(std::move(item));
    return result;
}

ParseResult RelParser::ParseStatements(const std::string &code)
{
    ParseResult result;
    result.mode = ParseMode::kStatement;

    const std::vector<std::string> statements = SplitStatements(code);
    for (const auto &stmt : statements)
    {
        ParseResult single = ParseSingleStatement(stmt);
        result.items.insert(result.items.end(), single.items.begin(), single.items.end());
    }
    return result;
}

ParseResult RelParser::ParseExpression(const std::string &code)
{
    ParseResult result;
    result.mode = ParseMode::kExpression;

    ParseResultItem item;
    item.name = "__expression__";
    item.content = code;
    item.type = ItemType::kExpression;
    item.status = ResultStatus::kSuccess;
    item.dependencies = ExtractDependencies(code);
    result.items.push_back(std::move(item));
    return result;
}

} // namespace rel_engine
} // namespace xequation
