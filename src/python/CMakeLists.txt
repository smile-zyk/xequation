set(BIN2C_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/bin2c.py)

set(PYTHON_PARSER_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/python_parser.py)
set(PYTHON_PARSER_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PYTHON_PARSER_OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/python_parser_embed.h)

set(xequation_python_SRC
    ${PYTHON_PARSER_OUTPUT}
    python_base.h
    python_executor.h
    python_executor.cc
    python_parser.h
    python_parser.cc
    value_pybind_converter.h
    python_equation_context.h
    python_equation_context.cc
    python_equation_engine.h
    python_equation_engine.cc
)

add_library(xequation_python STATIC ${xequation_python_SRC})

add_custom_target(generate_python_parser ALL
    COMMAND ${Python_EXECUTABLE} ${BIN2C_SCRIPT} ${PYTHON_PARSER_INPUT} 
            -o ${PYTHON_PARSER_OUTPUT_DIR} -v python_parser -n xequation
    DEPENDS ${BIN2C_SCRIPT} ${PYTHON_PARSER_INPUT}
    BYPRODUCTS ${PYTHON_PARSER_OUTPUT}
    COMMENT "Generating python_parser_embed.h from python_parser.py"
    VERBATIM
)

add_dependencies(xequation_python generate_python_parser)

set_target_properties(xequation_python PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_link_libraries(xequation_python PUBLIC pybind11::headers)
target_link_libraries(xequation_python PUBLIC Python::Python)
target_link_libraries(xequation_python PUBLIC xequation_core)

target_include_directories(xequation_python PUBLIC ../)