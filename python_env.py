import sys
import os
import site
import pybind11
import math

def print_python_config():
    print("Python path configuration:")
    print(f"  PYTHONHOME = {os.environ.get('PYTHONHOME', '(not set)')}")
    print(f"  PYTHONPATH = {os.environ.get('PYTHONPATH', '(not set)')}")
    print(f"  program name = '{os.path.basename(sys.argv[0])}'")
    print(f"  isolated = {1 if sys.flags.isolated else 0}")
    print(f"  environment = {0 if sys.flags.ignore_environment else 1}")
    print(f"  user site = {0 if sys.flags.no_user_site else 1}")
    print(f"  import site = {0 if sys.flags.no_site else 1}")
    print(f"  is in build tree = {0}")
    print(f"  stdlib dir = '{sys.prefix}/lib/python{sys.version_info.major}.{sys.version_info.minor}'")
    print(f"  sys._base_executable = '{getattr(sys, '_base_executable', sys.executable)}'")
    print(f"  sys.base_prefix = '{sys.base_prefix}'")
    print(f"  sys.base_exec_prefix = '{sys.base_exec_prefix}'")
    print(f"  sys.executable = '{sys.executable}'")
    print(f"  sys.prefix = '{sys.prefix}'")
    print(f"  sys.exec_prefix = '{sys.exec_prefix}'")
    print("  sys.path = [")
    for path in sys.path:
        print(f"    '{path}',")
    print("  ]")
    print(f"  site-package = {site.getsitepackages()}")

if __name__ == "__main__":
    print_python_config()
    print(pybind11.get_cmake_dir())
    a = 0.5
    d = math.sin(a * math.pi)
    print(d)