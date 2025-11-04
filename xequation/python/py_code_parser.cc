// Copyright 2024 Your Company. All rights reserved.

#include "py_code_parser.h"

namespace xequation
{
namespace python
{
const char kParserPythonCode[] = R"(
import builtins
import ast

class PyCodeParser:
    def __init__(self):
        self.valid_types = {
            'FunctionDef': 'func',
            'ClassDef': 'class', 
            'Import': 'import',
            'ImportFrom': 'import_from',
            'Assign': 'var'
        }
        self.builtin_names = set(dir(builtins))
    
    def analyze(self, code):
        try:
            tree = ast.parse(code)
            
            if len(tree.body) != 1:
                raise SyntaxError(f"Only one statement allowed, found {len(tree.body)}")
            
            statement = tree.body[0]
            statement_type = type(statement).__name__
            
            if statement_type not in self.valid_types:
                valid_type_names = list(self.valid_types.keys())
                raise ValueError(f"Unsupported statement type: {statement_type}. Supported types: {', '.join(valid_type_names)}")
            
            analyzer_method = getattr(self, f'_analyze_{statement_type}', None)
            if analyzer_method:
                return analyzer_method(statement, code)
            else:
                raise NotImplementedError(f"Analyzer not implemented for statement type: {statement_type}")
                
        except SyntaxError as e:
            if "invalid syntax" in str(e):
                raise SyntaxError(f"Invalid syntax in code: {code}") from e
            raise
    
    def _check_builtin_name(self, name):
        if name in self.builtin_names:
            raise NameError(f"Name '{name}' is a builtin and cannot be redefined")
    
    def _filter_builtin_dependencies(self, dependencies):
        return [dep for dep in dependencies if dep not in self.builtin_names]
    
    def _analyze_FunctionDef(self, node, code):
        self._check_builtin_name(node.name)
        dependencies = self._extract_dependencies(node)
        filtered_deps = self._filter_builtin_dependencies(dependencies)
        
        return {
            'name': node.name,
            'dependencies': filtered_deps,
            'type': 'func',
            'content': code.strip()
        }
    
    def _analyze_ClassDef(self, node, code):
        self._check_builtin_name(node.name)
        dependencies = self._extract_dependencies(node)
        filtered_deps = self._filter_builtin_dependencies(dependencies)
        
        return {
            'name': node.name,
            'dependencies': filtered_deps,
            'type': 'class',
            'content': code.strip()
        }
    
    def _analyze_Import(self, node, code):
        if len(node.names) != 1:
            raise ValueError("Import statement can only import one module at a time")
        
        alias = node.names[0]
        name = alias.asname if alias.asname else alias.name
        self._check_builtin_name(name)
        
        return {
            'name': name,
            'dependencies': [],
            'type': 'import',
            'content': code.strip()
        }
    
    def _analyze_ImportFrom(self, node, code):
        if len(node.names) != 1:
            raise ValueError("From...import statement can only import one symbol at a time")
        
        alias = node.names[0]
        name = alias.asname if alias.asname else alias.name
        self._check_builtin_name(name)
        
        return {
            'name': name,
            'dependencies': [],
            'type': 'import_from',
            'content': code.strip()
        }
    
    def _analyze_Assign(self, node, code):
        if len(node.targets) != 1:
            raise ValueError("Assignment statement can only have one target variable")
        
        target = node.targets[0]
        if not isinstance(target, ast.Name):
            raise ValueError("Assignment target must be a simple variable name")
        
        self._check_builtin_name(target.id)
        dependencies = self._extract_dependencies(node.value)
        filtered_deps = self._filter_builtin_dependencies(dependencies)
        
        return {
            'name': target.id,
            'dependencies': filtered_deps,
            'type': 'var',
            'content': code.strip()
        }
    
    def _extract_dependencies(self, node):
        dependencies = []
        
        class DependencyVisitor(ast.NodeVisitor):
            def __init__(self, deps_list):
                self.deps_list = deps_list
            
            def visit_Name(self, node):
                if isinstance(node.ctx, ast.Load):
                    self.deps_list.append(node.id)
                self.generic_visit(node)
        
        visitor = DependencyVisitor(dependencies)
        visitor.visit(node)
        return list(set(dependencies))
)";

PyCodeParser::PyCodeParser()
{
    py::exec(kParserPythonCode);

    py::module main = py::module::import("__main__");
    py::object python_class = main.attr("PyCodeParser");
    parser_ = python_class();
}

PyCodeParser::~PyCodeParser()
{
    parser_.release();
}

ParseType StringToType(const std::string &type_str)
{
    static const std::unordered_map<std::string, ParseType> type_map = {
        {"func", ParseType::kFuncDecl},
        {"class", ParseType::kClassDecl},
        {"var", ParseType::kVarDecl},
        {"import", ParseType::kImport},
        {"import_from", ParseType::kImportFrom}
    };

    auto it = type_map.find(type_str);
    if (it != type_map.end())
    {
        return it->second;
    }
    return ParseType::kErrorType;
}

ParseResult PyCodeParser::Parse(const std::string &code)
{
    try
    {
        py::dict result = parser_.attr("analyze")(code);

        ParseResult res;
        res.name = result["name"].cast<std::string>();
        res.dependencies = result["dependencies"].cast<std::vector<std::string>>();
        res.type = StringToType(result["type"].cast<std::string>());
        res.content = result["content"].cast<std::string>();

        return res;
    }
    catch (const py::error_already_set &e)
    {
        throw ParseException(e.what());
    }
}
} // namespace python
} // namespace xequation