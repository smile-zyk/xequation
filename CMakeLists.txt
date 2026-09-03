cmake_minimum_required(VERSION 3.15)
project(xequation LANGUAGES CXX)

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
# 需要的 target：xdataset / rel。
# Python bridge 可选：BUILD_PYTHON=ON（默认开）时 rel 编译嵌入式 CPython
# 绑定（pybind11::embed + python_loader），env.json 的 python_plugins 才可加载；
# 宿主（demo）通过 python_manager 初始化/销毁解释器。
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
option(BUILD_PYTHON "Enable embedded Python + plugin support (env.json python_plugins)" ON)
add_subdirectory(3rd/vendor)

add_subdirectory(src)
add_subdirectory(tests)
if(ENABLE_GUI_SUPPORT)
    add_subdirectory(demo)
endif()
