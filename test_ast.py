import ast

class AdvancedVariableVisitor(ast.NodeVisitor):
    def __init__(self):
        self.variables = set()
        self.function_calls = set()
        self.attributes = set()
    
    def visit_Name(self, node):
        if isinstance(node.ctx, ast.Load):
            self.variables.add(node.id)
        self.generic_visit(node)
    
    def visit_Call(self, node):
        # 处理函数调用
        if isinstance(node.func, ast.Name):
            self.function_calls.add(node.func.id)
        elif isinstance(node.func, ast.Attribute):
            # 处理类似 module.function() 的调用
            attr_name = self._get_attribute_chain(node.func)
            self.attributes.add(attr_name)
        self.generic_visit(node)
    
    def visit_Attribute(self, node):
        # 处理属性访问，如 obj.attr
        attr_name = self._get_attribute_chain(node)
        self.attributes.add(attr_name)
        self.generic_visit(node)
    
    def _get_attribute_chain(self, node):
        """获取属性访问链，如 a.b.c"""
        parts = []
        current = node
        while isinstance(current, ast.Attribute):
            parts.append(current.attr)
            current = current.value
        if isinstance(current, ast.Name):
            parts.append(current.id)
        return '.'.join(reversed(parts))

def analyze_expression(expr):
    """全面分析表达式"""
    try:
        tree = ast.parse(expr, mode='eval')
        visitor = AdvancedVariableVisitor()
        visitor.visit(tree)
        
        return {
            'variables': sorted(list(visitor.variables)),
            'function_calls': sorted(list(visitor.function_calls)),
            'attributes': sorted(list(visitor.attributes)),
            'expression': expr
        }
    except SyntaxError as e:
        return {'error': str(e), 'expression': expr}

# 测试复杂表达式
complex_expressions = [
    "x + math.sin(y) + z**2",
    "df['column'].sum() + len(items)",
    "np.array(data).reshape(rows, cols)",
    "obj.method(x) + Class.attribute",
    "(a + b) if condition else default_value",
    "os.path.join('folder', 'file.txt')"
]

for expr in complex_expressions:
    result = analyze_expression(expr)
    print(f"表达式: {expr}")
    if 'error' not in result:
        print(f"变量: {result['variables']}")
        print(f"函数调用: {result['function_calls']}")
        print(f"属性访问: {result['attributes']}")
    else:
        print(f"错误: {result['error']}")
    print("=" * 50)