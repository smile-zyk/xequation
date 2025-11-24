#!/usr/bin/env python3
"""
简单测试 RestrictedPython 的 compile_restricted 功能
"""

from RestrictedPython import compile_restricted
from RestrictedPython.Guards import safe_builtins

def test_simple():
    """测试最简单的 compile_restricted 用法"""
    print("开始测试 compile_restricted...")
    
    # 创建安全环境（对应 C++ 中的 safe_globals_, local）
    safe_globals_ = {'__builtins__': safe_builtins}
    local = {}
    
    try:
        # 对应 C++ 代码的第一行
        # pybind11::object compile_restricted = restrictedpython_module_.attr("compile_restricted");
        compile_restricted_func = compile_restricted
        
        print("✓ 成功获取 compile_restricted 函数")
        
        # 对应 C++ 代码的第二行
        # pybind11::object bytecode = compile_restricted("1234", "<string>", "eval");
        bytecode = compile_restricted_func("1234", "<string>", "eval")
        
        print("✓ 成功编译字节码")
        print(f"字节码类型: {type(bytecode)}")
        
        # 对应 C++ 代码的第三行
        # pybind11::object result = pybind11::eval(bytecode, safe_globals_, local);
        result = eval(bytecode, safe_globals_, local)
        
        print("✓ 成功执行字节码")
        print(f"执行结果: {result}")
        print(f"结果类型: {type(result).__name__}")
        
        return True
        
    except Exception as e:
        print(f"✗ 发生错误: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_multiple_cases():
    """测试多个简单的表达式"""
    test_cases = [
        "1234",
        "1 + 2",
        "3 * 4",
        "'hello'",
        "[1, 2, 3]"
    ]
    
    safe_globals_ = {'__builtins__': safe_builtins}
    local = {}
    
    for code in test_cases:
        print(f"\n测试表达式: {code}")
        try:
            bytecode = compile_restricted(code, "<string>", "eval")
            result = eval(bytecode, safe_globals_, local)
            print(f"✓ 成功: {result}")
        except Exception as e:
            print(f"✗ 失败: {e}")

if __name__ == "__main__":
    print("RestrictedPython 简单测试")
    print("=" * 40)
    
    # 检查是否安装了 RestrictedPython
    try:
        import RestrictedPython
        print(f"找到 RestrictedPython (版本: {getattr(RestrictedPython, '__version__', '未知')})")
    except ImportError:
        print("错误: 未安装 RestrictedPython")
        print("请运行: pip install RestrictedPython")
        exit(1)
    
    print("\n运行主要测试...")
    success = test_simple()
    
    if success:
        print("\n运行额外测试用例...")
        test_multiple_cases()
    
    print("\n测试完成!")