set(xequation_core_SRC
    value.h
    value.cc
    dependency_graph.h
    dependency_graph.cc
    equation.h
    equation.cc
    equation_group.h
    equation_group.cc
    equation_manager.h
    equation_manager.cc
    equation_engine.h
    equation_context.h
    equation_common.h
    equation_signals_manager.h
)

add_library(xequation_core STATIC ${xequation_core_SRC})

target_link_libraries(xequation_core PUBLIC Boost::multi_index)
target_link_libraries(xequation_core PUBLIC Boost::uuid)
target_link_libraries(xequation_core PUBLIC Boost::compute)

target_include_directories(xequation_core PUBLIC ../)
target_include_directories(xequation_core PUBLIC ${TSL_ORDERED_MAP_INCLUDE_DIRS})