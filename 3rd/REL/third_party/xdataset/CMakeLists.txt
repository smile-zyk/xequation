cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(xdataset)

option(BUILD_XDATASET_TEST "Build xdataset unit tests" ON)

find_package(Eigen3 3.3 CONFIG REQUIRED)
find_package(Boost 1.72 CONFIG REQUIRED)

# HDF5: prefer Homebrew/system HDF5 via HDF5_ROOT hint, fall back to CMake's FindHDF5
if(HDF5_ROOT)
    set(HDF5_ROOT_HINT HINTS "${HDF5_ROOT}")
elseif(DEFINED ENV{HDF5_ROOT})
    set(HDF5_ROOT_HINT HINTS "$ENV{HDF5_ROOT}")
elseif(APPLE)
    # Auto-detect Homebrew HDF5 on macOS
    set(HDF5_ROOT_HINT HINTS "/opt/homebrew/opt/hdf5" "/usr/local/opt/hdf5")
endif()

find_package(HDF5 REQUIRED COMPONENTS C)
set(HDF5_TARGET hdf5::hdf5)
message(STATUS "HDF5: found at ${HDF5_ROOT} (version ${HDF5_VERSION})")

set(XDATASET_SOURCES
	src/data_shape.cc
	src/dimension_spec.cc
	src/multi_dimension_spec.cc
	src/multi_index_selector.cc
	src/data_array.cc
	src/block.cc
	src/data_frame.cc
	src/data_series.cc
	src/measurement.cc
	src/unit.cc
	src/unit_registry.cc
	src/dataset.cc
	src/hdf5_io.cc
	src/touchstone_io.cc
	src/data_array_io.cc
)

# -- xdataset shared lib ---------------------------------------------------

add_library(xdataset SHARED ${XDATASET_SOURCES})
target_compile_definitions(xdataset PRIVATE XDATASET_BUILD_DLL)

# GCC 15 needs -O1 for Eigen header-only template inlining, regardless of Eigen version
target_compile_options(xdataset PRIVATE $<$<CXX_COMPILER_ID:GNU>:$<$<CONFIG:Debug>:-O1>>)

if(BUILD_XDATASET_TEST)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
    enable_testing()

    # -- full xdataset test -------------------------------------------------

    add_executable(xdataset_test
        tests/data_series_test.cc
        tests/measurement_test.cc
        tests/multi_dimension_test.cc
        tests/block_test.cc
        tests/data_array_test.cc
        tests/data_frame_test.cc
        tests/dataset_test.cc
        tests/hdf5_io_test.cc
        tests/touchstone_io_test.cc
        tests/unit_test.cc
    )

    target_include_directories(xdataset_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(xdataset_test PRIVATE xdataset GTest::gtest_main)
    gtest_discover_tests(xdataset_test)
endif()

add_executable(xdataset_playground
    apps/xdataset_playground/main.cc
)

target_include_directories(xdataset PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(xdataset_playground PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(xdataset PUBLIC Eigen3::Eigen Boost::headers ${HDF5_TARGET})
target_link_libraries(xdataset_playground PRIVATE xdataset)