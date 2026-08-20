cmake_minimum_required(VERSION 3.16)

project(REL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Test environment: load case/test_env.json at startup ----
add_compile_definitions(REL_LOAD_TEST_ENV)

# Third-party: xdataset (multi-dimensional data structures for REL).
# Enable HDF5 if it's available on the system.
if(DEFINED ENV{HDF5_ROOT} OR EXISTS "C:/msys64/mingw64/bin/libhdf5-320.dll")
    set(XDATASET_ENABLE_HDF5 ON CACHE BOOL "" FORCE)
endif()
# xdataset is a dependency here; its own unit tests should not pollute REL's
# CTest suite (it is pulled in with EXCLUDE_FROM_ALL, so xdataset_test is not
# built and gtest_discover_tests would register a NOT_BUILT stub).
set(BUILD_XDATASET_TEST OFF CACHE BOOL "Build xdataset unit tests" FORCE)
# 当 xequation 作为宿主时，xdataset 由宿主在 3rd/xdataset 直接引入
# （与 REL 同层），这里仅在 xdataset 尚未被引入时才自行构建。
if(NOT TARGET xdataset)
    add_subdirectory(third_party/xdataset EXCLUDE_FROM_ALL)
endif()

# =========================================================================
#  Python plugin (embedded interpreter + pybind11 bindings)
# =========================================================================
#
#  BUILD_PYTHON controls whether the Python bridge is compiled into
#  rel.  On by default; set -DBUILD_PYTHON=OFF for a rel with
#  zero external dependencies (only xdataset).  When ON, it additionally
#  requires pybind11 (and its matching CPython).  numpy interop is implemented
#  via the buffer protocol, so numpy headers are NOT required at build time.
option(BUILD_PYTHON "Enable embedded Python + plugin support" ON)

# ---- Python bridge sources + pybind11 discovery ------------------------
#
#  The Python sources are listed here so they can appear in
#  compile_commands.json (for clangd) even when BUILD_PYTHON=OFF; they are
#  compiled into rel only when BUILD_PYTHON=ON.
set(REL_PYTHON_SOURCES
    src/runtime/python/rel_module.cc
    src/runtime/python/xdataset_bindings.cc
    src/runtime/python/rel_bindings.cc
    src/runtime/python/python_loader.cc
)

# Extra Python sources that live outside rel (the python_manager
# static library owns python_manager.cc).  Listed here only so they appear in
# compile_commands.json (for clangd) when BUILD_PYTHON=OFF.
set(REL_PYTHON_EXTRA_SOURCES
    src/python_manager/python_manager.cc
)

# Discover CPython first (before pybind11) so pybind11 reuses the found
# Python3::Python target instead of running its own discovery.  This is what
# makes `pybind11::embed` link against libpython and expose Python.h.
#   - BUILD_PYTHON=ON  -> REQUIRED (embedding needs the interpreter + libpython)
#   - BUILD_PYTHON=OFF -> QUIET best-effort (only to feed the clangd index below)
if(BUILD_PYTHON)
    find_package(Python3 COMPONENTS Interpreter Development.Embed REQUIRED)
else()
    find_package(Python3 COMPONENTS Interpreter Development.Embed QUIET)
endif()

# Best-effort pybind11 discovery (required only when BUILD_PYTHON=ON).  This
# also locates a pip-installed pybind11 via `python3 -m pybind11 --cmakedir`,
# so BUILD_PYTHON=ON does not need a manually supplied pybind11_DIR.
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

# ---- Report the discovered Python / pybind11 paths -----------------------
message(STATUS "BUILD_PYTHON         = ${BUILD_PYTHON}")
message(STATUS "Python3 executable   = ${Python3_EXECUTABLE}")
message(STATUS "Python3 version      = ${Python3_VERSION}")
message(STATUS "Python3 include dirs = ${Python3_INCLUDE_DIRS}")
message(STATUS "Python3 libraries    = ${Python3_LIBRARIES}")
message(STATUS "Python3 stdlib       = ${Python3_STDLIB}")
message(STATUS "Python3 site-packages= ${Python3_SITELIB}")
message(STATUS "pybind11 found       = ${pybind11_FOUND}")
if(pybind11_FOUND)
    message(STATUS "pybind11_DIR         = ${pybind11_DIR}")
    message(STATUS "pybind11 include dir = ${pybind11_INCLUDE_DIR}")
    message(STATUS "pybind11 version     = ${pybind11_VERSION}")
else()
    message(WARNING "pybind11 NOT found - Python bridge will not be compiled")
endif()

# =========================================================================
#  python_manager (STATIC) — embedded CPython environment management
# =========================================================================
#
#  Owns the *environment* side of the embedded interpreter: configuration
#  (py_home + module search paths), initialization and finalization.  Uses
#  only the CPython C API (no pybind11).  The hosts (rel_cli.exe / rel_test)
#  initialize the interpreter through this library; rel only executes
#  plugins against an already-initialized interpreter.

if(BUILD_PYTHON)
    add_library(python_manager STATIC src/python_manager/python_manager.cc)
    target_include_directories(python_manager PUBLIC src/python_manager)
    target_link_libraries(python_manager PUBLIC Python3::Python)

    # ---- build-time Python paths -----------------------------------------
    #
    #  The hosts configure the embedded interpreter with an explicit py_home
    #  and sys.path so plugins find the stdlib, the C extension modules
    #  (lib-dynload) and pip packages (site-packages) regardless of the
    #  process working directory or PYTHONPATH.  The paths are injected into
    #  python_manager as compile definitions and applied by
    #  PyEnvManager::SetDefaultPyEnvConfig().
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c "import sys; print(sys.base_prefix)"
        OUTPUT_VARIABLE REL_PYTHON_HOME
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(REL_PYTHON_HOME STREQUAL "")
        # Fallback: <prefix>/lib/pythonX.Y -> <prefix>
        get_filename_component(REL_PYTHON_HOME
            "${Python3_STDLIB}/../.." ABSOLUTE)
    endif()
    # Normalize to forward slashes so the paths are safe inside C string
    # literals (a backslash would start an escape sequence).
    string(REPLACE "\\" "/" REL_PYTHON_HOME "${REL_PYTHON_HOME}")
    string(REPLACE "\\" "/" REL_PYTHON_STDLIB "${Python3_STDLIB}")
    string(REPLACE "\\" "/" REL_PYTHON_SITE_PACKAGES "${Python3_SITELIB}")

    target_compile_definitions(python_manager PRIVATE
        REL_PYTHON_HOME="${REL_PYTHON_HOME}"
        REL_PYTHON_STDLIB="${REL_PYTHON_STDLIB}"
        REL_PYTHON_LIB_DYNLOAD="${REL_PYTHON_STDLIB}/lib-dynload"
        REL_PYTHON_SITE_PACKAGES="${REL_PYTHON_SITE_PACKAGES}")

    # ---- deploy the CPython DLL (Windows) --------------------------------
    #
    #  Hosts that link python_manager import libpython directly, so the
    #  matching CPython DLL must sit next to the binary (or be on PATH).
    #  Derive it from the discovered interpreter:
    #    <prefix>/bin/python3.X.exe -> <prefix>/bin/libpython3.X.dll (MinGW)
    #    <prefix>/python.exe        -> <prefix>/python3X.dll    (python.org)
    if(WIN32)
        get_filename_component(_py_bin_dir "${Python3_EXECUTABLE}" DIRECTORY)
        # MinGW layout: <prefix>/bin/libpython3.X.dll
        set(REL_PYTHON_DLL
            "${_py_bin_dir}/libpython${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}.dll")
        if(NOT EXISTS "${REL_PYTHON_DLL}")
            # python.org layout: <prefix>/python3X.dll
            get_filename_component(_py_prefix "${_py_bin_dir}" DIRECTORY)
            set(REL_PYTHON_DLL
                "${_py_prefix}/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll")
        endif()
        # The directory that holds the native dependencies of extension
        # modules (numpy -> libopenblas.dll, ...).  Registered at runtime via
        # AddDllDirectory (Python 3.8+ no longer searches PATH for .pyd deps).
        get_filename_component(REL_PYTHON_DLL_DIR "${REL_PYTHON_DLL}" DIRECTORY)
        string(REPLACE "\\" "/" REL_PYTHON_DLL_DIR "${REL_PYTHON_DLL_DIR}")
        target_compile_definitions(python_manager PRIVATE
            REL_PYTHON_DLL_DIR="${REL_PYTHON_DLL_DIR}")
        if(EXISTS "${REL_PYTHON_DLL}")
            # FindPython3's runtime-library detection can pick the wrong
            # interpreter installation on machines with several Pythons (it
            # resolved to a non-existent python314.dll from a python.org
            # install instead of msys2's libpython3.12.dll).  $<TARGET_RUNTIME_DLLS>
            # reads IMPORTED_LOCATION of the SHARED Python3::Python target
            # (FindPython sets the per-config variants), so point those at
            # the DLL that matches the embedding import library.
            set_target_properties(Python3::Python PROPERTIES
                IMPORTED_LOCATION "${REL_PYTHON_DLL}"
                IMPORTED_LOCATION_RELEASE "${REL_PYTHON_DLL}"
                IMPORTED_LOCATION_DEBUG "${REL_PYTHON_DLL}")
        endif()
    endif()
endif()

# =========================================================================
#  rel (SHARED) — the merged REL engine (front-end + runtime)
# =========================================================================
#
#  One dynamic library hosting the WHOLE engine:
#
#    language front-end (src/core):
#      token/scanner/parser/AST/evaluator + rel.h (Parse / Eval)
#    value layer + runtime services (src/runtime):
#      Value, Environment (datasets/variables/functions/config), operators
#      and math kernels, the "builtin" + "math" function libraries, and the
#      Python bridge (the embedded `rel` module).
#
#  Public API: the flat `include/` folder (rel.h, value.h, environment.h,
#  function.h, expr.h, token.h, error.h, rel_api.h).  Internal headers stay
#  under src/core and src/runtime; their symbols are still exported (REL_API)
#  so hosts (rel_cli.exe) and the test suite can use them, but they are not part
#  of the public header surface.

set(REL_SOURCES
    # ---- language front-end (src/core) ---------------------------------
    src/core/token.cc
    src/core/scanner.cc
    src/core/expr.cc
    src/core/ast_printer.cc
    src/core/parser.cc
    src/core/error.cc
    src/core/evaluator.cc
    src/core/rel.cc
    # ---- value layer + runtime services (src/runtime) ------------------
    src/runtime/value.cc
    src/runtime/function/function.cc
    src/runtime/builtin_library/math_library.cc
    src/runtime/builtin_library/builtin_library.cc
    src/runtime/operation/operator.cc
    src/runtime/operation/pipeline.cc
    src/runtime/operation/operation_helpers.cc
    src/runtime/operation/math_operation.cc
    src/runtime/environment.cc
    src/runtime/environment_config.cc
)

if(BUILD_PYTHON)
    if(NOT pybind11_FOUND)
        message(FATAL_ERROR
            "BUILD_PYTHON=ON requires pybind11. Install it (e.g. pip install "
            "pybind11) or point CMake at it via -Dpybind11_DIR=...")
    endif()
    list(APPEND REL_SOURCES ${REL_PYTHON_SOURCES})
endif()

add_library(rel SHARED ${REL_SOURCES})
target_compile_definitions(rel PRIVATE REL_BUILD_DLL)
target_include_directories(rel PUBLIC
    include      # public API (flat)
    src/core     # internal headers (hosts / tests)
    src/runtime  # internal headers (hosts / tests)
)
target_link_libraries(rel PUBLIC xdataset)

if(BUILD_PYTHON)
    # The whole project is C++11 (see CMAKE_CXX_STANDARD above); the Python
    # bridge stays C++11 too — the buffer-protocol approach avoids the
    # C++14+ pybind11 helpers (no overload_cast / generic lambdas).
    target_compile_definitions(rel PRIVATE REL_HAS_PYTHON)
    target_link_libraries(rel PRIVATE pybind11::embed)
    # pybind11's quiet discovery can fall back to the legacy FindPythonLibs,
    # which sets PYTHON_INCLUDE_DIRS but not Python3_INCLUDE_DIRS — the latter
    # is what pybind11::python_headers expects.  Add the CPython include path
    # explicitly so rel sees Python.h.
    target_include_directories(rel PRIVATE ${Python3_INCLUDE_DIRS})
elseif(pybind11_FOUND)
    # BUILD_PYTHON=OFF: still emit compile_commands.json entries for the
    # Python sources so clangd can index them against pybind11 + CPython
    # headers.  This OBJECT library is never built (EXCLUDE_FROM_ALL), but its
    # compile flags (include paths + REL_HAS_PYTHON) land in
    # compile_commands.json.
    add_library(rel_python_index OBJECT EXCLUDE_FROM_ALL ${REL_PYTHON_SOURCES} ${REL_PYTHON_EXTRA_SOURCES})
    target_compile_definitions(rel_python_index PRIVATE
        REL_HAS_PYTHON REL_BUILD_DLL)
    target_include_directories(rel_python_index PRIVATE
        include src/core src/runtime src/python_manager ${Python3_INCLUDE_DIRS})
    target_link_libraries(rel_python_index PRIVATE xdataset pybind11::headers)
endif()

# Plugin loader needs dlopen on Linux.
if(UNIX AND NOT APPLE)
    target_link_libraries(rel PUBLIC ${CMAKE_DL_LIBS})
endif()

# =========================================================================
#  rel_cli (EXECUTABLE) — CLI driver: REPL + file mode
# =========================================================================

add_executable(rel_cli src/main.cc)
target_link_libraries(rel_cli PRIVATE rel rel_function_library_sample)

if(BUILD_PYTHON)
    # The host owns the embedded interpreter lifecycle (python_manager).
    target_link_libraries(rel_cli PRIVATE python_manager)
    target_compile_definitions(rel_cli PRIVATE REL_HAS_PYTHON)
endif()

# Inject the source root so main.cc can locate case/ (e.g. test_env.json) with
# an absolute path regardless of the working directory.  Forward slashes only,
# so the value is a safe C string literal.
string(REPLACE "\\" "/" REL_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
target_compile_definitions(rel_cli PRIVATE REL_SOURCE_DIR="${REL_SOURCE_DIR}")

# Readline / editline for REPL history (up/down arrow navigation).
if(APPLE)
    find_library(EDIT_LIBRARY edit REQUIRED)
    target_link_libraries(rel_cli PRIVATE ${EDIT_LIBRARY})
elseif(UNIX)
    find_package(Readline REQUIRED)
    target_link_libraries(rel_cli PRIVATE Readline::Readline)
endif()

# Copy xdataset DLL, rel DLL and its dependencies next to rel_cli.exe.
if(WIN32)
    add_custom_command(TARGET rel_cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:xdataset>"
            "$<TARGET_FILE_DIR:rel_cli>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rel>"
            "$<TARGET_FILE_DIR:rel_cli>"
    )
    if(BUILD_PYTHON AND REL_PYTHON_DLL)
        add_custom_command(TARGET rel_cli POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${REL_PYTHON_DLL}"
                "$<TARGET_FILE_DIR:rel_cli>"
        )
    endif()
    if(XDATASET_ENABLE_HDF5 AND HDF5_ROOT)
        add_custom_command(TARGET rel_cli POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HDF5_ROOT}/bin/libhdf5-320.dll"
                "$<TARGET_FILE_DIR:rel_cli>"
        )
    endif()
endif()

# =========================================================================
#  Function library (sample static extension)
# =========================================================================
#
#  A C++ extension is a static library that links rel and exposes a
#  MakeLibrary() factory.  The host (rel_cli.exe / tests) registers it explicitly
#  via Environment::RegisterLibrary.

add_library(rel_function_library_sample STATIC
    src/function_library/function_library_sample.cc
)
target_include_directories(rel_function_library_sample PUBLIC
    src/function_library
    include
)
target_link_libraries(rel_function_library_sample PUBLIC rel)

# ---- Tests ---------------------------------------------------------------
# Single test executable (same pattern as xdataset_test): all test sources are
# compiled into one binary and registered with CTest via gtest_discover_tests.
enable_testing()
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

set(REL_TEST_SOURCES
    tests/test_scanner.cc
    tests/test_parser.cc
    tests/test_environment.cc
    tests/test_evaluator_lit.cc
    tests/test_evaluator_gen.cc
    tests/test_evaluator_op.cc
    tests/test_evaluator_func.cc
    tests/test_builtin_functions.cc
    tests/test_math_operation.cc
    tests/test_function_library.cc
    tests/value_test.cc
    tests/operation_test.cc
)

# Python plugin tests need the embedded interpreter, so they are only built
# when BUILD_PYTHON=ON.
if(BUILD_PYTHON)
    list(APPEND REL_TEST_SOURCES tests/test_python_plugin.cc)
endif()

add_executable(rel_test ${REL_TEST_SOURCES})
target_link_libraries(rel_test PRIVATE rel rel_function_library_sample GTest::gtest_main)

if(BUILD_PYTHON)
    # Tests load plugins through the embedded interpreter, which the test
    # binary initializes itself via python_manager.
    target_link_libraries(rel_test PRIVATE python_manager)
    target_compile_definitions(rel_test PRIVATE REL_HAS_PYTHON)
endif()

# Copy runtime DLLs next to the test binary on Windows so gtest_discover_tests
# can find them during the build-time discovery step.
if(WIN32)
    add_custom_command(TARGET rel_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_RUNTIME_DLLS:rel_test>"
            "$<TARGET_FILE_DIR:rel_test>"
        COMMAND_EXPAND_LISTS
    )
    if(BUILD_PYTHON AND REL_PYTHON_DLL)
        add_custom_command(TARGET rel_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${REL_PYTHON_DLL}"
                "$<TARGET_FILE_DIR:rel_test>"
        )
    endif()
    if(XDATASET_ENABLE_HDF5 AND HDF5_ROOT)
        add_custom_command(TARGET rel_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HDF5_ROOT}/bin/libhdf5-320.dll"
                "$<TARGET_FILE_DIR:rel_test>"
        )
    endif()
