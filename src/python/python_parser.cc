#include "python_parser.h"
#include "core/equation.h"
#include "core/equation_common.h"
#include "python/python_common.h"
#include "python_parser_embed.h"
#include <pybind11/gil.h>
#include <string>
#include <vector>

namespace xequation
{
namespace python
{

PythonParser::PythonParser()
{
    pybind11::gil_scoped_acquire acquire;
    std::string py_code(reinterpret_cast<const char *>(python_parser_data), python_parser_size);
    pybind11::exec(py_code);

    pybind11::module main = pybind11::module::import("__main__");
    pybind11::object python_class = main.attr("PythonParser");
    parser_ = python_class();
}

PythonParser::~PythonParser()
{
    parser_.release();
}

std::vector<std::string> PythonParser::SplitStatements(const std::string &code)
{
    pybind11::gil_scoped_acquire acquire;
    try
    {
        pybind11::list result = parser_.attr("split_statements")(code);

        std::vector<std::string> statements;
        for (const auto &item : result)
        {
            statements.push_back(item.cast<std::string>());
        }

        return statements;
    }
    catch (const pybind11::error_already_set &e)
    {
        pybind11::object pv = e.value();
        pybind11::object str_func = pybind11::module_::import("builtins").attr("str");
        std::string error_msg = str_func(pv).cast<std::string>();
        throw ParseException(error_msg);
    }
}

ParseResult PythonParser::ParseStatements(const std::string &code)
{
    pybind11::gil_scoped_acquire acquire;
    try
    {
        std::vector<std::string> statements = SplitStatements(code);
        ParseResult result;
        result.mode = ParseMode::kStatement;
        for (const auto &stmt_code : statements)
        {
            ParseResult stmt_results = ParseSingleStatement(stmt_code);
            result.items.insert(result.items.end(), stmt_results.items.begin(), stmt_results.items.end());
        }
        return result;
    }
    catch (const pybind11::error_already_set &e)
    {
        pybind11::object pv = e.value();
        pybind11::object str_func = pybind11::module_::import("builtins").attr("str");
        std::string error_msg = str_func(pv).cast<std::string>();
        throw ParseException(error_msg);
    }
}

ParseResult PythonParser::ParseSingleStatement(const std::string &code)
{
    pybind11::gil_scoped_acquire acquire;
    try
    {
        std::string code_hash = parser_.attr("compute_code_hash")(code).cast<std::string>();

        auto cached_result = parse_result_cache_.get(code_hash);
        if (cached_result)
        {
            return *cached_result;
        }

        pybind11::list py_parse_result = parser_.attr("parse_single_statement")(code);

        ParseResult result;
        result.mode = ParseMode::kStatement;
        for (const auto &item : py_parse_result)
        {
            pybind11::dict item_dict = item.cast<pybind11::dict>();
            std::string name = item_dict["name"].cast<std::string>();
            std::vector<std::string> dependencies = item_dict["dependencies"].cast<std::vector<std::string>>();
            ItemType type = ItemTypeConverter::FromString(item_dict["type"].cast<std::string>());
            std::string content = item_dict["content"].cast<std::string>();
            std::string message = item_dict["message"].cast<std::string>();
            std::string status_str = item_dict["status"].cast<std::string>();

            ParseResultItem parse_item;
            parse_item.name = name;
            parse_item.dependencies = dependencies;
            parse_item.type = type;
            parse_item.content = content;
            parse_item.message = message;
            parse_item.status = ResultStatusConverter::FromString(status_str);
            result.items.push_back(parse_item);
        }

        parse_result_cache_.insert(code_hash, result);

        return result;
    }
    catch (const pybind11::error_already_set &e)
    {
        pybind11::object pv = e.value();
        pybind11::object str_func = pybind11::module_::import("builtins").attr("str");
        std::string error_msg = str_func(pv).cast<std::string>();
        throw ParseException(error_msg);
    }
}

ParseResult PythonParser::ParseExpression(const std::string &code)
{
    pybind11::gil_scoped_acquire acquire;
    try
    {
        pybind11::list py_parse_result = parser_.attr("parse_expression_dependencies")(code);

        ParseResult parse_result;
        parse_result.mode = ParseMode::kExpression;
        ParseResultItem parse_item;
        parse_item.name = "__expression__";
        parse_item.content = code;
        parse_item.type = ItemType::kExpression;
        parse_item.status = ResultStatus::kSuccess;
        for (const auto &item : py_parse_result)
        {
            parse_item.dependencies.push_back(item.cast<std::string>());
        }
        parse_result.items.push_back(parse_item);
        return parse_result;
    }
    catch (const pybind11::error_already_set &e)
    {
        pybind11::object pv = e.value();
        pybind11::object str_func = pybind11::module_::import("builtins").attr("str");
        std::string error_msg = str_func(pv).cast<std::string>();
        ParseResult parse_result;
        parse_result.mode = ParseMode::kExpression;
        ParseResultItem parse_item;
        parse_item.name = "__expression__";
        parse_item.content = code;
        parse_item.type = ItemType::kError;
        parse_item.status = MapPythonExceptionToStatus(e);
        parse_item.message = error_msg;
        parse_result.items.push_back(parse_item);
        return parse_result;
    }
}
} // namespace python
} // namespace xequation