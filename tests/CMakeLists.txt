find_package(GTest REQUIRED)

function(add_gtest_executable target_name test_label source_file)
    set(options "")
    set(oneValueArgs "")
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${target_name} ${source_file})

    set_target_properties(${target_name} PROPERTIES CXX_VISIBILITY_PRESET hidden)

    target_link_libraries(${target_name} PRIVATE
        xequation_core
        xequation_rel
        xequation_python
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
    )

    # REL 运行时 DLL（rel / xdataset）拷贝到测试可执行文件目录，
    # 否则 ctest 运行时找不到（MSYS2 报 "error while loading shared libraries"）。
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
add_gtest_executable(pybind_cast_test "PyObjectConverter" pybind_cast_test.cc)
add_gtest_executable(python_parser_test "PythonParser" python_parser_test.cc)
add_gtest_executable(python_executor_test "PythonExecutor" python_executor_test.cc)
add_gtest_executable(python_equation_engine_test "PythonEquationEngine" python_equation_engine_test.cc)

# ---- embed 门面测试：只链接 xequation_embed 聚合库 -------------------------
# （不链接 xequation_core / xequation_rel / xequation_python，
#   验证宿主仓库"一个库 + 一个头文件"的接入方式。）
add_executable(embed_proxy_test embed_proxy_test.cc)

set_target_properties(embed_proxy_test PROPERTIES CXX_VISIBILITY_PRESET hidden)

target_link_libraries(embed_proxy_test PRIVATE
    xequation_embed
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
)

add_custom_command(TARGET embed_proxy_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:rel>"
        "$<TARGET_FILE_DIR:embed_proxy_test>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:xdataset>"
        "$<TARGET_FILE_DIR:embed_proxy_test>"
    COMMENT "Copy REL runtime DLLs next to embed_proxy_test")

gtest_discover_tests(embed_proxy_test
    PROPERTIES LABELS "EmbedProxy"
    DISCOVERY_TIMEOUT 60
)