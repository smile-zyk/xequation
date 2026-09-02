# =========================================================================
#  3rd/vendor/CMakeLists.txt —— xequation 重写的第三方集成
#
#  我们不直接 add_subdirectory 上游 smile-zyk/REL 与 smile-zyk/xdataset 的
#  CMakeLists.txt（它们以独立 project() 组织，会污染导出的 target / CTest）。
#  相反，这里【重写】它们的构建，只构造 xequation 需要的 target：
#     xdataset        (SHARED) —— REL 的数据结构层
#     rel             (SHARED) —— REL 引擎（前端 + 运行时 + Python bridge）
#     python_manager  (STATIC) —— 嵌入式 CPython 环境管理
#
#  这样上游 submodule 保持干净，其 CMake 改动全部收敛在 xequation 侧。
# =========================================================================

# ---- 第三方依赖发现（原本由上游 CMakeLists 完成） -------------------------
find_package(Eigen3 3.3 CONFIG REQUIRED)
find_package(Boost 1.72 CONFIG REQUIRED)
find_package(tsl-ordered-map CONFIG REQUIRED)

# HDF5：优先 HDF5_ROOT / $ENV{HDF5_ROOT}，否则回退 FindHDF5
if(HDF5_ROOT)
  set(HDF5_ROOT_HINT HINTS "${HDF5_ROOT}")
elseif(DEFINED ENV{HDF5_ROOT})
  set(HDF5_ROOT_HINT HINTS "$ENV{HDF5_ROOT}")
elseif(APPLE)
  set(HDF5_ROOT_HINT HINTS "/opt/homebrew/opt/hdf5" "/usr/local/opt/hdf5")
endif()
find_package(HDF5 REQUIRED COMPONENTS C)
set(HDF5_TARGET hdf5::hdf5)
message(STATUS "HDF5: found at ${HDF5_ROOT} (version ${HDF5_VERSION})")

# ---- Python / pybind11（Python bridge + python_manager 需要） ----------------
# BUILD_PYTHON=ON（根 CMakeLists 的 option，默认开）时编译 python_manager
# （STATIC）与 rel 的 Python bridge：需要 Python3 的 Interpreter +
# Development.Embed 组件（嵌入需要 libpython）与 pybind11（embed）。
# pybind11 优先 CONFIG 包；找不到时回退 `python -m pybind11 --cmakedir`
# （pip 安装的 pybind11 自带 CMake 配置）。
if(BUILD_PYTHON)
  find_package(Python3 COMPONENTS Interpreter Development.Embed REQUIRED)
else()
  find_package(Python3 COMPONENTS Interpreter Development.Embed QUIET)
endif()

find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND AND Python3_Interpreter_FOUND)
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
    OUTPUT_VARIABLE _pybind11_cmakedir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(_pybind11_cmakedir)
    set(pybind11_DIR "${_pybind11_cmakedir}")
    find_package(pybind11 CONFIG QUIET)
  endif()
endif()
if(BUILD_PYTHON AND NOT pybind11_FOUND)
  message(FATAL_ERROR
    "BUILD_PYTHON=ON requires pybind11. Install it (e.g. pip install pybind11 "
    "or pacman -S mingw-w64-x86_64-pybind11).")
endif()
message(STATUS "BUILD_PYTHON         = ${BUILD_PYTHON}")
if(Python3_FOUND)
  message(STATUS "Python3 executable   = ${Python3_EXECUTABLE}")
  message(STATUS "Python3 version      = ${Python3_VERSION}")
endif()
message(STATUS "pybind11 found       = ${pybind11_FOUND}")

