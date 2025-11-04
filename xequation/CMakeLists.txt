find_package(Python REQUIRED COMPONENTS Development Interpreter)

if(Python_FOUND)
    execute_process(
        COMMAND ${Python_EXECUTABLE} -c "import pybind11; print(pybind11.get_cmake_dir())"
        OUTPUT_VARIABLE pybind11_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    message(STATUS "pybind11 DIR: ${pybind11_DIR}")
endif()

find_package(pybind11 REQUIRED)

find_package(Boost REQUIRED COMPONENTS multi_index filesystem)

message(STATUS "Python executable: ${Python_EXECUTABLE}")
message(STATUS "Python include dirs: ${Python_INCLUDE_DIRS}")
message(STATUS "Python libraries: ${Python_LIBRARIES}")

set(xequation_SRC
    core/value.h
    core/value.cc
    core/dependency_graph.h
    core/dependency_graph.cc
    core/equation.h
    core/event_stamp.h
    core/event_stamp.cc
    core/equation_manager.h
    core/equation_manager.cc
    core/expr_engine.h
    core/expr_common.h
    python/py_base.h
    python/py_code_executor.h
    python/py_code_executor.cc
    python/py_code_parser.h
    python/py_code_parser.cc
    python/value_pybind_converter.h
    python/value_pybind_converter.cc
    python/py_expr_context.h
    python/py_expr_context.cc
    python/py_expr_engine.h
    python/py_expr_engine.cc
)

add_library(xequation STATIC ${xequation_SRC} )

set_target_properties(xequation PROPERTIES CXX_VISIBILITY_PRESET hidden)

target_link_libraries(xequation PUBLIC pybind11::headers)
target_link_libraries(xequation PUBLIC Python::Python)
target_link_libraries(xequation PUBLIC Boost::multi_index Boost::filesystem)

target_include_directories(xequation PUBLIC ./)