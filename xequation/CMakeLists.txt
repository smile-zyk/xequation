find_package(Boost REQUIRED COMPONENTS multi_index)

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
    core/equation_engine.h
    core/equation_context.h
    core/equation_common.h
    python/python_base.h
    python/python_executor.h
    python/python_executor.cc
    python/python_parser.h
    python/python_parser.cc
    python/value_pybind_converter.h
    python/value_pybind_converter.cc
    python/python_equation_context.h
    python/python_equation_context.cc
    python/python_equation_engine.h
    python/python_equation_engine.cc
)

add_library(xequation STATIC ${xequation_SRC})

set_target_properties(xequation PROPERTIES CXX_VISIBILITY_PRESET hidden)

target_link_libraries(xequation PUBLIC pybind11::headers)
target_link_libraries(xequation PUBLIC Python::Python)
target_link_libraries(xequation PUBLIC Boost::multi_index)

target_include_directories(xequation PUBLIC ./)