endif()

set(_gtest_discovery_timeout 60)
gtest_discover_tests(rel_test DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})

# =========================================================================
#  rel_python_manager_test — lifecycle tests for the python_manager library
# =========================================================================
#
#  Kept as a SEPARATE executable from rel_test: these tests finalize and
#  re-initialize the embedded interpreter, which is incompatible with the
#  pybind11-based plugin tests (embedded modules are registered via
#  PyImport_AppendInittab at static-init time and do not survive
#  Py_Finalize + re-init in the same process).

if(BUILD_PYTHON)
    add_executable(rel_python_manager_test tests/test_python_manager.cc)
    target_link_libraries(rel_python_manager_test PRIVATE
        python_manager GTest::gtest_main)

    if(WIN32)
        add_custom_command(TARGET rel_python_manager_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_RUNTIME_DLLS:rel_python_manager_test>"
                "$<TARGET_FILE_DIR:rel_python_manager_test>"
            COMMAND_EXPAND_LISTS
        )
        if(REL_PYTHON_DLL)
            add_custom_command(TARGET rel_python_manager_test POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${REL_PYTHON_DLL}"
                    "$<TARGET_FILE_DIR:rel_python_manager_test>"
            )
        endif()
    endif()

    gtest_discover_tests(rel_python_manager_test
        DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})
endif()
