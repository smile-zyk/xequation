find_package(GTest REQUIRED)

function(add_gtest_executable target_name test_label source_file)
    set(options "")
    set(oneValueArgs "")
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${target_name} ${source_file})

    set_target_properties(${target_name} PROPERTIES CXX_VISIBILITY_PRESET hidden)

    target_link_libraries(${target_name} PRIVATE
        xequation_core
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
    )

    # Copy the REL runtime DLLs (rel / xdataset) next to the test executable,
    # otherwise ctest cannot find them (MSYS2 reports "error while loading
    # shared libraries").
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rel>"
            "$<TARGET_FILE_DIR:${target_name}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:xdataset>"
            "$<TARGET_FILE_DIR:${target_name}>"
        COMMENT "Copy REL runtime DLLs next to ${target_name}")

    gtest_discover_tests(${target_name}
        PROPERTIES LABELS ${test_label}
        DISCOVERY_TIMEOUT 60
    )

endfunction()

add_gtest_executable(value_test "Value" value_test.cc)
add_gtest_executable(rel_engine_test "RelEngine" rel_engine_test.cc)
add_gtest_executable(rel_repro_test "RelEngineRepro" rel_repro_test.cc)
add_gtest_executable(dependency_graph_test "DependencyGraph" dependency_graph_test.cc)
add_gtest_executable(equation_manager_test "EquationManager" equation_manager_test.cc)
add_gtest_executable(equation_signals_manager_test "EquationSignalsManager" equation_signals_manager_test.cc)
add_gtest_executable(embed_proxy_test "EmbedProxy" embed_proxy_test.cc)

# =========================================================================
#  xdataset unit tests
#
#  3rd/vendor/CMakeLists.txt rewrites the upstream build (it does NOT
#  add_subdirectory 3rd/xdataset), so the upstream xdataset_test target is
#  never created here.  Build the sources directly against our xdataset
#  target so the data-structure layer is covered by this project's CTest too.
# =========================================================================

set(XDATASET_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/xdataset")

add_executable(xdataset_test
    ${XDATASET_TEST_DIR}/tests/measurement_test.cc
    ${XDATASET_TEST_DIR}/tests/unit_test.cc
    ${XDATASET_TEST_DIR}/tests/data_frame_test.cc
    ${XDATASET_TEST_DIR}/tests/data_series_test.cc
    ${XDATASET_TEST_DIR}/tests/block_test.cc
    ${XDATASET_TEST_DIR}/tests/data_array_test.cc
    ${XDATASET_TEST_DIR}/tests/dataset_test.cc
    ${XDATASET_TEST_DIR}/tests/multi_dimension_test.cc
    ${XDATASET_TEST_DIR}/tests/xdataset_predefine_test.cc
)

target_include_directories(xdataset_test PRIVATE
    ${XDATASET_TEST_DIR}/include
    ${XDATASET_TEST_DIR}
)

target_link_libraries(xdataset_test PRIVATE
    xdataset
    GTest::gtest
    GTest::gtest_main
)

# The xdataset DLL must sit next to the test binary (MSYS2 otherwise reports
# "error while loading shared libraries").
add_custom_command(TARGET xdataset_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:xdataset>"
        "$<TARGET_FILE_DIR:xdataset_test>"
    COMMENT "Copy xdataset runtime DLL next to xdataset_test")

gtest_discover_tests(xdataset_test
    PROPERTIES LABELS XDataset
    DISCOVERY_TIMEOUT 60
)