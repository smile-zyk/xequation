set(xequation_rel_SRC
    rel_equation_engine.h
    rel_equation_engine.cc
    rel_equation_context.h
    rel_equation_context.cc
    rel_executor.h
    rel_executor.cc
    rel_parser.h
    rel_parser.cc
)

add_library(xequation_rel STATIC ${xequation_rel_SRC})

target_link_libraries(xequation_rel PUBLIC rel)          # rel::Value / rel::Parse / rel::Environment
target_link_libraries(xequation_rel PUBLIC xequation_core)

target_include_directories(xequation_rel PUBLIC ../)
