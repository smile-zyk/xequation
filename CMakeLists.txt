cmake_minimum_required(VERSION 3.15)
project(xequation LANGUAGES CXX)

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

if(ENABLE_GUI_SUPPORT)
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
    add_library(qt_config INTERFACE)
    target_link_libraries(qt_config INTERFACE Qt5::Core Qt5::Gui Qt5::Widgets)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

enable_testing()
add_subdirectory(xequation)
add_subdirectory(tests)

add_subdirectory(3rd/qtpropertybrowser)
add_subdirectory(gui)
add_subdirectory(demo)
