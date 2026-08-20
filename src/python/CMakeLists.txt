set(xequation_python_SRC
    python_base.h
    python_executor.h
    python_executor.cc
    python_parser.h
    python_parser.cc
    python_parser_embedded.h
    value_pybind_converter.h
    python_equation_context.h
    python_equation_context.cc
    python_equation_engine.h
    python_equation_engine.cc
)

add_library(xequation_python STATIC ${xequation_python_SRC})

set_target_properties(xequation_python PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_link_libraries(xequation_python PUBLIC pybind11::headers)
target_link_libraries(xequation_python PUBLIC Python::Python)
# Python 环境管理/初始化委托给 REL 的 python_manager（幂等，仅宿主初始化）
target_link_libraries(xequation_python PUBLIC python_manager)
target_link_libraries(xequation_python PUBLIC xequation_core)

target_include_directories(xequation_python PUBLIC ../)