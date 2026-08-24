set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(rel_demo_SRC
rel_demo_widget.h
rel_demo_widget.cc
equation_property_window.h
equation_property_window.cc
data_frame_table_model.h
data_frame_table_model.cc
data_frame_table_view.h
data_frame_table_view.cc
main.cc
)

add_executable(rel_demo ${rel_demo_SRC})

target_link_libraries(rel_demo PUBLIC qt_config)
target_link_libraries(rel_demo PUBLIC xequation_core)
target_link_libraries(rel_demo PUBLIC xequation_rel)
target_link_libraries(rel_demo PUBLIC xequation_gui)

if(WIN32)
  get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
  get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
  add_custom_command(TARGET
    rel_demo POST_BUILD
    COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:rel_demo>
    COMMENT "run windeployqt")
endif()

# windeployqt 只拷贝 Qt DLL；REL / xdataset 的运行时 DLL 及其传递依赖
# （如 HDF5：libhdf5-320.dll -> libaws-*.dll / libsz-2.dll / libz-ng2.dll）
# 由 copy_runtime_deps.cmake 自动解析并拷贝（排除系统 DLL 与 Qt DLL）。
if(WIN32)
  get_filename_component(RUNTIME_DEP_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(RUNTIME_DEP_EXTRA_DIRS "${QT_BIN_DIR};${RUNTIME_DEP_COMPILER_BIN}")
  add_custom_command(TARGET rel_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DEXE=$<TARGET_FILE:rel_demo>
        -DDEST=$<TARGET_FILE_DIR:rel_demo>
        "-DEXTRA_DIRS=${RUNTIME_DEP_EXTRA_DIRS}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/copy_runtime_deps.cmake"
    COMMENT "Copy full runtime dependency DLLs next to rel_demo")
endif()