set(XDATASET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../xdataset")
set(REL_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/../REL")

# =========================================================================
#  xdataset (SHARED)
# =========================================================================
set(XDATASET_SOURCES
  ${XDATASET_DIR}/src/data_shape.cc
  ${XDATASET_DIR}/src/dimension_spec.cc
  ${XDATASET_DIR}/src/multi_dimension_spec.cc
  ${XDATASET_DIR}/src/multi_index_selector.cc
  ${XDATASET_DIR}/src/data_array.cc
  ${XDATASET_DIR}/src/block.cc
  ${XDATASET_DIR}/src/data_frame.cc
  ${XDATASET_DIR}/src/data_series.cc
  ${XDATASET_DIR}/src/measurement.cc
  ${XDATASET_DIR}/src/unit.cc
  ${XDATASET_DIR}/src/unit_registry.cc
  ${XDATASET_DIR}/src/dataset.cc
  ${XDATASET_DIR}/src/hdf5_io.cc
  ${XDATASET_DIR}/src/touchstone_io.cc
  ${XDATASET_DIR}/src/data_array_io.cc
)

add_library(xdataset SHARED ${XDATASET_SOURCES})
target_compile_definitions(xdataset PRIVATE XDATASET_BUILD_DLL)
# GCC 15 需要 -O1 让 Eigen 头文件模板内联（参考上游注释）
target_compile_options(xdataset PRIVATE $<$<CXX_COMPILER_ID:GNU>:$<$<CONFIG:Debug>:-O1>>)

target_include_directories(xdataset PUBLIC
  ${XDATASET_DIR}/include
  ${XDATASET_DIR}
)
target_link_libraries(xdataset PUBLIC
  Eigen3::Eigen
  Boost::headers
  tsl::ordered_map
  ${HDF5_TARGET}
)

# =========================================================================
#  python_manager (STATIC) —— 嵌入式 CPython 环境管理
# =========================================================================
if(BUILD_PYTHON)
  add_library(python_manager STATIC ${REL_DIR}/src/python_manager/python_manager.cc)
  target_include_directories(python_manager PUBLIC ${REL_DIR}/src/python_manager)
  target_link_libraries(python_manager PUBLIC Python3::Python)

  # ---- build-time Python paths -------------------------------------------
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import sys; print(sys.base_prefix)"
    OUTPUT_VARIABLE REL_PYTHON_HOME
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(REL_PYTHON_HOME STREQUAL "")
    get_filename_component(REL_PYTHON_HOME "${Python3_STDLIB}/../.." ABSOLUTE)
  endif()
  string(REPLACE "\\" "/" REL_PYTHON_HOME     "${REL_PYTHON_HOME}")
  string(REPLACE "\\" "/" REL_PYTHON_STDLIB   "${Python3_STDLIB}")
  string(REPLACE "\\" "/" REL_PYTHON_SITELIB  "${Python3_SITELIB}")

  target_compile_definitions(python_manager PRIVATE
    REL_PYTHON_HOME="${REL_PYTHON_HOME}"
    REL_PYTHON_STDLIB="${REL_PYTHON_STDLIB}"
    REL_PYTHON_LIB_DYNLOAD="${REL_PYTHON_STDLIB}/lib-dynload"
    REL_PYTHON_SITE_PACKAGES="${REL_PYTHON_SITELIB}")

  # Windows: 部署匹配的 CPython DLL
  if(WIN32)
    get_filename_component(_py_bin_dir "${Python3_EXECUTABLE}" DIRECTORY)
    set(REL_PYTHON_DLL
      "${_py_bin_dir}/libpython${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}.dll")
    if(NOT EXISTS "${REL_PYTHON_DLL}")
      set(REL_PYTHON_DLL
        "${_py_bin_dir}/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll")
    endif()
    if(NOT EXISTS "${REL_PYTHON_DLL}")
      get_filename_component(_py_prefix "${_py_bin_dir}" DIRECTORY)
      set(REL_PYTHON_DLL
        "${_py_prefix}/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll")
    endif()
    get_filename_component(REL_PYTHON_DLL_DIR "${REL_PYTHON_DLL}" DIRECTORY)
    string(REPLACE "\\" "/" REL_PYTHON_DLL_DIR "${REL_PYTHON_DLL_DIR}")
    target_compile_definitions(python_manager PRIVATE
      REL_PYTHON_DLL_DIR="${REL_PYTHON_DLL_DIR}")
    if(EXISTS "${REL_PYTHON_DLL}")
      set_target_properties(Python3::Python PROPERTIES
        IMPORTED_LOCATION "${REL_PYTHON_DLL}"
        IMPORTED_LOCATION_RELEASE "${REL_PYTHON_DLL}"
        IMPORTED_LOCATION_DEBUG "${REL_PYTHON_DLL}")
    endif()
  endif()
endif()

# =========================================================================
#  rel (SHARED) —— 合并的 REL 引擎（front-end + runtime）
# =========================================================================
set(REL_SOURCES
  ${REL_DIR}/src/core/token.cc
  ${REL_DIR}/src/core/scanner.cc
  ${REL_DIR}/src/core/expr.cc
  ${REL_DIR}/src/core/ast_printer.cc
  ${REL_DIR}/src/core/parser.cc
  ${REL_DIR}/src/core/error.cc
  ${REL_DIR}/src/core/evaluator.cc
  ${REL_DIR}/src/core/rel.cc
  ${REL_DIR}/src/runtime/value.cc
  ${REL_DIR}/src/runtime/function/function.cc
  ${REL_DIR}/src/runtime/builtin_library/math_library.cc
  ${REL_DIR}/src/runtime/builtin_library/builtin_library.cc
  ${REL_DIR}/src/runtime/operation/operator.cc
  ${REL_DIR}/src/runtime/operation/pipeline.cc
  ${REL_DIR}/src/runtime/operation/operation_helpers.cc
  ${REL_DIR}/src/runtime/operation/math_operation.cc
  ${REL_DIR}/src/runtime/environment.cc
  ${REL_DIR}/src/runtime/environment_config.cc
)

if(BUILD_PYTHON)
  list(APPEND REL_SOURCES
    ${REL_DIR}/src/runtime/python/rel_module.cc
    ${REL_DIR}/src/runtime/python/xdataset_bindings.cc
    ${REL_DIR}/src/runtime/python/rel_bindings.cc
    ${REL_DIR}/src/runtime/python/python_loader.cc
  )
endif()

add_library(rel SHARED ${REL_SOURCES})
target_compile_definitions(rel PRIVATE REL_BUILD_DLL)
target_include_directories(rel PUBLIC
  ${REL_DIR}/include     # public API (flat)
  ${REL_DIR}/src/core    # internal headers (hosts / tests)
  ${REL_DIR}/src/runtime # internal headers (hosts / tests)
)
target_link_libraries(rel PUBLIC xdataset)

if(BUILD_PYTHON)
  if(NOT pybind11_FOUND)
    message(FATAL_ERROR
      "BUILD_PYTHON=ON requires pybind11. Install it (e.g. pip install pybind11)")
  endif()
  # 整个工程是 C++11（见根 CMakeLists），Python bridge 保持 C++11
  target_compile_definitions(rel PRIVATE REL_HAS_PYTHON)
  target_link_libraries(rel PRIVATE pybind11::embed)
  target_include_directories(rel PRIVATE ${Python3_INCLUDE_DIRS})
endif()

# Plugin loader needs dlopen on Linux.
if(UNIX AND NOT APPLE)
  target_link_libraries(rel PUBLIC ${CMAKE_DL_LIBS})
endif()
