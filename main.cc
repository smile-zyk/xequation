#include <pybind11/embed.h>
#include <iostream>

namespace py = pybind11;

int main() 
{
    py::scoped_interpreter guard{}; // 初始化 Python 解释器
    
    py::exec("print('Hello from pybind11 embedded Python!')");
    
    auto math = py::module_::import("math");
    double root = math.attr("sqrt")(25).cast<double>();
    std::cout << "Square root of 25 is: " << root << std::endl;
    
    py::exec("x = 42");
    auto x = py::globals()["x"].cast<int>();
    std::cout << "Python variable x = " << x << std::endl;
    
    try {
        py::module_::import("script");  // 导入 script.py
    } catch (const py::error_already_set& e) {
        std::cerr << "Python error: " << e.what() << std::endl;
    }
    
    return 0;
}