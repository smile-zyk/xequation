find_package(GTest REQUIRED)

function(add_gtest_executable target_name source_file)
    set(options "")
    set(oneValueArgs "")
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${target_name} ${source_file})


    target_link_libraries(${target_name} PRIVATE
        xexprengine
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
    )

    gtest_discover_tests(${target_name})

endfunction()

add_gtest_executable(value_test value_test.cc)
add_gtest_executable(dependency_graph_test dependency_graph_test.cc)
add_gtest_executable(expr_context_test expr_context_test.cc)