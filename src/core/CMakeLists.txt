set(xequation_core_SRC
    equation_value.h
    dependency_graph.h
    dependency_graph.cc
    equation_manager.h
    equation_manager.cc
    equation_common.h
    equation_signals_manager.h
)

add_library(xequation_core STATIC ${xequation_core_SRC})

target_link_libraries(xequation_core PUBLIC Boost::multi_index)
target_link_libraries(xequation_core PUBLIC Boost::uuid)
target_link_libraries(xequation_core PUBLIC Boost::compute)
target_link_libraries(xequation_core PUBLIC rel)

target_include_directories(xequation_core PUBLIC ../)
target_include_directories(xequation_core PUBLIC ${TSL_ORDERED_MAP_INCLUDE_DIRS})