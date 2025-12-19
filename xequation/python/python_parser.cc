// Copyright 2024 Your Company. All rights reserved.

#include "python_parser.h"
#include "core/equation.h"
#include "core/equation_common.h"
#include "python/python_common.h"
#include <pybind11/gil.h>
#include <string>
#include <vector>

namespace xequation
{
namespace python
{
const char kParserPythonCode[] = R"(
import builtins
import ast
import importlib
import hashlib


class PythonParser:
    def __init__(self):
        self.valid_types = {
            "FunctionDef": "func",
            "ClassDef": "class",
            "Import": "import",
            "ImportFrom": "import_from",
            "Assign": "var",
        }
        self.builtin_names = set(dir(builtins))

    def get_ast_signature(self, code_string):
        tree = ast.parse(code_string)

        def normalize_node(node):
            if isinstance(node, ast.AST):
                result = {"type": type(node).__name__}
                for field, value in ast.iter_fields(node):
                    if field not in ("lineno", "col_offset", "end_lineno", "end_col_offset"):
                        if isinstance(value, list):
                            result[field] = [normalize_node(item) for item in value]
                        elif isinstance(value, ast.AST):
                            result[field] = normalize_node(value)
                        else:
                            result[field] = value
                return result
            return node

        normalized_ast = normalize_node(tree)
        signature = str(normalized_ast)
        return hashlib.md5(signature.encode()).hexdigest()

    def split_statements(self, code):
        tree = ast.parse(code)
        statements = []

        for stmt in tree.body:
            stmt_code = ast.get_source_segment(code, stmt)
            if stmt_code:
                statements.append(stmt_code.strip())
            else:
                statements.append(code.strip())

        return statements

    def parse_single_statement(self, code):
        tree = ast.parse(code)

        if len(tree.body) != 1:
            raise ValueError(
                f"parse_single_statement() expects exactly one statement, found {len(tree.body)}"
            )

        statement = tree.body[0]
        result = self._parse_statement_node(statement, code)

        if isinstance(result, list):
            return result
        return [result]

    def parse_multiple_statements(self, code):
        statements = self.split_statements(code)

        results = []
        for stmt_code in statements:
            stmt_results = self.parse_single_statement(stmt_code)
            results.extend(stmt_results)

        return results

    def parse_expression_dependencies(self, expression_str):
        expr_node = ast.parse(expression_str, mode="eval").body
        dependencies = self._extract_dependencies(expr_node)
        return dependencies

    def _parse_statement_node(self, statement, code):
        statement_type = type(statement).__name__

        if statement_type not in self.valid_types:
            valid_type_names = list(self.valid_types.keys())
            raise ValueError(
                f"Unsupported statement type: {statement_type}. Supported types: {', '.join(valid_type_names)}"
            )

        analyzer_method = getattr(self, f"_analyze_{statement_type}", None)
        if analyzer_method:
            return analyzer_method(statement, code)
        raise NotImplementedError(
            f"Analyzer not implemented for statement type: {statement_type}"
        )

    def _check_builtin_name(self, name):
        if name in self.builtin_names:
            raise NameError(f"Name '{name}' is a builtin and cannot be redefined")

    def _analyze_FunctionDef(self, node, code):
        self._check_builtin_name(node.name)

        return {
            "name": node.name,
            "dependencies": [],
            "type": "Function",
            "content": code.strip(),
        }

    def _analyze_ClassDef(self, node, code):
        self._check_builtin_name(node.name)

        return {
            "name": node.name,
            "dependencies": [],
            "type": "Class",
            "content": code.strip(),
        }

    def _analyze_Import(self, node, code):
        results = []

        for alias in node.names:
            if alias.asname:
                self._check_builtin_name(alias.asname)
            name = alias.asname if alias.asname else alias.name

            single_import_code = f"import {alias.name}"
            if alias.asname:
                single_import_code += f" as {alias.asname}"

            results.append(
                {
                    "name": name,
                    "dependencies": [],
                    "type": "Import",
                    "content": single_import_code,
                }
            )

        return results

    def _analyze_ImportFrom(self, node, code):
        results = []

        if any(alias.name == "*" for alias in node.names):
            if node.module is None:
                raise ValueError("Invalid from import: module name is required for star imports")

