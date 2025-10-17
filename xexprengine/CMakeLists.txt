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

set(XEXPRENGINE_SRC
    core/value.cc
    core/dependency_graph.cc
    core/variable.cc
    core/event_stamp.cc
    core/variable_manager.cc
    python/value_pybind_converter.cc
    python/py_symbol_extractor.cc
    python/py_expr_context.cc
    python/py_expr_engine.cc
    python/py_restricted_evaluator.cc
)

add_library(xexprengine STATIC ${XEXPRENGINE_SRC} )

set_target_properties(xexprengine PROPERTIES CXX_VISIBILITY_PRESET hidden)

target_link_libraries(xexprengine PUBLIC pybind11::headers)
target_link_libraries(xexprengine PUBLIC Python::Python)
target_link_libraries(xexprengine PUBLIC Boost::multi_index Boost::filesystem)

target_include_directories(xexprengine PUBLIC ./)