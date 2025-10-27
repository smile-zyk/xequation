import ast

def is_expression(s):
    try:
        ast.parse(s, mode='eval')  # 尝试解析为表达式
        return True
    except SyntaxError:
        return False

def is_statement(s):
    try:
        ast.parse(s)  # 默认mode='exec'，用于解析语句
        return True
    except SyntaxError:
        return False

# 示例用法
code1 = "x + 1"  # 表达式
code2 = "import os"  # 语句
code3 = "if x > 0: print(x)"  # 语句

print(is_expression(code1))  # True
print(is_expression(code2))  # False
print(is_statement(code2))   # True
print(is_statement(code3))   # True

code = """
a = 10
b = 20
result = a + b
"""

local_vars = {}
exec(code, globals(), local_vars)

print(local_vars['a'])       # 输出: 10
print(local_vars['b'])       # 输出: 20
print(local_vars['result'])  # 输出: 30