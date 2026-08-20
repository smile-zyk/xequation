cmake_minimum_required(VERSION 3.15)
project(xequation LANGUAGES CXX)

find_package(Python REQUIRED COMPONENTS Development Interpreter)

message(STATUS "Python executable: ${Python_EXECUTABLE}")
message(STATUS "Python include dirs: ${Python_INCLUDE_DIRS}")
message(STATUS "Python libraries: ${Python_LIBRARIES}")

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
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Svg)
    add_library(qt_config INTERFACE)
    target_link_libraries(qt_config INTERFACE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Svg)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

enable_testing()

# ---- REL runtime (rel::Value) -------------------------------------------
# 集成 smile-zyk/REL 的 rel 库：xequation 只需要 rel::Value。
# xdataset 与 REL 同层（3rd/xdataset），由 xequation 直接引入；
# REL 侧仅在 xdataset target 不存在时才自行构建（见 3rd/REL/CMakeLists.txt）。
# BUILD_PYTHON=ON：REL 编译其 Python bridge（pybind11::embed），并提供
# python_manager（嵌入式 CPython 环境管理），xequation 的 Python 引擎
# 委托它做初始化/销毁，不再自带环境管理。
# rel_cli / rel_test 等目标由 EXCLUDE_FROM_ALL 排除出默认构建。
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rd/REL/CMakeLists.txt")
    message(FATAL_ERROR
        "REL project not found at ${CMAKE_CURRENT_SOURCE_DIR}/3rd/REL. "
        "Expected the REL source tree under 3rd/REL.")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rd/xdataset/CMakeLists.txt")
    message(FATAL_ERROR
        "xdataset project not found at ${CMAKE_CURRENT_SOURCE_DIR}/3rd/xdataset. "
        "Expected the xdataset source tree under 3rd/xdataset.")
endif()
set(BUILD_XDATASET_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(3rd/xdataset XDATASET EXCLUDE_FROM_ALL)
set(BUILD_PYTHON ON CACHE BOOL "" FORCE)
add_subdirectory(3rd/REL REL EXCLUDE_FROM_ALL)

add_subdirectory(3rd/qtpropertybrowser)
add_subdirectory(3rd/qcodeeditor)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(demo)
