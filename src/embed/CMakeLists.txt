# =============================================================================
#  xequation_embed -- 宿主仓库嵌入用的聚合静态库（无 GUI）
# =============================================================================
#
#  用途：
#    - 宿主仓库只 add_subdirectory 本目录、只链接 xequation_embed 一个目标、
#      只 include <embed/xequation_embed.h>，即可获得 Python + REL 两个
#      EquationManager 的全部能力（解析 / 计算 / 编辑 / 信号通知）。
#    - 本目录直接编译 core / rel_engine / python 三个子模块的源码，
#      不依赖 src/gui，也不依赖本仓库顶层 CMakeLists。
#
#  前置条件（调用方需已提供，与 xequation 顶层构建一致）：
#    - rel、xdataset、python_manager 目标（add_subdirectory(3rd/REL) 与
#      add_subdirectory(3rd/xdataset) 提供，注意顺序：xdataset 先于 REL）
#    - find_package(Python REQUIRED COMPONENTS Development Interpreter)
#    - find_package(pybind11 REQUIRED)
#    - find_package(Boost REQUIRED COMPONENTS multi_index uuid compute variant)
#    - tsl::ordered_map（find_path(TSL_ORDERED_MAP_INCLUDE_DIRS "tsl/ordered_hash.h")）
#
#  运行时部署：
#    - rel_runtime / xdataset 是 SHARED 库，需部署到宿主可执行文件同目录
#      （参考 rel_demo/copy_runtime_deps.cmake）。
# =============================================================================

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(xequation_embed_SRC
    xequation_embed.cc

    # core
    ../core/equation_value.cc
    ../core/dependency_graph.cc
    ../core/equation.cc
    ../core/equation_group.cc
    ../core/equation_manager.cc

    # rel_engine
    ../rel_engine/rel_parser.cc
    ../rel_engine/rel_executor.cc
    ../rel_engine/rel_equation_context.cc
    ../rel_engine/rel_equation_engine.cc

    # python
    ../python/python_executor.cc
    ../python/python_parser.cc
    ../python/python_equation_context.cc
    ../python/python_equation_engine.cc
)

add_library(xequation_embed STATIC ${xequation_embed_SRC})

set_target_properties(xequation_embed PROPERTIES
    CXX_VISIBILITY_PRESET hidden
)

target_include_directories(xequation_embed PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..   # 包含根：core/xxx.h、python/xxx.h、rel_engine/xxx.h
    ${CMAKE_CURRENT_SOURCE_DIR}      # embed 自身
    ${TSL_ORDERED_MAP_INCLUDE_DIRS}
)

target_link_libraries(xequation_embed PUBLIC rel)
target_link_libraries(xequation_embed PUBLIC Boost::multi_index)
target_link_libraries(xequation_embed PUBLIC Boost::uuid)
target_link_libraries(xequation_embed PUBLIC Boost::compute)
target_link_libraries(xequation_embed PUBLIC Boost::variant)
target_link_libraries(xequation_embed PUBLIC pybind11::headers)
target_link_libraries(xequation_embed PUBLIC Python::Python)
# Python 环境管理/初始化委托给 REL 的 python_manager（幂等，仅宿主初始化）
target_link_libraries(xequation_embed PUBLIC python_manager)
