set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(proxy_demo_SRC
proxy_demo_widget.h
proxy_demo_widget.cc
expression_property_widget.h
expression_property_widget.cc
expression_data_frame_model.h
expression_data_frame_model.cc
expression_data_frame_view.h
expression_data_frame_view.cc
expression_data_frame_tab_widget.h
expression_data_frame_tab_widget.cc
main.cc
task/task.h
task/task.cc
task/task_manager.h
task/task_manager.cc
task/toast_task_manager.h
task/toast_task_manager.cc
toast/toast_manager.h
toast/toast_manager.cc
toast/toast_progress_bar.h
toast/toast_progress_bar.cc
)

add_executable(proxy_demo ${proxy_demo_SRC})

target_include_directories(proxy_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(proxy_demo PUBLIC qt_config)
# REL EquationManager 单例由 xequation_core 提供（equation_manager.h 的 RelEquationManager）。
target_link_libraries(proxy_demo PUBLIC xequation_core)

# 嵌入式 Python：宿主（proxy_demo）通过 python_manager 初始化/销毁解释器，
# rel 只对已初始化的解释器执行 env.json 的 python_plugins（BUILD_PYTHON=ON）。
if(BUILD_PYTHON)
  target_link_libraries(proxy_demo PUBLIC python_manager)
  target_compile_definitions(proxy_demo PRIVATE PROXY_HAS_PYTHON)
endif()

if(WIN32)
  get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
  get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
  add_custom_command(TARGET
    proxy_demo POST_BUILD
    COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:proxy_demo>
    COMMENT "run windeployqt")
endif()

# windeployqt only copies Qt DLLs; REL / xdataset runtime DLLs and their
# transitive deps (e.g. HDF5: libhdf5-320.dll -> libaws-*.dll / libsz-2.dll /
# libz-ng2.dll) are resolved and copied by copy_runtime_deps.cmake (excluding
# system and Qt DLLs).
if(WIN32)
  get_filename_component(RUNTIME_DEP_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(RUNTIME_DEP_EXTRA_DIRS "${QT_BIN_DIR};${RUNTIME_DEP_COMPILER_BIN}")
  add_custom_command(TARGET proxy_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DEXE=$<TARGET_FILE:proxy_demo>
        -DDEST=$<TARGET_FILE_DIR:proxy_demo>
        "-DEXTRA_DIRS=${RUNTIME_DEP_EXTRA_DIRS}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/copy_runtime_deps.cmake"
    COMMENT "Copy full runtime dependency DLLs next to proxy_demo")
endif()
