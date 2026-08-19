set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(xequation_demo_SRC
demo_widget.h
demo_widget.cc
mock_equation_group_list_widget.h
mock_equation_group_list_widget.cc
message_widget.h
message_widget.cc
main.cc
)

add_executable(xequation_demo ${xequation_demo_SRC})

target_link_libraries(xequation_demo PUBLIC qt_config)
target_link_libraries(xequation_demo PUBLIC xequation_core)
target_link_libraries(xequation_demo PUBLIC xequation_python)
target_link_libraries(xequation_demo PUBLIC xequation_gui)

if(WIN32)
  get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
  get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
  add_custom_command(TARGET
    xequation_demo POST_BUILD
    COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:xequation_demo>
    COMMENT "run windeployqt")
endif()

# windeployqt 只拷贝 Qt DLL；REL 的运行时 DLL（rel_runtime / xdataset）
# 需要显式拷贝到可执行文件目录，否则运行时找不到（MSYS2 报
# "error while loading shared libraries: libxdataset.dll"）。
add_custom_command(TARGET xequation_demo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:rel_runtime>"
      "$<TARGET_FILE_DIR:xequation_demo>"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:xdataset>"
      "$<TARGET_FILE_DIR:xequation_demo>"
  COMMENT "Copy REL runtime DLLs next to xequation_demo")