import builtins
import ast
import importlib
import hashlib

class PythonParser:
    def __init__(self):
        self.valid_types = {
            'FunctionDef': 'func',
            'ClassDef': 'class', 
            'Import': 'import',
            'ImportFrom': 'import_from',
            'Assign': 'var'
        }
        self.builtin_names = set(dir(builtins))
    
    def get_ast_signature(code_string):
        try:
            tree = ast.parse(code_string)
            
            def normalize_node(node):
                if isinstance(node, ast.AST):
                    result = {}
                    result['type'] = type(node).__name__
                    for field, value in ast.iter_fields(node):
                        if field not in ['lineno', 'col_offset', 'end_lineno', 'end_col_offset']:
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
            
        except SyntaxError:
            return hashlib.md5(code_string.encode()).hexdigest()

    def split_statements(self, code):
        try:
            tree = ast.parse(code)
            statements = []
            
            for stmt in tree.body:
                stmt_code = ast.get_source_segment(code, stmt)
                if stmt_code:
                    statements.append(stmt_code.strip())
                else:
                    statements.append(code.strip())
            
            return statements
        except SyntaxError as e:
            if "invalid syntax" in str(e):
                raise SyntaxError(f"Invalid syntax in code: {code}") from e
            raise
    
    def parse_single_statement(self, code):
        try:
            tree = ast.parse(code)
            
            if len(tree.body) != 1:
                raise ValueError(f"parse_single_statement() expects exactly one statement, found {len(tree.body)}")
            
            statement = tree.body[0]
            result = self._parse_statement_node(statement, code)
            
            if isinstance(result, list):
                return result
            else:
                return [result]
                
        except SyntaxError as e:
            if "invalid syntax" in str(e):
                raise SyntaxError(f"Invalid syntax in code: {code}") from e
            raise
    
    def parse_multiple_statements(self, code):
        statements = self.split_statements(code)
        
        results = []
        for stmt_code in statements:
            stmt_results = self.parse_single_statement(stmt_code)
            results.extend(stmt_results)
        
        return results
    
    def _parse_statement_node(self, statement, code):
        statement_type = type(statement).__name__
        
        if statement_type not in self.valid_types:
            valid_type_names = list(self.valid_types.keys())
            raise ValueError(f"Unsupported statement type: {statement_type}. Supported types: {', '.join(valid_type_names)}")
        
        analyzer_method = getattr(self, f'_analyze_{statement_type}', None)
        if analyzer_method:
            return analyzer_method(statement, code)
        else:
            raise NotImplementedError(f"Analyzer not implemented for statement type: {statement_type}")
    
    def _check_builtin_name(self, name):
        if name in self.builtin_names:
            raise NameError(f"Name '{name}' is a builtin and cannot be redefined")

    def _filter_builtin_dependencies(self, dependencies):
        return [dep for dep in dependencies if dep not in self.builtin_names]
    
    def _analyze_FunctionDef(self, node, code):
        self._check_builtin_name(node.name)
        
        return {
            'name': node.name,
            'dependencies': [],
            'type': 'func',
            'content': code.strip()
        }
    
    def _analyze_ClassDef(self, node, code):
        self._check_builtin_name(node.name)
        
        return {
            'name': node.name,
            'dependencies': [],
            'type': 'class',
            'content': code.strip()
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
            
            results.append({
                'name': name,
                'dependencies': [],
                'type': 'import',
                'content': single_import_code
            })
        
        return results
    
    def _analyze_ImportFrom(self, node, code):
        results = []
        
        if any(alias.name == '*' for alias in node.names):
            if node.module is None:
                raise ValueError("Invalid from import: module name is required for star imports")
            
            try:
                imported_module = importlib.import_module(node.module)
                module_symbols = [name for name in dir(imported_module) if not name.startswith('_')]
                
                for symbol_name in module_symbols:
                    results.append({
                        'name': symbol_name,
                        'dependencies': [],
                        'type': 'import_from',
                        'content': f"from {node.module} import {symbol_name}"
                    })
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
                
                results.append({
                    'name': name,
                    'dependencies': [],
                    'type': 'import_from',
                    'content': single_import_code
                })
        
        return results
    
    def _analyze_Assign(self, node, code):
        if len(node.targets) != 1:
            raise ValueError("Assignment statement can only have one target variable")
        
        target = node.targets[0]
        if not isinstance(target, ast.Name):
            raise ValueError("Assignment target must be a variable name")
        
        self._check_builtin_name(target.id)
        dependencies = self._extract_dependencies(node.value)
        filtered_deps = self._filter_builtin_dependencies(dependencies)
        
        # 提取赋值表达式部分
        value_code = ast.get_source_segment(code, node.value)
        
        return {
            'name': target.id,
            'dependencies': filtered_deps,
            'type': 'var',
            'content': value_code.strip() if value_code else code.strip()
        }
    
    def _extract_dependencies(self, node):
        """提取表达式中的所有依赖，包括完整的属性访问路径"""
        dependencies = []
        
        class DependencyVisitor(ast.NodeVisitor):
            def __init__(self, deps_list):
                self.deps_list = deps_list
            
            def visit_Name(self, node):
                if isinstance(node.ctx, ast.Load):
                    self.deps_list.append(node.id)
            
            def visit_Attribute(self, node):
                # 只处理直接的属性访问，不处理调用结果的属性
                if isinstance(node.ctx, ast.Load):
                    # 检查左侧是否是Name或Attribute（不是Call）
                    if isinstance(node.value, (ast.Name, ast.Attribute)):
                        attr_path = self._get_base_attribute_path(node)
                        if attr_path:
                            parts = attr_path.split('.')
                            for i in range(1, len(parts) + 1):
                                partial_path = '.'.join(parts[:i])
                                self.deps_list.append(partial_path)
                
                # 继续分析子节点
                self.generic_visit(node)
            
            def _get_base_attribute_path(self, node):
                """只构建基于变量名的属性路径，不包含函数调用"""
                if isinstance(node, ast.Attribute):
                    left_part = self._get_base_attribute_path(node.value)
                    if left_part and not self._contains_call(node.value):
                        return f"{left_part}.{node.attr}"
                    else:
                        return None
                elif isinstance(node, ast.Name):
                    return node.id
                else:
                    return None
            
            def _contains_call(self, node):
                """检查节点是否包含函数调用"""
                if isinstance(node, ast.Call):
                    return True
                elif isinstance(node, ast.Attribute):
                    return self._contains_call(node.value)
                elif isinstance(node, ast.Name):
                    return False
                return False
        
        visitor = DependencyVisitor(dependencies)
        visitor.visit(node)
        
        # 去重并返回
        return dependencies


def test_parser():
    """测试解析器"""
    parser = PythonParser()
    
    test_cases = [
        ("单语句代码", "def hello():\n    print('world')"),
        ("多语句代码", """
import os, math
x = 10
def test():
    return x + 1
"""),
        ("导入语句", "import os, sys, json as js"),
        ("导入语句", "from math import *"),
        ("导入语句", "from math import pow as print"),
        ("导入语句", "import os.path"),
        ("from导入", "from collections import defaultdict, Counter"),
        ("变量赋值", "file_path = os.path.join('folder', 'file.txt') + a"),
        ("变量赋值", "result = calculate(a, b) + len(data)"),
        ("变量赋值", "t = complex(i, j).real"),
        ("类定义", "class MyClass(BaseClass):\n    value = 123")
    ]
    
    for test_name, code in test_cases:
        print(f"\n{'='*60}")
        print(f"测试: {test_name}")
        print(f"{'='*60}")
        print(f"输入代码:\n{code.strip()}")
        
        # 测试代码分割
        print(f"\n1. 代码分割结果 (split_statements):")
        try:
            statements = parser.split_statements(code)
            for i, stmt in enumerate(statements):
                print(f"   语句 {i}: {stmt}")
        except Exception as e:
            print(f"   分割错误: {e}")
            continue
        
        # 测试单个语句解析
        print(f"\n2. 逐个解析每个语句 (parse_single_statement):")
        for i, stmt in enumerate(statements):
            print(f"   解析语句 {i}:")
            try:
                result = parser.parse_single_statement(stmt)
                if isinstance(result, list):
                    for j, item in enumerate(result):
                        print(f"     [{j}] 名称: {item['name']:15} 类型: {item['type']:10} 依赖: {item['dependencies']}")
                        print(f"      内容: {item['content'][:80]}{'...' if len(item['content']) > 80 else ''}")
                else:
                    print(f"     [0] 名称: {result['name']:15} 类型: {result['type']:10} 依赖: {result['dependencies']}")
                    print(f"      内容: {item['content'][:80]}{'...' if len(item['content']) > 80 else ''}")
            except Exception as e:
                print(f"     解析错误: {e}")
        
        # 测试整体解析
        print(f"\n3. 整体解析结果 (parse_multiple_statements):")
        try:
            results = parser.parse_multiple_statements(code)
            for i, item in enumerate(results):
                print(f"   [{i}] 名称: {item['name']:15} 类型: {item['type']:10} 依赖: {item['dependencies']}")
                print(f"      内容: {item['content'][:80]}{'...' if len(item['content']) > 80 else ''}")
        except Exception as e:
            print(f"   解析错误: {e}")
    
    # 测试单语句解析的严格性
    print(f"\n{'='*60}")
    print("测试单语句解析的严格性")
    print(f"{'='*60}")
    
    multi_stmt_code = "import os\nimport sys"
    print(f"测试代码: {multi_stmt_code}")
    
    print("\n1. 使用 parse_single_statement (应该报错):")
    try:
        result = parser.parse_single_statement(multi_stmt_code)
        print(f"   结果: {result}")
    except Exception as e:
        print(f"   正确报错: {e}")
    
    print("\n2. 使用 parse_multiple_statements (应该成功):")
    try:
        results = parser.parse_multiple_statements(multi_stmt_code)
        for i, item in enumerate(results):
            print(f"   [{i}] 名称: {item['name']:15} 类型: {item['type']:10}")
            print(f"      内容: {item['content'][:80]}{'...' if len(item['content']) > 80 else ''}")
    except Exception as e:
        print(f"   错误: {e}")


if __name__ == "__main__":
    test_parser()