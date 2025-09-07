find_package(Python REQUIRED COMPONENTS Development Interpreter)
find_package(pybind11 REQUIRED)

find_package(Boost 1.79.0 REQUIRED COMPONENTS serialization)

message(STATUS "Python executable: ${Python_EXECUTABLE}")
message(STATUS "Python include dirs: ${Python_INCLUDE_DIRS}")
message(STATUS "Python libraries: ${Python_LIBRARIES}")

set(XEXPRENGINE_SRC
    value.cc
    variable_dependency_graph.cc
    expr_context.cc
    expr_engine.cc
    variable_factory.cc
    variable.cc
)

add_library(xexprengine STATIC ${XEXPRENGINE_SRC})

target_include_directories(xexprengine PUBLIC . ${Python_INCLUDE_DIRS})
target_link_libraries(xexprengine PRIVATE pybind11::embed ${Python_LIBRARIES})
target_link_libraries(xexprengine PRIVATE Boost::serialization)

target_include_directories(xexprengine PRIVATE ${Boost_INCLUDE_DIRS})