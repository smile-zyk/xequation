find_package(Python REQUIRED COMPONENTS Development Interpreter)
find_package(pybind11 REQUIRED)

message(STATUS "Python executable: ${Python_EXECUTABLE}")
message(STATUS "Python include dirs: ${Python_INCLUDE_DIRS}")
message(STATUS "Python libraries: ${Python_LIBRARIES}")

set(XEXPRENGINE_SRC
    exprvalue.cc
)

add_library(xexprengine STATIC ${XEXPRENGINE_SRC})

target_include_directories(xexprengine PUBLIC . ${Python_INCLUDE_DIRS})
target_link_libraries(xexprengine PRIVATE pybind11::embed ${Python_LIBRARIES})