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
# xdataset 和 REL 以 git submodule 形式引入（3rd/xdataset、3rd/REL），
# 但是【不 invoke 上游的 CMakeLists.txt】—— 上游以独立 project() 组织，
# 会污染导出 target / CTest。这里由 3rd/vendor 重写构建，只构造 xequation
# 需要的 target：xdataset / rel / python_manager。
# BUILD_PYTHON=ON：rel 编译 Python bridge（pybind11::embed），并提供
# python_manager（嵌入式 CPython 环境管理），xequation 的 Python 引擎
# 委托它做初始化/销毁，不再自带环境管理。
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rd/REL/include/value.h")
    message(FATAL_ERROR
        "REL submodule not found at ${CMAKE_CURRENT_SOURCE_DIR}/3rd/REL. "
        "Run 'git submodule update --init' first.")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rd/xdataset/include/data_frame.h")
    message(FATAL_ERROR
        "xdataset submodule not found at ${CMAKE_CURRENT_SOURCE_DIR}/3rd/xdataset. "
        "Run 'git submodule update --init' first.")
endif()
set(BUILD_PYTHON ON CACHE BOOL "" FORCE)
add_subdirectory(3rd/vendor)

add_subdirectory(3rd/qtpropertybrowser)
add_subdirectory(3rd/qcodeeditor)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(demo)
add_subdirectory(proxy_demo)
