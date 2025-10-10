#include "py_symbol_extractor.h"

using namespace xexprengine;

ParseResult PySymbolExtractor::Extract(const std::string &py_code)
{
    auto it = cache_map_.find(py_code);
    if (it != cache_map_.end())
    {
        return it->second->value;
    }

    ParseResult result = Parse(py_code);

    cache_list_.emplace_front(py_code, result);
    cache_map_[py_code] = cache_list_.begin();

    EvictLRU();

    return result;
}

void PySymbolExtractor::ClearCache()
{
    cache_list_.clear();
    cache_map_.clear();
}

void PySymbolExtractor::SetMaxCacheSize(size_t max_size)
{
    max_cache_size_ = max_size;
    while (cache_list_.size() > max_cache_size_)
    {
        EvictLRU();
    }
}

size_t PySymbolExtractor::GetCacheSize()
{
    return cache_list_.size();
}

ParseResult PySymbolExtractor::Parse(const std::string &py_code)
{
    ParseResult result;
    result.status = VariableStatus::kParseSuccess;

    try
    {
        py::gil_scoped_acquire acquire;
        py::object ast_module = py::module::import("ast");
        auto tree = ast_module.attr("parse")(py_code);

        auto walk_iter = ast_module.attr("walk")(tree);

        for (auto node : walk_iter)
        {
            ProcessNode(node, result);
        }
    }
    catch (const py::error_already_set &e)
    {
        result.status = VariableStatus::kParseSyntaxError;
        result.parse_error_message = e.what();
    }

    return result;
}

void PySymbolExtractor::ProcessNameNode(py::handle node, ParseResult &result)
{
    if (py::hasattr(node, "ctx"))
    {
        auto ctx = node.attr("ctx");

        if (py::isinstance<py::object>(ctx))
        {
            if (py::hasattr(ctx, "__class__"))
            {
                auto ctx_class = ctx.attr("__class__").attr("__name__").cast<std::string>();

                if (ctx_class == "Load" || ctx_class == "Store")
                {
                    auto id = node.attr("id").cast<std::string>();
                    result.variables.insert(id);
                }
            }
        }
    }
}

void PySymbolExtractor::ProcessCallNode(py::handle node, ParseResult &result)
{
    if (py::hasattr(node, "func"))
    {
        auto func = node.attr("func");

        if (py::hasattr(func, "id"))
        {
            auto func_id = func.attr("id").cast<std::string>();
            result.functions.insert(func_id);
        }
    }
}

void PySymbolExtractor::ProcessNode(py::handle node, ParseResult &result)
{
    auto node_type = node.attr("__class__").attr("__name__").cast<std::string>();

    if (node_type == "Name")
    {
        ProcessNameNode(node, result);
    }
    else if (node_type == "Call")
    {
        ProcessCallNode(node, result);
    }
}

void PySymbolExtractor::EvictLRU()
{
    if (cache_list_.size() > max_cache_size_)
    {
        auto last = cache_list_.back();
        cache_map_.erase(last.key);
        cache_list_.pop_back();
    }
}
