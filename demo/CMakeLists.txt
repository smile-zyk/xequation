set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(demo_SRC
demo_widget.h
demo_widget.cc
property_widget.h
property_widget.cc
data_frame_model.h
data_frame_model.cc
data_frame_view.h
data_frame_view.cc
data_frame_tab_widget.h
data_frame_tab_widget.cc
explorer_view.h
explorer_view.cc
tree_view_tag.h
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

add_executable(demo ${demo_SRC})

target_include_directories(demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(demo PUBLIC qt_config)
# REL EquationManager 单例由 xequation_core 提供（equation_manager.h 的 RelEquationManager）。
target_link_libraries(demo PUBLIC xequation_core)

# 嵌入式 Python：宿主（demo）通过 python_manager 初始化/销毁解释器，
# rel 只对已初始化的解释器执行 env.json 的 python_plugins（BUILD_PYTHON=ON）。
if(BUILD_PYTHON)
  target_link_libraries(demo PUBLIC python_manager)
  target_compile_definitions(demo PRIVATE DEMO_HAS_PYTHON)
endif()

if(WIN32)
  get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
  get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
  add_custom_command(TARGET
    demo POST_BUILD
    COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:demo>
    COMMENT "run windeployqt")
endif()

# windeployqt only copies Qt DLLs; REL / xdataset runtime DLLs and their
# transitive deps (e.g. HDF5: libhdf5-320.dll -> libaws-*.dll / libsz-2.dll /
# libz-ng2.dll) are resolved and copied by copy_runtime_deps.cmake (excluding
# system and Qt DLLs).
if(WIN32)
  get_filename_component(RUNTIME_DEP_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(RUNTIME_DEP_EXTRA_DIRS "${QT_BIN_DIR};${RUNTIME_DEP_COMPILER_BIN}")
  add_custom_command(TARGET demo POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DEXE=$<TARGET_FILE:demo>
        -DDEST=$<TARGET_FILE_DIR:demo>
        "-DEXTRA_DIRS=${RUNTIME_DEP_EXTRA_DIRS}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/copy_runtime_deps.cmake"
    COMMENT "Copy full runtime dependency DLLs next to demo")
endif()
