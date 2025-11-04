import builtins
import ast

class CodeAnalyzer:
    def __init__(self):
        self.valid_types = {
            'FunctionDef': 'func',
            'ClassDef': 'class', 
            'Import': 'import',
            'ImportFrom': 'import_from',
            'Assign': 'var'
        }
        # 获取所有内置变量名
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
            # 重新抛出但提供更清晰的错误信息
            if "invalid syntax" in str(e):
                raise SyntaxError(f"Invalid syntax in code: {code}") from e
            raise
    
    def _check_builtin_name(self, name):
        """检查名称是否是内置变量，如果是则抛出异常"""
        if name in self.builtin_names:
            raise NameError(f"Name '{name}' is a builtin and cannot be redefined")
    
    def _filter_builtin_dependencies(self, dependencies):
        """过滤掉内置变量依赖"""
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
        """从节点中提取依赖的变量名"""
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


class CodeExecutor:
    """
    代码执行器类，提供隔离的exec和eval功能，结合语法分析
    """
    
    def __init__(self):
        """初始化执行器，设置固定的全局命名空间"""
        self.global_dict = {'__builtins__': builtins}
        self.analyzer = CodeAnalyzer()
    
    def analyze_code(self, code_string):
        """
        分析代码字符串，返回分析结果
        
        Args:
            code_string: 要分析的代码字符串
            
        Returns:
            分析结果字典
        """
        return self.analyzer.analyze(code_string)
    
    def exec(self, code_string, local_dict=None):
        """
        执行给定的代码字符串（先进行语法分析）
        
        Args:
            code_string: 要执行的代码字符串
            local_dict: 局部变量字典，如果为None则创建空字典
        """
        if local_dict is None:
            local_dict = {}
        
        # 先进行语法分析
        analysis_result = self.analyze_code(code_string)
        
        # 检查依赖是否满足
        self._check_dependencies(analysis_result['dependencies'], local_dict)
        
        # 执行代码
        exec(code_string, self.global_dict, local_dict)
        return local_dict
    
    def eval(self, expression, local_dict=None):
        """
        评估给定的表达式
        
        Args:
            expression: 要评估的表达式
            local_dict: 局部变量字典，如果为None则创建空字典
        
        Returns:
            表达式的结果
        """
        if local_dict is None:
            local_dict = {}
        
        # 评估表达式
        return eval(expression, self.global_dict, local_dict)
    
    def execute_script(self, code_string, initial_locals=None):
        """
        执行脚本并返回所有局部变量
        
        Args:
            code_string: 要执行的代码字符串
            initial_locals: 初始局部变量字典
        
        Returns:
            执行后的局部变量字典
        """
        local_dict = initial_locals.copy() if initial_locals else {}
        self.exec(code_string, local_dict)
        return local_dict
    
    def get_available_builtins(self):
        """
        获取可用的内置函数列表
        
        Returns:
            内置函数名称列表
        """
        return list(self.global_dict['__builtins__'].__dict__.keys())
    
    def _check_dependencies(self, dependencies, local_dict):
        """
        检查依赖是否满足
        
        Args:
            dependencies: 依赖的变量名列表
            local_dict: 局部变量字典
            
        Raises:
            NameError: 如果依赖不满足
        """
        for dep in dependencies:
            if dep not in local_dict:
                raise NameError(f"Undefined variable: '{dep}'")


# 使用示例
if __name__ == "__main__":
    executor = CodeExecutor()
    
    # 测试语法分析
    print("=== 语法分析测试 ===")
    
    test_cases = [
        "x = 10",
        "def hello(): return 'world'",
        "class MyClass: pass",
        "import math",
        "from os import path"
    ]
    
    for code in test_cases:
        try:
            result = executor.analyze_code(code)
            print(f"代码: {code}")
            print(f"分析结果: {result}\n")
        except Exception as e:
            print(f"代码: {code}")
            print(f"错误: {type(e).__name__}: {e}\n")
    
    # 测试内置变量保护
    print("=== 内置变量保护测试 ===")
    try:
        executor.analyze_code("print = 123")  # 尝试覆盖内置函数
    except NameError as e:
        print(f"正确捕获异常: {e}")
    
    try:
        executor.analyze_code("len = 456")  # 尝试覆盖内置函数
    except NameError as e:
        print(f"正确捕获异常: {e}")
    
    # 测试错误情况
    print("=== 错误情况测试 ===")
    
    error_cases = [
        ("x = 10; y = 20", "多语句"),
        ("import math, os", "多模块导入"),
        ("from os import path, system", "多符号导入"),
        ("x, y = 1, 2", "多变量赋值"),
        ("123 = x", "无效赋值目标"),
        ("invalid syntax here", "语法错误")
    ]
    
    for code, description in error_cases:
        try:
            executor.analyze_code(code)
        except Exception as e:
            print(f"{description}: {type(e).__name__}: {e}")
    
    # 测试依赖检查
    print("\n=== 依赖检查测试 ===")
    locals_dict = {}
    
    # 先定义变量
    executor.exec("a = 5", locals_dict)
    print(f"定义变量后: {locals_dict}")
    
    # 使用已定义的变量（应该成功）
    try:
        executor.exec("b = a + 10", locals_dict)
        print(f"使用依赖成功: {locals_dict}")
    except Exception as e:
        print(f"错误: {type(e).__name__}: {e}")
    
    # 使用未定义的变量（应该失败）
    try:
        executor.exec("c = undefined_var + 10", locals_dict)
    except NameError as e:
        print(f"正确捕获依赖错误: {e}")
    
    # 测试内置变量在依赖中过滤
    print("=== 内置变量依赖过滤测试 ===")
    analysis = executor.analyze_code("result = len([1,2,3]) + max([4,5,6])")
    print(f"分析结果: {analysis}")
    print("注意: len和max等内置函数不会出现在dependencies中")