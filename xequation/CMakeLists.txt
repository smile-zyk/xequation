find_package(Boost REQUIRED COMPONENTS multi_index uuid)
find_path(TSL_ORDERED_MAP_INCLUDE_DIRS "tsl/ordered_hash.h")
set(xequation_SRC
    core/value.h
    core/value.cc
    core/dependency_graph.h
    core/dependency_graph.cc
    core/equation.h
    core/equation.cc
    core/equation_group.h
    core/equation_group.cc
    core/equation_manager.h
    core/equation_manager.cc
    core/equation_engine.h
    core/equation_context.h
    core/equation_common.h
    core/equation_signals_manager.h
    python/python_base.h
    python/python_executor.h
    python/python_executor.cc
    python/python_parser.h
    python/python_parser.cc
    python/value_pybind_converter.h
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
target_include_directories(xequation PUBLIC ${TSL_ORDERED_MAP_INCLUDE_DIRS})
target_link_libraries(xequation PUBLIC Boost::uuid)