            try:
                imported_module = importlib.import_module(node.module)
                module_symbols = [name for name in dir(imported_module) if not name.startswith("_")]

                for symbol_name in module_symbols:
                    results.append(
                        {
                            "name": symbol_name,
                            "dependencies": [],
                            "type": "ImportFrom",
                            "content": f"from {node.module} import {symbol_name}",
                        }
                    )
            except ImportError as e:
                raise ImportError(f"Cannot import module '{node.module}' for star import: {e}")
        else:
            for alias in node.names:
                if alias.asname:
                    self._check_builtin_name(alias.asname)
                name = alias.asname if alias.asname else alias.name

                single_import_code = f"from {node.module} import {alias.name}"
                if alias.asname:
                    single_import_code += f" as {alias.asname}"

                results.append(
                    {
                        "name": name,
                        "dependencies": [],
                        "type": "ImportFrom",
                        "content": single_import_code,
                    }
                )

        return results

    def _analyze_Assign(self, node, code):
        if len(node.targets) != 1:
            raise ValueError("Assignment statement can only have one target variable")

        target = node.targets[0]
        if not isinstance(target, ast.Name):
            raise ValueError("Assignment target must be a variable name")

        self._check_builtin_name(target.id)
        dependencies = self._extract_dependencies(node.value)

        value_code = ast.get_source_segment(code, node.value)

        return {
            "name": target.id,
            "dependencies": dependencies,
            "type": "Variable",
            "content": value_code.strip() if value_code else code.strip(),
        }

    def _extract_dependencies(self, node):
        dependencies = []

        class DependencyVisitor(ast.NodeVisitor):
            def __init__(self, deps_list):
                self.deps_list = deps_list

            def visit_Name(self, node):
                if isinstance(node.ctx, ast.Load):
                    self.deps_list.append(node.id)

            def visit_Attribute(self, node):
                if isinstance(node.ctx, ast.Load):
                    if isinstance(node.value, (ast.Name, ast.Attribute)):
                        attr_path = self._get_base_attribute_path(node)
                        if attr_path:
                            parts = attr_path.split(".")
                            for i in range(1, len(parts) + 1):
                                partial_path = ".".join(parts[:i])
                                self.deps_list.append(partial_path)
                self.generic_visit(node)

            def _get_base_attribute_path(self, node):
                if isinstance(node, ast.Attribute):
                    left_part = self._get_base_attribute_path(node.value)
                    if left_part and not self._contains_call(node.value):
                        return f"{left_part}.{node.attr}"
                    return None
                if isinstance(node, ast.Name):
                    return node.id
                return None

            def _contains_call(self, node):
                if isinstance(node, ast.Call):
                    return True
                if isinstance(node, ast.Attribute):
                    return self._contains_call(node.value)
                if isinstance(node, ast.Name):
                    return False
                return False

        visitor = DependencyVisitor(dependencies)
        visitor.visit(node)

        return list(dict.fromkeys(dependencies))
)";

PythonParser::PythonParser()
{
    pybind11::gil_scoped_acquire acquire;
    pybind11::exec(kParserPythonCode);

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
        for (const auto& item : result)
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
        for (const auto& stmt_code : statements)
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

    std::string code_hash = parser_.attr("get_ast_signature")(code).cast<std::string>();

    auto cached_result = parse_result_cache_.get(code_hash);
    if (cached_result)
    {
        return *cached_result;
    }
    try
    {
        pybind11::list py_parse_result = parser_.attr("parse_single_statement")(code);

        ParseResult result;
        result.mode = ParseMode::kStatement;
        for (const auto& item : py_parse_result)
        {
            pybind11::dict item_dict = item.cast<pybind11::dict>();
            std::string name = item_dict["name"].cast<std::string>();
            std::vector<std::string> dependencies = item_dict["dependencies"].cast<std::vector<std::string>>();
            ItemType type = ItemTypeConverter::FromString(item_dict["type"].cast<std::string>());
            std::string content = item_dict["content"].cast<std::string>();
            
            ParseResultItem parse_item;
            parse_item.name = name;
            parse_item.dependencies = dependencies;
            parse_item.type = type;
            parse_item.content = content;
            parse_item.status = ResultStatus::kSuccess;
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
        for (const auto& item : py_parse_result)
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