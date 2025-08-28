find_package(GTest REQUIRED)

function(add_gtest_executable target_name source_file)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs INCLUDE_DIRS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${target_name} ${source_file})

    if(ARG_INCLUDE_DIRS)
        target_include_directories(${target_name} PRIVATE ${ARG_INCLUDE_DIRS})
    endif()

    target_link_libraries(${target_name} PRIVATE
        xexprengine
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
    )

    gtest_discover_tests(${target_name})

endfunction()

add_gtest_executable(value_test value_test.cc ${CMAKE_SOURCE_DIR}/xexprengine)