cmake_minimum_required(VERSION 3.15)

project(Template LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(Python REQUIRED COMPONENTS Development Interpreter)

message(STATUS "Python executable: ${Python_EXECUTABLE}")
message(STATUS "Python include dirs: ${Python_INCLUDE_DIRS}")
message(STATUS "Python libraries: ${Python_LIBRARIES}")
find_package(pybind11 REQUIRED)
find_package(fmt REQUIRED)


add_executable(Template main.cc)

target_link_libraries(Template PRIVATE pybind11::embed)