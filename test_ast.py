import ast

class SymbolExtractor(ast.NodeVisitor):
    def __init__(self):
        self.symbols = {
            'variables': set(),      # 普通变量符号
            'functions': set()        # 普通函数调用符号
        }
    
    def visit_Name(self, node):
        """处理变量名"""
        if isinstance(node.ctx, ast.Load):
            # 如果是读取变量（使用变量）
            self.symbols['variables'].add(node.id)
        elif isinstance(node.ctx, ast.Store):
            # 如果是写入变量（赋值变量）
            self.symbols['variables'].add(node.id)
        self.generic_visit(node)
    
    def visit_Call(self, node):
        """处理函数调用 - 只处理普通函数调用"""
        if isinstance(node.func, ast.Name):
            # 只处理普通函数调用（如 sin()）
            self.symbols['functions'].add(node.func.id)
        
        # 继续遍历参数中的符号
        self.generic_visit(node)
    
    def get_symbols(self):
        """返回整理后的符号"""
        return {
            'variables': sorted(self.symbols['variables']),
            'functions': sorted(self.symbols['functions'])
        }

# 测试代码
test_code = """
import numpy
result = numpy.cos(1) + test(b) + test.a + obj.method()
x = obj.attribute
"""

def extract_symbols_from_code(code_string):
    """从代码中提取符号"""
    try:
        tree = ast.parse(code_string)
        extractor = SymbolExtractor()
        extractor.visit(tree)
        return extractor.get_symbols()
    except SyntaxError as e:
        print(f"语法错误: {e}")
        return {'variables': [], 'functions': []}

# 执行提取
symbols = extract_symbols_from_code(test_code)

print("提取到的符号:")
print("=" * 50)
print("变量:", symbols['variables'])
print("函数调用:", symbols['functions'])